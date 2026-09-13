#include "NWPremiumGameplayDirector.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NWAbilityFeedbackActor.h"
#include "NWActionReticleWidget.h"
#include "NWCharacter.h"
#include "NWCombatHUDWidget.h"
#include "NWCombatLibrary.h"
#include "NWEnemy.h"
#include "NWEnemyVisualDirector.h"
#include "NWPremiumEnvironmentDirector.h"
#include "NWProceduralWorldManager.h"

ANWPremiumGameplayDirector::ANWPremiumGameplayDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;
    bReplicates = true;
    SetReplicateMovement(false);
    SetNetUpdateFrequency(1.0f);
}

void ANWPremiumGameplayDirector::BeginPlay()
{
    Super::BeginPlay();
    EnsurePremiumWorldSystems();
    UE_LOG(LogTemp, Warning, TEXT("[PREMIUM-V2] action RPG ativo: FREE AIM por camera + soft aim opcional + reticulo + ambiente real + mobs visiveis."));
}

void ANWPremiumGameplayDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()) { return; }

    EnsurePremiumWorldSystems();
    EnsureLocalHUDs(DeltaSeconds);

    if (HasAuthority())
    {
        EnsureTrainingEncounter(DeltaSeconds);
        DetectAbilityCasts();
        ResolvePendingAssists();
    }
}

void ANWPremiumGameplayDirector::EnsurePremiumWorldSystems()
{
    if (bPremiumWorldSystemsSpawned || !GetWorld() || GetNetMode() == NM_DedicatedServer) { return; }

    bool bHasEnvironment = false;
    for (TActorIterator<ANWPremiumEnvironmentDirector> It(GetWorld()); It; ++It)
    {
        bHasEnvironment = true;
        break;
    }
    if (!bHasEnvironment)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.ObjectFlags |= RF_Transient;
        GetWorld()->SpawnActor<ANWPremiumEnvironmentDirector>(ANWPremiumEnvironmentDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    bool bHasEnemyVisuals = false;
    for (TActorIterator<ANWEnemyVisualDirector> It(GetWorld()); It; ++It)
    {
        bHasEnemyVisuals = true;
        break;
    }
    if (!bHasEnemyVisuals)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.ObjectFlags |= RF_Transient;
        GetWorld()->SpawnActor<ANWEnemyVisualDirector>(ANWEnemyVisualDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    bPremiumWorldSystemsSpawned = true;
}

void ANWPremiumGameplayDirector::EnsureLocalHUDs(float DeltaSeconds)
{
    HudAccumulator += DeltaSeconds;
    if (HudAccumulator < HudEnsureInterval || !GetWorld()) { return; }
    HudAccumulator = 0.0f;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }

        ANWCharacter* Character = Cast<ANWCharacter>(PC->GetPawn());
        if (!Character) { continue; }

        TArray<UUserWidget*> ExistingHUDs;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, ExistingHUDs, UNWCombatHUDWidget::StaticClass(), false);
        if (ExistingHUDs.IsEmpty())
        {
            UNWCombatHUDWidget* HUD = CreateWidget<UNWCombatHUDWidget>(PC, UNWCombatHUDWidget::StaticClass());
            if (HUD)
            {
                HUD->SetObservedCharacter(Character);
                HUD->AddToViewport(30);
                HUD->SetVisibility(ESlateVisibility::HitTestInvisible);
                UE_LOG(LogTemp, Display, TEXT("[HUD] CombatHUD garantido para %s."), *Character->GetName());
            }
        }

        TArray<UUserWidget*> ExistingReticles;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, ExistingReticles, UNWActionReticleWidget::StaticClass(), false);
        if (ExistingReticles.IsEmpty())
        {
            UNWActionReticleWidget* Reticle = CreateWidget<UNWActionReticleWidget>(PC, UNWActionReticleWidget::StaticClass());
            if (Reticle)
            {
                Reticle->AddToViewport(100);
                Reticle->SetVisibility(ESlateVisibility::HitTestInvisible);
                UE_LOG(LogTemp, Display, TEXT("[RETICLE] crosshair premium garantido para %s."), *Character->GetName());
            }
        }
    }
}

void ANWPremiumGameplayDirector::EnsureTrainingEncounter(float DeltaSeconds)
{
    if (bTrainingEncounterReady || !HasAuthority() || !GetWorld()) { return; }

    TrainingAccumulator += DeltaSeconds;
    if (TrainingAccumulator < 1.25f) { return; }
    TrainingAccumulator = 0.0f;

    ANWCharacter* Character = nullptr;
    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            Character = *It;
            break;
        }
    }
    if (!Character) { return; }

    int32 Nearby = 0;
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy) || Enemy->IsWorldBoss()) { continue; }
        if (FVector::Dist2D(Character->GetActorLocation(), Enemy->GetActorLocation()) <= TrainingEncounterRadius)
        {
            ++Nearby;
        }
    }

    const int32 Needed = FMath::Max(0, DesiredNearbyTrainingEnemies - Nearby);
    if (Needed <= 0)
    {
        bTrainingEncounterReady = true;
        UE_LOG(LogTemp, Warning, TEXT("[ENCOUNTER] pronto: %d mobs ja estavam dentro de %.0f cm do jogador."), Nearby, TrainingEncounterRadius);
        return;
    }

    ANWProceduralWorldManager* WorldManager = nullptr;
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        WorldManager = *It;
        break;
    }

    FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
    if (Forward.IsNearlyZero()) { Forward = FVector::ForwardVector; }
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal2D();

    struct FSpawnSpec
    {
        ENWEnemyArchetype Archetype;
        float ForwardDistance;
        float SideDistance;
    };

    static const FSpawnSpec Specs[] = {
        { ENWEnemyArchetype::Zombie, 900.0f, -420.0f },
        { ENWEnemyArchetype::Ghost, 1050.0f, 360.0f },
        { ENWEnemyArchetype::Brute, 1225.0f, 0.0f },
        { ENWEnemyArchetype::Zombie, 1425.0f, -620.0f },
        { ENWEnemyArchetype::Ghost, 1575.0f, 590.0f }
    };

    int32 Spawned = 0;
    for (const FSpawnSpec& Spec : Specs)
    {
        if (Spawned >= Needed) { break; }

        FVector Location = Character->GetActorLocation() + Forward * Spec.ForwardDistance + Right * Spec.SideDistance;
        Location.Z = WorldManager
            ? WorldManager->GetTerrainHeightAt(Location.X, Location.Y) + 125.0f
            : Character->GetActorLocation().Z + 20.0f;

        FVector ToPlayer = Character->GetActorLocation() - Location;
        ToPlayer.Z = 0.0f;
        const FRotator Rotation = ToPlayer.IsNearlyZero() ? Character->GetActorRotation() : ToPlayer.Rotation();
        const FTransform SpawnTransform(Rotation, Location);

        ANWEnemy* Enemy = GetWorld()->SpawnActorDeferred<ANWEnemy>(
            ANWEnemy::StaticClass(), SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Enemy) { continue; }

        Enemy->ConfigureEnemy(Spec.Archetype, false, 1);
        UGameplayStatics::FinishSpawningActor(Enemy, SpawnTransform);
        ++Spawned;
    }

    bTrainingEncounterReady = true;
    UE_LOG(LogTemp, Warning, TEXT("[ENCOUNTER] arena de teste pronta: existentes=%d novos=%d total-alvo=%d | mobs posicionados no arco da mira."),
        Nearby, Spawned, DesiredNearbyTrainingEnemies);
}

void ANWPremiumGameplayDirector::DetectAbilityCasts()
{
    if (!GetWorld()) { return; }

    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character)) { continue; }

        FPlayerCooldownState& State = PlayerStates.FindOrAdd(Character);
        const ENWWeaponType Weapon = Character->GetActiveWeapon();

        if (!State.bInitialized || State.Weapon != Weapon || State.Cooldowns.Num() != 3)
        {
            State.Weapon = Weapon;
            State.Cooldowns.SetNumZeroed(3);
            for (int32 AbilityIndex = 0; AbilityIndex < 3; ++AbilityIndex)
            {
                State.Cooldowns[AbilityIndex] = Character->GetAbilityCooldownRemaining(AbilityIndex);
            }
            State.bInitialized = true;
            continue;
        }

        const FNWWeaponDefinition Definition = NWCombat::GetWeaponDefinition(Weapon);
        for (int32 AbilityIndex = 0; AbilityIndex < 3; ++AbilityIndex)
        {
            const float CurrentCooldown = Character->GetAbilityCooldownRemaining(AbilityIndex);
            const float PreviousCooldown = State.Cooldowns[AbilityIndex];

            if (CurrentCooldown > PreviousCooldown + 0.30f && CurrentCooldown > 0.40f && Definition.Abilities.IsValidIndex(AbilityIndex))
            {
                const FNWWeaponAbilityDefinition& Ability = Definition.Abilities[AbilityIndex];
                UE_LOG(LogTemp, Display, TEXT("[ABILITY-CHECK] %s/%s aceito | cooldown %.2fs | FREE AIM"),
                    *Definition.Name, *Ability.Name, CurrentCooldown);

                if (Ability.Kind == ENWAbilityKind::Heal)
                {
                    FVector AimDirection = Character->GetBaseAimRotation().Vector().GetSafeNormal();
                    SpawnCastFeedback(Character, Weapon, Ability, AbilityIndex, Character->GetActorLocation() + FVector(0.0f, 0.0f, 75.0f));
                }
                else
                {
                    QueueAbilityAssist(Character, Weapon, AbilityIndex);
                }
            }

            State.Cooldowns[AbilityIndex] = CurrentCooldown;
        }
    }
}

void ANWPremiumGameplayDirector::QueueAbilityAssist(ANWCharacter* Character, ENWWeaponType Weapon, int32 AbilityIndex)
{
    if (!Character || !GetWorld()) { return; }

    const FNWWeaponDefinition Definition = NWCombat::GetWeaponDefinition(Weapon);
    if (!Definition.Abilities.IsValidIndex(AbilityIndex)) { return; }
    const FNWWeaponAbilityDefinition& Ability = Definition.Abilities[AbilityIndex];

    FPendingAbilityAssist Pending;
    Pending.Character = Character;
    Pending.Weapon = Weapon;
    Pending.AbilityIndex = AbilityIndex;
    Pending.ResolveAt = GetWorld()->GetTimeSeconds() + AbilityVerificationDelay;
    Pending.AimPoint = ResolveFreeAimPoint(Character, FreeAimTraceRange, Pending.AimDirection);

    const float NativeRange = FMath::Max(Ability.Range, 320.0f);
    const float ObservationRange = NativeRange + FMath::Max(Ability.Radius, SoftAimRadius) + 360.0f;
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }
        if (FVector::Dist2D(Character->GetActorLocation(), Enemy->GetActorLocation()) > ObservationRange) { continue; }
        Pending.ObservedEnemies.Add(Enemy);
        Pending.HealthRatiosAtCast.Add(Enemy->GetHealthRatio());
    }

    FVector FaceDirection = Pending.AimPoint - Character->GetActorLocation();
    FaceDirection.Z = 0.0f;
    if (!FaceDirection.IsNearlyZero())
    {
        const FRotator Current = Character->GetActorRotation();
        Character->SetActorRotation(FRotator(Current.Pitch, FaceDirection.Rotation().Yaw, Current.Roll));
    }

    const FVector VisualPoint = ClampAimPointToAbilityRange(
        Character,
        Pending.AimPoint,
        FMath::Max(Ability.Range, 320.0f),
        !IsRangedWeapon(Weapon));
    SpawnCastFeedback(Character, Weapon, Ability, AbilityIndex, VisualPoint);

    PendingAssists.Add(MoveTemp(Pending));
}

void ANWPremiumGameplayDirector::SpawnCastFeedback(
    ANWCharacter* Character,
    ENWWeaponType Weapon,
    const FNWWeaponAbilityDefinition& Ability,
    int32 AbilityIndex,
    const FVector& AimPoint)
{
    if (!Character || !GetWorld() || GetNetMode() == NM_DedicatedServer) { return; }

    FLinearColor Color(0.30f, 0.58f, 1.0f, 1.0f);
    switch (Weapon)
    {
        case ENWWeaponType::Staff: Color = FLinearColor(0.25f, 0.45f, 1.0f, 1.0f); break;
        case ENWWeaponType::Greatsword: Color = FLinearColor(1.0f, 0.48f, 0.12f, 1.0f); break;
        case ENWWeaponType::DualSwords: Color = FLinearColor(0.90f, 0.22f, 0.18f, 1.0f); break;
        case ENWWeaponType::SwordShield: Color = FLinearColor(1.0f, 0.76f, 0.18f, 1.0f); break;
        case ENWWeaponType::Daggers: Color = FLinearColor(0.45f, 1.0f, 0.30f, 1.0f); break;
        case ENWWeaponType::Bow: Color = FLinearColor(0.20f, 0.90f, 0.45f, 1.0f); break;
        case ENWWeaponType::Firearm: Color = FLinearColor(1.0f, 0.68f, 0.25f, 1.0f); break;
        default: break;
    }
    if (Ability.Kind == ENWAbilityKind::Heal)
    {
        Color = FLinearColor(0.25f, 1.0f, 0.52f, 1.0f);
    }

    const float Radius = Ability.Kind == ENWAbilityKind::AreaDamage
        ? FMath::Max(160.0f, Ability.Radius)
        : (Ability.Kind == ENWAbilityKind::Heal ? 170.0f : 105.0f);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    ANWAbilityFeedbackActor* Feedback = GetWorld()->SpawnActor<ANWAbilityFeedbackActor>(
        ANWAbilityFeedbackActor::StaticClass(), AimPoint, FRotator::ZeroRotator, Params);
    if (Feedback)
    {
        Feedback->InitializeFeedback(Color, Radius, Ability.Kind == ENWAbilityKind::AreaDamage ? 0.34f : 0.24f);
    }

    UE_LOG(LogTemp, Display, TEXT("[ABILITY-FREECAST] slot=%d point=(%.0f,%.0f,%.0f) raio=%.0f"),
        AbilityIndex + 1, AimPoint.X, AimPoint.Y, AimPoint.Z, Radius);
}

void ANWPremiumGameplayDirector::ResolvePendingAssists()
{
    if (!GetWorld() || PendingAssists.IsEmpty()) { return; }
    const float Now = GetWorld()->GetTimeSeconds();

    for (int32 Index = PendingAssists.Num() - 1; Index >= 0; --Index)
    {
        const FPendingAbilityAssist& Pending = PendingAssists[Index];
        if (Now < Pending.ResolveAt) { continue; }

        ANWCharacter* Character = Pending.Character.Get();
        if (!IsValid(Character))
        {
            PendingAssists.RemoveAtSwap(Index);
            continue;
        }

        const FNWWeaponDefinition Definition = NWCombat::GetWeaponDefinition(Pending.Weapon);
        const FString AbilityName = Definition.Abilities.IsValidIndex(Pending.AbilityIndex)
            ? Definition.Abilities[Pending.AbilityIndex].Name
            : FString(TEXT("Habilidade"));

        if (NativeCastDamagedEnemy(Pending))
        {
            UE_LOG(LogTemp, Display, TEXT("[ABILITY-CHECK] %s/%s: cast nativo confirmou dano; nenhuma correcao aplicada."),
                *Definition.Name, *AbilityName);
        }
        else
        {
            const int32 AssistedHits = ApplyFallbackAbilityHit(Pending);
            if (AssistedHits > 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("[ABILITY-ASSIST] %s/%s: soft aim corrigiu o targeting -> %d alvo(s)."),
                    *Definition.Name, *AbilityName, AssistedHits);
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("[ABILITY-FREECAST] %s/%s executada sem alvo: cast permanece valido, sem target lock obrigatorio."),
                    *Definition.Name, *AbilityName);
            }
        }

        PendingAssists.RemoveAtSwap(Index);
    }
}

bool ANWPremiumGameplayDirector::NativeCastDamagedEnemy(const FPendingAbilityAssist& Pending) const
{
    const int32 Count = FMath::Min(Pending.ObservedEnemies.Num(), Pending.HealthRatiosAtCast.Num());
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const ANWEnemy* Enemy = Pending.ObservedEnemies[Index].Get();
        if (!IsValid(Enemy))
        {
            return true;
        }
        if (Enemy->GetHealthRatio() < Pending.HealthRatiosAtCast[Index] - 0.0005f)
        {
            return true;
        }
    }
    return false;
}

int32 ANWPremiumGameplayDirector::ApplyFallbackAbilityHit(const FPendingAbilityAssist& Pending)
{
    ANWCharacter* Character = Pending.Character.Get();
    if (!Character || !GetWorld()) { return 0; }

    const FNWWeaponDefinition Definition = NWCombat::GetWeaponDefinition(Pending.Weapon);
    if (!Definition.Abilities.IsValidIndex(Pending.AbilityIndex)) { return 0; }
    const FNWWeaponAbilityDefinition& Ability = Definition.Abilities[Pending.AbilityIndex];
    if (Ability.Kind == ENWAbilityKind::Heal) { return 0; }

    const float MaxRange = FMath::Max(Ability.Range, 320.0f);
    const bool bRanged = IsRangedWeapon(Pending.Weapon);
    const FVector CastPoint = ClampAimPointToAbilityRange(Character, Pending.AimPoint, MaxRange, !bRanged);
    AController* InstigatorController = Character->GetController();

    if (Ability.Kind == ENWAbilityKind::DirectDamage)
    {
        ANWEnemy* AimTarget = FindBestAimTarget(Character, CastPoint, Pending.AimDirection, MaxRange, SoftAimRadius);
        if (!AimTarget) { return 0; }
        UGameplayStatics::ApplyDamage(AimTarget, Ability.Power, InstigatorController, Character, UDamageType::StaticClass());
        return 1;
    }

    const float Radius = FMath::Max(180.0f, Ability.Radius + 65.0f);
    int32 Hits = 0;
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }
        if (FVector::Dist2D(CastPoint, Enemy->GetActorLocation()) > Radius) { continue; }

        UGameplayStatics::ApplyDamage(Enemy, Ability.Power, InstigatorController, Character, UDamageType::StaticClass());
        ++Hits;
        if (Hits >= 8) { break; }
    }

    if (Hits == 0)
    {
        if (ANWEnemy* SoftTarget = FindBestAimTarget(Character, CastPoint, Pending.AimDirection, MaxRange, SoftAimRadius * 0.85f))
        {
            if (FVector::Dist2D(CastPoint, SoftTarget->GetActorLocation()) <= Radius + SoftAimRadius)
            {
                UGameplayStatics::ApplyDamage(SoftTarget, Ability.Power, InstigatorController, Character, UDamageType::StaticClass());
                Hits = 1;
            }
        }
    }

    return Hits;
}

ANWEnemy* ANWPremiumGameplayDirector::FindBestAimTarget(
    ANWCharacter* Character,
    const FVector& AimPoint,
    const FVector& AimDirection,
    float MaxRange,
    float SoftRadius) const
{
    if (!Character || !GetWorld()) { return nullptr; }

    const FVector Origin = Character->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
    FVector Direction = AimDirection.GetSafeNormal();
    if (Direction.IsNearlyZero()) { Direction = Character->GetBaseAimRotation().Vector().GetSafeNormal(); }
    if (Direction.IsNearlyZero()) { Direction = Character->GetActorForwardVector(); }
    const FVector SegmentEnd = Origin + Direction * MaxRange;

    ANWEnemy* BestTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max();

    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }

        const FVector EnemyPoint = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
        const FVector ToEnemy = EnemyPoint - Origin;
        const float ForwardDistance = FVector::DotProduct(ToEnemy, Direction);
        if (ForwardDistance < -60.0f || ForwardDistance > MaxRange + SoftRadius) { continue; }

        const FVector Closest = FMath::ClosestPointOnSegment(EnemyPoint, Origin, SegmentEnd);
        const float LateralDistance = FVector::Dist(EnemyPoint, Closest);
        const float PointDistance = FVector::Dist(EnemyPoint, AimPoint);
        if (LateralDistance > SoftRadius && PointDistance > SoftRadius * 1.35f) { continue; }

        const float Score = LateralDistance * 1.7f + PointDistance * 0.35f + FMath::Max(0.0f, ForwardDistance) * 0.025f;
        if (Score < BestScore)
        {
            BestScore = Score;
            BestTarget = Enemy;
        }
    }

    return BestTarget;
}

FVector ANWPremiumGameplayDirector::ResolveFreeAimPoint(ANWCharacter* Character, float MaxRange, FVector& OutAimDirection) const
{
    OutAimDirection = FVector::ForwardVector;
    if (!Character || !GetWorld()) { return FVector::ZeroVector; }

    FVector ViewLocation;
    FRotator ViewRotation;
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    OutAimDirection = ViewRotation.Vector().GetSafeNormal();
    if (OutAimDirection.IsNearlyZero()) { OutAimDirection = Character->GetActorForwardVector(); }

    const FVector End = ViewLocation + OutAimDirection * FMath::Max(500.0f, MaxRange);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(NWFreeAimTrace), true, Character);
    Params.AddIgnoredActor(Character);

    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params) && Hit.bBlockingHit)
    {
        return Hit.ImpactPoint;
    }
    return End;
}

FVector ANWPremiumGameplayDirector::ClampAimPointToAbilityRange(
    ANWCharacter* Character,
    const FVector& AimPoint,
    float MaxRange,
    bool bFlattenForMelee) const
{
    if (!Character) { return AimPoint; }

    const FVector Origin = Character->GetActorLocation();
    FVector Delta = AimPoint - Origin;
    if (bFlattenForMelee) { Delta.Z = 0.0f; }

    const float SafeRange = FMath::Max(120.0f, MaxRange);
    const float Length = Delta.Size();
    if (Length > SafeRange && Length > KINDA_SMALL_NUMBER)
    {
        Delta = Delta / Length * SafeRange;
    }

    FVector Result = Origin + Delta;
    if (bFlattenForMelee)
    {
        Result.Z = Origin.Z - 70.0f;
    }
    return Result;
}

bool ANWPremiumGameplayDirector::IsRangedWeapon(ENWWeaponType Weapon) const
{
    return Weapon == ENWWeaponType::Staff || Weapon == ENWWeaponType::Bow || Weapon == ENWWeaponType::Firearm;
}
