#include "NWEnemy.h"

#include "Animation/AnimInstance.h"
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
    if (CapsuleMesh.Succeeded())
    {
        BodyMesh->SetStaticMesh(CapsuleMesh.Object);
    }
}

void ANWEnemy::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;
    GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
    TryApplyLicensedCreatureVisual();
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

        if (!Direction.IsNearlyZero())
        {
            SetActorRotation(Direction.Rotation());
        }
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
    if (AppliedDamage >= MaxHealth * 0.34f && Health > 0.0f)
    {
        ApplyStagger(0.28f);
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
    StaggeredUntilTime = FMath::Max(StaggeredUntilTime, GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, DurationSeconds));
    GetCharacterMovement()->StopMovementImmediately();
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

    const int32 LootSeed = GetUniqueID() * 31 + FMath::RoundToInt(GetWorld()->GetTimeSeconds() * 100.0f) + Epoch * 7919;
    const FNWGeneratedItem Item = NWCombat::GenerateProceduralItem(LootSeed, Epoch);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ANWLootPickup* Pickup = GetWorld()->SpawnActor<ANWLootPickup>(ANWLootPickup::StaticClass(), GetActorLocation() + FVector(0.0f, 0.0f, 70.0f), FRotator::ZeroRotator, SpawnParams);
    if (Pickup)
    {
        Pickup->InitializeLoot(Item);
        UE_LOG(LogTemp, Display, TEXT("[LOOT] %s caiu no mundo | score %.1f"), *Item.Name, NWCombat::GetItemScore(Item));
    }
}

void ANWEnemy::TryApplyLicensedCreatureVisual()
{
    USkeletalMesh* LicensedMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Meshes/Grux.Grux"));
    UClass* LicensedAnimClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Grux_AnimBlueprint.Grux_AnimBlueprint_C"));
    if (!LicensedMesh || !LicensedAnimClass)
    {
        return;
    }

    GetMesh()->SetSkeletalMeshAsset(LicensedMesh);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    GetMesh()->SetAnimInstanceClass(LicensedAnimClass);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetVisibility(true, true);
    BodyMesh->SetVisibility(false, true);
}

void ANWEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWEnemy, Health);
}

void ANWEnemy::OnRep_Health() {}

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
            const float Score = Distance * 0.55f;
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
