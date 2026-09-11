#include "NWEnemy.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
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

    APawn* TargetPawn = FindNearestPlayer();
    if (!TargetPawn)
    {
        return;
    }

    const FVector ToTarget = TargetPawn->GetActorLocation() - GetActorLocation();
    const float Distance2D = FVector(ToTarget.X, ToTarget.Y, 0.0f).Size();

    if (Distance2D > DetectionRange)
    {
        return;
    }

    if (Distance2D > AttackRange)
    {
        FVector Direction = FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal();
        const FVector Delta = Direction * MoveSpeed * DeltaSeconds;
        SetActorLocation(GetActorLocation() + Delta, true);

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
        UGameplayStatics::ApplyDamage(TargetPawn, AttackDamage, GetController(), this, UDamageType::StaticClass());
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

APawn* ANWEnemy::FindNearestPlayer() const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    APawn* BestPawn = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn)
        {
            continue;
        }

        const float DistanceSq = FVector::DistSquared(GetActorLocation(), Pawn->GetActorLocation());
        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            BestPawn = Pawn;
        }
    }

    return BestPawn;
}
