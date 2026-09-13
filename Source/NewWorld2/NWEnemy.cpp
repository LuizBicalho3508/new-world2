#include "NWEnemy.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NWCivilian.h"
#include "NWCharacter.h"
#include "NWCombatDirectorSubsystem.h"
#include "NWCombatLibrary.h"
#include "NWEnemyHealthBarWidget.h"
#include "NWLootPickup.h"
#include "NWProceduralWorldManager.h"
#include "NWSettlementCore.h"
#include "UObject/ConstructorHelpers.h"

ANWEnemy::ANWEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.10f;

    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(4.0f);

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(GetCapsuleComponent());
    BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -2.0f));
    BodyMesh->SetRelativeScale3D(FVector(0.38f, 0.38f, 1.65f));
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // UE 5.8 no longer contains /Engine/BasicShapes/Capsule. Cylinder is only
    // a last-resort visual and is hidden as soon as EnemyVisualDirector applies a creature mesh.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (FallbackMesh.Succeeded()) { BodyMesh->SetStaticMesh(FallbackMesh.Object); }

    HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("EnemyHealthBar"));
    HealthBarWidget->SetupAttachment(GetCapsuleComponent());
    HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 128.0f));
    HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
    HealthBarWidget->SetWidgetClass(UNWEnemyHealthBarWidget::StaticClass());
    HealthBarWidget->SetDrawSize(FVector2D(240.0f, 62.0f));
    HealthBarWidget->SetDrawAtDesiredSize(false);
    HealthBarWidget->SetPivot(FVector2D(0.5f, 0.5f));
    HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANWEnemy::BeginPlay()
{
    Super::BeginPlay();
    ApplyArchetypeStats();
    if (HasAuthority())
    {
        Health = MaxHealth;
        Poise = MaxPoise;
    }
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
        GetCharacterMovement()->bUseControllerDesiredRotation = false;
        GetCharacterMovement()->bOrientRotationToMovement = false;
    }
    BindHealthBar();
    RefreshHealthBar();
}

void ANWEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        if (UNWCombatDirectorSubsystem* Director = GetWorld()->GetSubsystem<UNWCombatDirectorSubsystem>())
        {
            Director->ForgetActor(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void ANWEnemy::ConfigureEnemy(ENWEnemyArchetype InArchetype, bool bInWorldBoss, int32 InBossTier)
{
    EnemyArchetype = InArchetype;
    bWorldBoss = bInWorldBoss;
    BossTier = FMath::Clamp(InBossTier, 1, 8);
    ApplyArchetypeStats();

    PrimaryActorTick.TickInterval = bWorldBoss ? 0.08f : NormalThinkInterval;
    SetNetUpdateFrequency(bWorldBoss ? 15.0f : 10.0f);
    SetMinNetUpdateFrequency(bWorldBoss ? 7.5f : 4.0f);
    CachedTarget.Reset();
    NextTargetRefreshTime = -1000.0f;

    if (HasAuthority())
    {
        Health = MaxHealth;
        Poise = MaxPoise;
        LastPoiseDamageTime = -1000.0f;
        ForceNetUpdate();
    }

    if (HealthBarWidget)
    {
        HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, bWorldBoss ? 185.0f : 128.0f));
        HealthBarWidget->SetDrawSize(bWorldBoss ? FVector2D(330.0f, 74.0f) : FVector2D(240.0f, 62.0f));
    }
    RefreshHealthBar();
}

void ANWEnemy::ApplyArchetypeStats()
{
    if (GetCapsuleComponent()) { GetCapsuleComponent()->SetCapsuleSize(42.0f, 88.0f); }
    PlayerAggroRange = 2350.0f;
    WorldTargetRange = 9000.0f;
    PoiseRecoveryDelay = 2.0f;
    PoiseRecoveryPerSecond = 34.0f;

    switch (EnemyArchetype)
    {
        case ENWEnemyArchetype::Zombie:
            MaxHealth = 270.0f;
            MaxPoise = 82.0f;
            MoveSpeed = 178.0f;
            AttackDamage = 12.0f;
            AttackCooldown = 1.35f;
            AttackRange = 170.0f;
            break;
        case ENWEnemyArchetype::Ghost:
            MaxHealth = 230.0f;
            MaxPoise = 64.0f;
            PoiseRecoveryPerSecond = 42.0f;
            MoveSpeed = 290.0f;
            AttackDamage = 13.0f;
            AttackCooldown = 1.05f;
            AttackRange = 200.0f;
            break;
        case ENWEnemyArchetype::Brute:
        default:
            MaxHealth = 360.0f;
            MaxPoise = 132.0f;
            MoveSpeed = 225.0f;
            AttackDamage = 16.0f;
            AttackCooldown = 1.30f;
            AttackRange = 185.0f;
            break;
    }

    if (bWorldBoss)
    {
        MaxHealth = 2600.0f + BossTier * 520.0f;
        MaxPoise = 280.0f + BossTier * 36.0f;
        PoiseRecoveryDelay = 2.65f;
        PoiseRecoveryPerSecond = 46.0f + BossTier * 3.0f;
        AttackDamage = 17.0f + BossTier * 2.8f;
        AttackCooldown = FMath::Max(0.88f, 1.38f - BossTier * 0.045f);
        AttackRange = 245.0f;
        MoveSpeed = FMath::Max(185.0f, MoveSpeed * 0.90f);
        PlayerAggroRange = 4600.0f;
        WorldTargetRange = 6500.0f;
        if (GetCapsuleComponent()) { GetCapsuleComponent()->SetCapsuleSize(62.0f, 120.0f); }
    }

    Poise = FMath::Clamp(Poise, 0.0f, MaxPoise);
    if (GetCharacterMovement()) { GetCharacterMovement()->MaxWalkSpeed = MoveSpeed; }
}

void ANWEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()) { return; }

    const float NearestPlayerDistance = GetNearestPlayerDistance();
    if (HealthBarWidget)
    {
        const float VisibleDistance = bWorldBoss ? 6500.0f : 3000.0f;
        HealthBarWidget->SetVisibility(NearestPlayerDistance <= VisibleDistance && Health > 0.0f);
    }

    if (!HasAuthority()) { return; }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    const float Now = GetWorld()->GetTimeSeconds();

    if ((Now - LastPoiseDamageTime) >= PoiseRecoveryDelay && Poise < MaxPoise)
    {
        Poise = FMath::Min(MaxPoise, Poise + PoiseRecoveryPerSecond * DeltaSeconds);
    }

    if (Now < StaggeredUntilTime)
    {
        if (Movement) { Movement->StopMovementImmediately(); }
        return;
    }

    if (!bWorldBoss && NearestPlayerDistance > SleepDistanceFromPlayers)
    {
        PrimaryActorTick.TickInterval = SleepingThinkInterval;
        CachedTarget.Reset();
        NextTargetRefreshTime = Now + SleepingThinkInterval;
        if (Movement) { Movement->Velocity = FVector::ZeroVector; }
        return;
    }

    PrimaryActorTick.TickInterval = bWorldBoss ? 0.08f : NormalThinkInterval;

    const float TargetRefreshInterval = bWorldBoss ? 0.12f : 0.25f;
    if (Now >= NextTargetRefreshTime || !CachedTarget.IsValid())
    {
        CachedTarget = FindBestTarget();
        NextTargetRefreshTime = Now + TargetRefreshInterval;
    }

    AActor* Target = CachedTarget.Get();
    if (!IsValid(Target))
    {
        if (Movement) { Movement->Velocity = FVector::ZeroVector; }
        return;
    }

    const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
    const float Distance2D = FVector(ToTarget.X, ToTarget.Y, 0.0f).Size();

    if (Distance2D > AttackRange)
    {
        const FVector Direction = FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal();
        FVector VisualMoveDirection = Direction;

        const FVector CurrentLocation = GetActorLocation();
        const FVector DesiredDelta = Direction * MoveSpeed * DeltaSeconds;
        FHitResult ForwardHit;
        SetActorLocation(CurrentLocation + DesiredDelta, true, &ForwardHit);

        if (ForwardHit.bBlockingHit)
        {
            const float SideSign = (GetUniqueID() & 1) == 0 ? 1.0f : -1.0f;
            const FVector SideDirection = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal() * SideSign;
            FHitResult SideHit;
            const FVector SideStart = GetActorLocation();
            SetActorLocation(SideStart + SideDirection * MoveSpeed * DeltaSeconds * 0.90f, true, &SideHit);

            if (!SideHit.bBlockingHit)
            {
                VisualMoveDirection = SideDirection;
            }
            else
            {
                const FVector OtherSide = -SideDirection;
                FHitResult OtherSideHit;
                const FVector OtherStart = GetActorLocation();
                SetActorLocation(OtherStart + OtherSide * MoveSpeed * DeltaSeconds * 0.72f, true, &OtherSideHit);
                if (!OtherSideHit.bBlockingHit) { VisualMoveDirection = OtherSide; }
            }
        }

        if (Movement) { Movement->Velocity = VisualMoveDirection * MoveSpeed; }
        if (!VisualMoveDirection.IsNearlyZero()) { SetActorRotation(VisualMoveDirection.Rotation()); }
        return;
    }

    if (Movement) { Movement->Velocity = FVector::ZeroVector; }
    FVector FaceTarget(ToTarget.X, ToTarget.Y, 0.0f);
    if (!FaceTarget.IsNearlyZero()) { SetActorRotation(FaceTarget.Rotation()); }

    if ((Now - LastAttackTime) >= AttackCooldown)
    {
        bool bCanAttack = true;
        if (UNWCombatDirectorSubsystem* Director = GetWorld()->GetSubsystem<UNWCombatDirectorSubsystem>())
        {
            bCanAttack = Director->RequestAttackPermit(this, Target, bWorldBoss);
        }

        if (bCanAttack)
        {
            LastAttackTime = Now;
            UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
        }
    }
}

float ANWEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || DamageAmount <= 0.0f) { return 0.0f; }

    const float MaxSingleHitFraction = bWorldBoss ? 0.14f : 0.46f;
    const float CappedIncomingDamage = FMath::Min(DamageAmount, MaxHealth * MaxSingleHitFraction);
    const float AppliedDamage = Super::TakeDamage(CappedIncomingDamage, DamageEvent, EventInstigator, DamageCauser);
    if (AppliedDamage <= 0.0f) { return AppliedDamage; }

    Health = FMath::Clamp(Health - AppliedDamage, 0.0f, MaxHealth);
    RefreshHealthBar();

    if (Health > 0.0f)
    {
        LastPoiseDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastPoiseDamageTime;
        const float ImpactMultiplier = bWorldBoss ? 0.70f : 1.0f;
        const float PoiseDamage = FMath::Max(6.0f, AppliedDamage * 1.35f) * ImpactMultiplier;
        Poise = FMath::Max(0.0f, Poise - PoiseDamage);

        if (Poise <= KINDA_SMALL_NUMBER)
        {
            Poise = MaxPoise;
            ApplyStagger(bWorldBoss ? 0.42f : 0.72f);
            UE_LOG(LogTemp, Display, TEXT("[POISE] %s teve postura quebrada."), *GetDisplayName());
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[MOB-HP] %s recebeu %.1f | %.0f/%.0f (%.0f%%) | poise %.0f/%.0f"),
        *GetDisplayName(), AppliedDamage, Health, MaxHealth, GetHealthRatio() * 100.0f, Poise, MaxPoise);

    if (Health <= 0.0f)
    {
        if (HealthBarWidget) { HealthBarWidget->SetVisibility(false); }
        SpawnProceduralLoot(EventInstigator, DamageCauser);
        Destroy();
    }

    return AppliedDamage;
}

void ANWEnemy::ApplyStagger(float DurationSeconds)
{
    if (!HasAuthority() || !GetWorld()) { return; }
    const float Resistance = bWorldBoss ? 0.55f : 1.0f;
    StaggeredUntilTime = FMath::Max(StaggeredUntilTime, GetWorld()->GetTimeSeconds() + FMath::Max(0.08f, DurationSeconds * Resistance));
    if (GetCharacterMovement()) { GetCharacterMovement()->StopMovementImmediately(); }
}

FString ANWEnemy::GetDisplayName() const
{
    FString ArchetypeName;
    switch (EnemyArchetype)
    {
        case ENWEnemyArchetype::Zombie: ArchetypeName = TEXT("Morto-Vivo"); break;
        case ENWEnemyArchetype::Ghost: ArchetypeName = TEXT("Espectro"); break;
        case ENWEnemyArchetype::Brute:
        default: ArchetypeName = TEXT("Brutamontes"); break;
    }

    if (bWorldBoss)
    {
        return FString::Printf(TEXT("BOSS T%d - %s"), BossTier, *ArchetypeName);
    }
    return ArchetypeName;
}

void ANWEnemy::BindHealthBar()
{
    if (!HealthBarWidget) { return; }
    HealthBarWidget->InitWidget();
    if (UNWEnemyHealthBarWidget* Widget = Cast<UNWEnemyHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
    {
        Widget->SetObservedEnemy(this);
    }
}

void ANWEnemy::RefreshHealthBar()
{
    if (!HealthBarWidget) { return; }
    if (UNWEnemyHealthBarWidget* Widget = Cast<UNWEnemyHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
    {
        Widget->SetObservedEnemy(this);
        Widget->RefreshFromEnemy();
    }
}

void ANWEnemy::SpawnLootItem(const FNWGeneratedItem& Item, const FVector& Offset)
{
    if (!GetWorld()) { return; }
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ANWLootPickup* Pickup = GetWorld()->SpawnActor<ANWLootPickup>(ANWLootPickup::StaticClass(), GetActorLocation() + Offset, FRotator::ZeroRotator, SpawnParams);
    if (Pickup) { Pickup->InitializeLoot(Item); }
}

void ANWEnemy::SpawnProceduralLoot(AController* EventInstigator, AActor* DamageCauser)
{
    ANWCharacter* Killer = EventInstigator ? Cast<ANWCharacter>(EventInstigator->GetPawn()) : Cast<ANWCharacter>(DamageCauser);
    if (!Killer || !GetWorld()) { return; }

    int32 Epoch = 1;
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        Epoch = It->GetWorldEpoch();
        break;
    }

    const int32 BaseSeed = GetUniqueID() * 31 + FMath::RoundToInt(GetWorld()->GetTimeSeconds() * 100.0f) + Epoch * 7919;
    if (!bWorldBoss)
    {
        const FNWGeneratedItem Item = NWCombat::GenerateProceduralItem(BaseSeed, Epoch);
        SpawnLootItem(Item, FVector(0.0f, 0.0f, 70.0f));
        UE_LOG(LogTemp, Display, TEXT("[LOOT] %s caiu no mundo | score %.1f"), *Item.Name, NWCombat::GetItemScore(Item));
        return;
    }

    const FName Theme = EnemyArchetype == ENWEnemyArchetype::Ghost
        ? FName(TEXT("SpectralBoss"))
        : (EnemyArchetype == ENWEnemyArchetype::Zombie ? FName(TEXT("UndeadBoss")) : FName(TEXT("BrutalBoss")));

    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FNWGeneratedItem Legendary = NWCombat::GenerateLegendaryDungeonItem(BaseSeed + 977 * (Index + 1), Epoch + BossTier, Theme);
        const float Angle = 2.0f * PI * Index / 3.0f;
        SpawnLootItem(Legendary, FVector(FMath::Cos(Angle) * 85.0f, FMath::Sin(Angle) * 85.0f, 90.0f));
    }

    const FNWGeneratedItem Potion = NWCombat::GenerateBrutalTransformationPotion(BaseSeed ^ 0x51A7B0, Epoch + BossTier);
    SpawnLootItem(Potion, FVector(0.0f, 0.0f, 145.0f));
    UE_LOG(LogTemp, Warning, TEXT("[BOSS] derrotado: 3 lendarios + Pocao da Armadura Brutal Lendaria gerados."));
}

void ANWEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWEnemy, Health);
    DOREPLIFETIME(ANWEnemy, Poise);
    DOREPLIFETIME(ANWEnemy, EnemyArchetype);
    DOREPLIFETIME(ANWEnemy, bWorldBoss);
    DOREPLIFETIME(ANWEnemy, BossTier);
}

void ANWEnemy::OnRep_Health()
{
    RefreshHealthBar();
}

void ANWEnemy::OnRep_EnemyIdentity()
{
    ApplyArchetypeStats();
    if (HealthBarWidget)
    {
        HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, bWorldBoss ? 185.0f : 128.0f));
        HealthBarWidget->SetDrawSize(bWorldBoss ? FVector2D(330.0f, 74.0f) : FVector2D(240.0f, 62.0f));
    }
    RefreshHealthBar();
}

float ANWEnemy::GetNearestPlayerDistance() const
{
    if (!GetWorld()) { return TNumericLimits<float>::Max(); }

    float BestDistance = TNumericLimits<float>::Max();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn) { continue; }
        BestDistance = FMath::Min(BestDistance, FVector::Dist2D(GetActorLocation(), Pawn->GetActorLocation()));
    }
    return BestDistance;
}

AActor* ANWEnemy::FindBestTarget() const
{
    if (!GetWorld()) { return nullptr; }

    AActor* BestTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn) { continue; }

        const float Distance = FVector::Dist2D(GetActorLocation(), Pawn->GetActorLocation());
        if (Distance <= PlayerAggroRange)
        {
            const float Score = Distance * (bWorldBoss ? 0.35f : 0.55f);
            if (Score < BestScore)
            {
                BestScore = Score;
                BestTarget = Pawn;
            }
        }
    }

    for (TActorIterator<ANWCivilian> It(GetWorld()); It; ++It)
    {
        const float Distance = FVector::Dist2D(GetActorLocation(), It->GetActorLocation());
        if (Distance <= WorldTargetRange && Distance < BestScore)
        {
            BestScore = Distance;
            BestTarget = *It;
        }
    }

    for (TActorIterator<ANWSettlementCore> It(GetWorld()); It; ++It)
    {
        const float Distance = FVector::Dist2D(GetActorLocation(), It->GetActorLocation());
        const float Score = Distance * 1.08f;
        if (Distance <= WorldTargetRange && Score < BestScore)
        {
            BestScore = Score;
            BestTarget = *It;
        }
    }

    return BestTarget;
}
