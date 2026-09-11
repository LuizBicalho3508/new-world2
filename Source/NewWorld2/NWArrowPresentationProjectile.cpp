#include "NWArrowPresentationProjectile.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ANWArrowPresentationProjectile::ANWArrowPresentationProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetReplicateMovement(false);
    InitialLifeSpan = 2.5f;

    RootSphere = CreateDefaultSubobject<USphereComponent>(TEXT("RootSphere"));
    RootSphere->InitSphereRadius(7.0f);
    RootSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RootComponent = RootSphere;

    ArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
    ArrowMesh->SetupAttachment(RootSphere);
    ArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArrowMesh->SetGenerateOverlapEvents(false);
    ArrowMesh->SetCastShadow(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackArrow(TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (FallbackArrow.Succeeded())
    {
        ArrowMesh->SetStaticMesh(FallbackArrow.Object);
        ArrowMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.42f));
        ArrowMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    }

    Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
    Trail->SetupAttachment(RootSphere);
    Trail->SetAutoActivate(false);

    ElementLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ElementLight"));
    ElementLight->SetupAttachment(RootSphere);
    ElementLight->SetIntensity(0.0f);
    ElementLight->SetAttenuationRadius(260.0f);
    ElementLight->SetCastShadows(false);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = RootSphere;
    ProjectileMovement->InitialSpeed = 3800.0f;
    ProjectileMovement->MaxSpeed = 4600.0f;
    ProjectileMovement->ProjectileGravityScale = 0.22f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
}

void ANWArrowPresentationProjectile::InitializePresentation(
    UStaticMesh* InArrowMesh,
    UNiagaraSystem* InTrailSystem,
    ENWArrowElement InElement,
    const FVector& Direction,
    float Speed,
    float Scale)
{
    if (InArrowMesh)
    {
        ArrowMesh->SetStaticMesh(InArrowMesh);
        ArrowMesh->SetRelativeScale3D(FVector(Scale));
        ArrowMesh->SetRelativeRotation(FRotator::ZeroRotator);
    }

    if (InTrailSystem)
    {
        Trail->SetAsset(InTrailSystem);
        Trail->Activate(true);
    }

    FLinearColor LightColor = FLinearColor::White;
    float LightIntensity = 0.0f;
    switch (InElement)
    {
        case ENWArrowElement::Fire:
            LightColor = FLinearColor(1.0f, 0.20f, 0.03f);
            LightIntensity = 2400.0f;
            break;
        case ENWArrowElement::Poison:
            LightColor = FLinearColor(0.15f, 1.0f, 0.08f);
            LightIntensity = 1900.0f;
            break;
        case ENWArrowElement::Lightning:
            LightColor = FLinearColor(0.18f, 0.58f, 1.0f);
            LightIntensity = 2800.0f;
            break;
        case ENWArrowElement::Frost:
            LightColor = FLinearColor(0.45f, 0.88f, 1.0f);
            LightIntensity = 2100.0f;
            break;
        default:
            break;
    }

    ElementLight->SetLightColor(LightColor);
    ElementLight->SetIntensity(LightIntensity);

    const FVector SafeDirection = Direction.IsNearlyZero() ? FVector::ForwardVector : Direction.GetSafeNormal();
    ProjectileMovement->InitialSpeed = Speed;
    ProjectileMovement->MaxSpeed = FMath::Max(Speed, 4600.0f);
    ProjectileMovement->Velocity = SafeDirection * Speed;
    SetActorRotation(SafeDirection.Rotation());
}
