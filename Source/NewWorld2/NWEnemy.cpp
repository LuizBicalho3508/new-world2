#include "NWEnemy.h"

#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "NWCivilian.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"
#include "NWLootPickup.h"
#include "NWProceduralWorldManager.h"
#include "NWSettlementCore.h"
#include "UObject/ConstructorHelpers.h"

ANWEnemy::ANWEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;

    bReplicates = true;
    SetReplicateMovement(true);
    NetUpdateFrequency = 20.0f;

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(GetCapsuleComponent());
    BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -2.0f));
    BodyMesh->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.55f));
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleMesh(TEXT("/Engine/BasicShapes/Capsule.Capsule"));
    if (CapsuleMesh.Succeeded()) { BodyMesh->SetStaticMesh(CapsuleMesh.Object); }
}

void ANWEnemy::BeginPlay()
{
    Super::BeginPlay();
    ApplyArchetypeStats();
    if (HasAuthority()) { Health = MaxHealth; }
    GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
    TryApplyLicensedCreatureVisual();
}

void ANWEnemy::ConfigureEnemy(ENWEnemyArchetype InArchetype, bool bInWorldBoss, int32 InBossTier)
{
    EnemyArchetype = InArchetype;
    bWorldBoss = bInWorldBoss;
    BossTier = FMath::Clamp(InBossTier, 1, 8);
    ApplyArchetypeStats();
    if (HasAuthority())
    {
        Health = MaxHealth;
        ForceNetUpdate();
    }
    TryApplyLicensedCreatureVisual();
}

void ANWEnemy::ApplyArchetypeStats()
{
    switch (EnemyArchetype)
    {
        case ENWEnemyArchetype::Zombie:
            MaxHealth = 95.0f;
            MoveSpeed = 175.0f;
            AttackDamage = 11.0f;
            AttackCooldown = 1.35f;
            AttackRange = 165.0f;
            break;
        case ENWEnemyArchetype::Ghost:
            MaxHealth = 78.0f;
            MoveSpeed = 285.0f;
            AttackDamage = 12.0f;
            AttackCooldown = 1.05f;
            AttackRange = 195.0f;
            break;
        case ENWEnemyArchetype::Brute:
        default:
            MaxHealth = 70.0f;
            MoveSpeed = 235.0f;
            AttackDamage = 9.0f;
            AttackCooldown = 1.15f;
            AttackRange = 175.0f;
            break;
    }

    if (bWorldBoss)
    {
        MaxHealth = 700.0f + BossTier * 170.0f;
        AttackDamage = 13.0f + BossTier * 2.6f;
        AttackCooldown = FMath::Max(0.85f, 1.35f - BossTier * 0.05f);
        AttackRange = 230.0f;
        MoveSpeed = FMath::Max(190.0f, MoveSpeed * 0.92f);
        PlayerAggroRange = 4200.0f;
        WorldTargetRange = 6000.0f;
        GetCapsuleComponent()->SetCapsuleSize(62.0f, 120.0f);
    }

    if (GetCharacterMovement()) { GetCharacterMovement()->MaxWalkSpeed = MoveSpeed; }
}

void ANWEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority() || !GetWorld()) { return; }

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < StaggeredUntilTime)
    {
        GetCharacterMovement()->StopMovementImmediately();
        return;
    }

    AActor* Target = FindBestTarget();
    if (!Target) { return; }

    const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
    const float Distance2D = FVector(ToTarget.X, ToTarget.Y, 0.0f).Size();

    if (Distance2D > AttackRange)
    {
        const FVector Direction = FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal();
        SetActorLocation(GetActorLocation() + Direction * MoveSpeed * DeltaSeconds, true);
        if (!Direction.IsNearlyZero()) { SetActorRotation(Direction.Rotation()); }
        return;
    }

    if ((Now - LastAttackTime) >= AttackCooldown)
    {
        LastAttackTime = Now;
        UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
    }
}

float ANWEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (!HasAuthority() || AppliedDamage <= 0.0f) { return AppliedDamage; }

    Health = FMath::Clamp(Health - AppliedDamage, 0.0f, MaxHealth);
    const float StaggerThreshold = bWorldBoss ? 0.18f : 0.34f;
    if (AppliedDamage >= MaxHealth * StaggerThreshold && Health > 0.0f)
    {
        ApplyStagger(bWorldBoss ? 0.18f : 0.28f);
    }

    if (Health <= 0.0f)
    {
        SpawnProceduralLoot(EventInstigator, DamageCauser);
        Destroy();
    }

    return AppliedDamage;
}

void ANWEnemy::ApplyStagger(float DurationSeconds)
{
    if (!HasAuthority() || !GetWorld()) { return; }
    const float Resistance = bWorldBoss ? 0.45f : 1.0f;
    StaggeredUntilTime = FMath::Max(StaggeredUntilTime, GetWorld()->GetTimeSeconds() + FMath::Max(0.08f, DurationSeconds * Resistance));
    GetCharacterMovement()->StopMovementImmediately();
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

USkeletalMesh* ANWEnemy::FindInstalledCreatureMesh(const TArray<FString>& Keywords) const
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);
    int32 BestScore = 0;
    FAssetData BestAsset;
    for (const FAssetData& Asset : Assets)
    {
        const FString Searchable = Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString();
        int32 Score = 0;
        for (const FString& Keyword : Keywords)
        {
            if (Searchable.Contains(Keyword, ESearchCase::IgnoreCase)) { Score += 10; }
        }
        if (Score > BestScore)
        {
            BestScore = Score;
            BestAsset = Asset;
        }
    }
    return BestScore > 0 ? Cast<USkeletalMesh>(BestAsset.GetAsset()) : nullptr;
}

void ANWEnemy::TryApplyLicensedCreatureVisual()
{
    USkeletalMesh* LicensedMesh = nullptr;
    UClass* LicensedAnimClass = nullptr;

    if (bWorldBoss)
    {
        LicensedMesh = FindInstalledCreatureMesh({ TEXT("Boss"), TEXT("Demon"), TEXT("Warlord"), TEXT("Monster") });
    }
    else if (EnemyArchetype == ENWEnemyArchetype::Zombie)
    {
        LicensedMesh = FindInstalledCreatureMesh({ TEXT("Zombie"), TEXT("Undead"), TEXT("Ghoul") });
    }
    else if (EnemyArchetype == ENWEnemyArchetype::Ghost)
    {
        LicensedMesh = FindInstalledCreatureMesh({ TEXT("Ghost"), TEXT("Wraith"), TEXT("Specter"), TEXT("Spirit") });
    }
    else
    {
        LicensedMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Meshes/Grux.Grux"));
        LicensedAnimClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Grux_AnimBlueprint.Grux_AnimBlueprint_C"));
    }

    if (!LicensedMesh && EnemyArchetype != ENWEnemyArchetype::Brute)
    {
        LicensedMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Meshes/Grux.Grux"));
        LicensedAnimClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Grux_AnimBlueprint.Grux_AnimBlueprint_C"));
    }
    if (!LicensedMesh) { return; }

    GetMesh()->SetSkeletalMeshAsset(LicensedMesh);
    if (LicensedAnimClass)
    {
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        GetMesh()->SetAnimInstanceClass(LicensedAnimClass);
    }
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, bWorldBoss ? -120.0f : -88.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetRelativeScale3D(bWorldBoss ? FVector(1.35f) : FVector(1.0f));
    GetMesh()->SetVisibility(true, true);
    BodyMesh->SetVisibility(false, true);
}

void ANWEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWEnemy, Health);
    DOREPLIFETIME(ANWEnemy, EnemyArchetype);
    DOREPLIFETIME(ANWEnemy, bWorldBoss);
    DOREPLIFETIME(ANWEnemy, BossTier);
}

void ANWEnemy::OnRep_Health() {}
void ANWEnemy::OnRep_EnemyIdentity()
{
    ApplyArchetypeStats();
    TryApplyLicensedCreatureVisual();
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
