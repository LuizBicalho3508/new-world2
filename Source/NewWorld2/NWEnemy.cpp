#include "NWEnemy.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NWCivilian.h"
#include "NWCharacter.h"
#include "NWSettlementCore.h"
#include "UObject/ConstructorHelpers.h"

ANWEnemy::ANWEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;

    bReplicates = true;
    SetReplicateMovement(true);

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
}

void ANWEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority())
    {
        return;
    }

    AActor* Target = FindBestTarget();
    if (!Target)
    {
        return;
    }

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

    const float Now = GetWorld()->GetTimeSeconds();
    if ((Now - LastAttackTime) >= AttackCooldown)
    {
        LastAttackTime = Now;
        UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
    }
}

float ANWEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (!HasAuthority() || AppliedDamage <= 0.0f)
    {
        return AppliedDamage;
    }

    Health = FMath::Clamp(Health - AppliedDamage, 0.0f, MaxHealth);
    if (Health <= 0.0f)
    {
        ANWCharacter* Killer = EventInstigator ? Cast<ANWCharacter>(EventInstigator->GetPawn()) : Cast<ANWCharacter>(DamageCauser);
        if (Killer)
        {
            const int32 LootSeed = GetUniqueID() * 31 + FMath::RoundToInt(GetWorld()->GetTimeSeconds() * 100.0f);
            Killer->ReceiveProceduralLoot(LootSeed);
        }
        Destroy();
    }

    return AppliedDamage;
}

void ANWEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWEnemy, Health);
}

void ANWEnemy::OnRep_Health()
{
}

AActor* ANWEnemy::FindBestTarget() const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    AActor* BestTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn)
        {
            continue;
        }

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
