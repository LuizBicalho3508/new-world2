#include "NWPremiumGameplayDirector.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NWCharacter.h"
#include "NWCombatHUDWidget.h"
#include "NWCombatLibrary.h"
#include "NWEnemy.h"

ANWPremiumGameplayDirector::ANWPremiumGameplayDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;
    bReplicates = true;
    SetReplicateMovement(false);
    NetUpdateFrequency = 1.0f;
}

void ANWPremiumGameplayDirector::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Display, TEXT("[PREMIUM] gameplay watchdog ativo: HUD + verificacao de casts + aim-assist de fallback."));
}

void ANWPremiumGameplayDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()) { return; }

    EnsureLocalHUDs(DeltaSeconds);

    if (HasAuthority())
    {
        DetectAbilityCasts();
        ResolvePendingAssists();
    }
}

void ANWPremiumGameplayDirector::EnsureLocalHUDs(float DeltaSeconds)
{
    HudAccumulator += DeltaSeconds;
    if (HudAccumulator < HudEnsureInterval) { return; }
    HudAccumulator = 0.0f;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }

        ANWCharacter* Character = Cast<ANWCharacter>(PC->GetPawn());
        if (!Character) { continue; }

        TArray<UUserWidget*> ExistingWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, ExistingWidgets, UNWCombatHUDWidget::StaticClass(), false);
        if (!ExistingWidgets.IsEmpty()) { continue; }

        UNWCombatHUDWidget* HUD = CreateWidget<UNWCombatHUDWidget>(PC, UNWCombatHUDWidget::StaticClass());
        if (!HUD)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HUD] falha ao criar CombatHUD para %s."), *Character->GetName());
            continue;
        }

        HUD->SetObservedCharacter(Character);
        HUD->AddToViewport(30);
        HUD->SetVisibility(ESlateVisibility::HitTestInvisible);
        UE_LOG(LogTemp, Warning, TEXT("[HUD] CombatHUD garantido pelo premium watchdog para %s."), *Character->GetName());
    }
}

void ANWPremiumGameplayDirector::DetectAbilityCasts()
{
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
                UE_LOG(LogTemp, Display, TEXT("[ABILITY-CHECK] %s/%s aceito | cooldown %.2fs"),
                    *Definition.Name, *Ability.Name, CurrentCooldown);

                if (Ability.Kind != ENWAbilityKind::Heal)
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

    const float ObservationRange = FMath::Max(650.0f, Ability.Range * AimAssistRangeMultiplier + Ability.Radius * 2.0f);
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }
        if (FVector::Dist2D(Character->GetActorLocation(), Enemy->GetActorLocation()) > ObservationRange) { continue; }
        Pending.ObservedEnemies.Add(Enemy);
        Pending.HealthRatiosAtCast.Add(Enemy->GetHealthRatio());
    }

    PendingAssists.Add(MoveTemp(Pending));
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

        if (NativeCastDamagedEnemy(Pending))
        {
            UE_LOG(LogTemp, Display, TEXT("[ABILITY-CHECK] cast nativo confirmou dano; assistencia nao aplicada."));
        }
        else
        {
            const int32 AssistedHits = ApplyFallbackAbilityHit(Pending);
            const FNWWeaponDefinition Definition = NWCombat::GetWeaponDefinition(Pending.Weapon);
            const FString AbilityName = Definition.Abilities.IsValidIndex(Pending.AbilityIndex)
                ? Definition.Abilities[Pending.AbilityIndex].Name
                : FString(TEXT("Habilidade"));

            if (AssistedHits > 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("[ABILITY-ASSIST] %s/%s: targeting de terceira pessoa corrigido -> %d alvo(s)."),
                    *Definition.Name, *AbilityName, AssistedHits);
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("[ABILITY-MISS] %s/%s: nenhum inimigo valido dentro do alcance/cone."),
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
            // Destruido durante a janela quase sempre significa dano letal do cast nativo.
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

    const float MaxRange = FMath::Max(350.0f, Ability.Range * AimAssistRangeMultiplier);
    ANWEnemy* AimTarget = FindBestAimTarget(Character, MaxRange);
    AController* InstigatorController = Character->GetController();

    if (Ability.Kind == ENWAbilityKind::DirectDamage)
    {
        if (!AimTarget) { return 0; }
        UGameplayStatics::ApplyDamage(AimTarget, Ability.Power, InstigatorController, Character, UDamageType::StaticClass());
        return 1;
    }

    FVector Center = Character->GetActorLocation() + Character->GetBaseAimRotation().Vector() * FMath::Min(MaxRange * 0.55f, 360.0f);
    if (AimTarget)
    {
        Center = AimTarget->GetActorLocation();
    }

    const float Radius = FMath::Max(220.0f, Ability.Radius * 1.45f);
    int32 Hits = 0;
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }
        if (FVector::Dist2D(Center, Enemy->GetActorLocation()) > Radius) { continue; }

        UGameplayStatics::ApplyDamage(Enemy, Ability.Power, InstigatorController, Character, UDamageType::StaticClass());
        ++Hits;
        if (Hits >= 8) { break; }
    }

    // Se a area ficou vazia mas havia um alvo valido no cone, garante pelo menos o
    // alvo principal. Isso evita Q/E parecerem mortos em terceira pessoa.
    if (Hits == 0 && AimTarget)
    {
        UGameplayStatics::ApplyDamage(AimTarget, Ability.Power, InstigatorController, Character, UDamageType::StaticClass());
        Hits = 1;
    }

    return Hits;
}

ANWEnemy* ANWPremiumGameplayDirector::FindBestAimTarget(ANWCharacter* Character, float MaxRange) const
{
    if (!Character || !GetWorld()) { return nullptr; }

    const FVector Origin = Character->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
    FVector AimDirection = Character->GetBaseAimRotation().Vector().GetSafeNormal();
    if (AimDirection.IsNearlyZero()) { AimDirection = Character->GetActorForwardVector(); }

    ANWEnemy* BestTarget = nullptr;
    float BestScore = -BIG_NUMBER;

    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }

        FVector ToTarget = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f) - Origin;
        const float Distance = ToTarget.Size();
        if (Distance <= 1.0f || Distance > MaxRange) { continue; }

        ToTarget /= Distance;
        const float Dot = FVector::DotProduct(AimDirection, ToTarget);
        if (Dot < MinimumAimDot) { continue; }

        // Alinhamento pesa mais do que distancia: funciona como aim assist leve,
        // nao como auto-target em 360 graus.
        const float Score = Dot * 1000.0f - Distance * 0.22f;
        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Enemy;
        }
    }

    return BestTarget;
}
