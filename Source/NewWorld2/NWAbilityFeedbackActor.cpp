#include "NWAbilityFeedbackActor.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ANWAbilityFeedbackActor::ANWAbilityFeedbackActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.02f;
    bReplicates = false;
    SetActorEnableCollision(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PulseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PulseMesh"));
    PulseMesh->SetupAttachment(SceneRoot);
    PulseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PulseMesh->SetGenerateOverlapEvents(false);
    PulseMesh->SetCastShadow(false);
    PulseMesh->SetReceivesDecals(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        PulseMesh->SetStaticMesh(SphereMesh.Object);
    }

    PulseLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PulseLight"));
    PulseLight->SetupAttachment(SceneRoot);
    PulseLight->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
    PulseLight->SetCastShadows(false);
    PulseLight->SetAttenuationRadius(520.0f);
    PulseLight->SetIntensity(0.0f);
}

void ANWAbilityFeedbackActor::BeginPlay()
{
    Super::BeginPlay();

    if (PulseMesh)
    {
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (BaseMaterial)
        {
            DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this, TEXT("MID_NW_AbilityFeedback"));
            if (DynamicMaterial)
            {
                DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FeedbackColor);
                PulseMesh->SetMaterial(0, DynamicMaterial);
            }
        }
    }

    if (PulseLight)
    {
        PulseLight->SetLightColor(FeedbackColor);
        PulseLight->SetIntensity(2400.0f);
    }
}

void ANWAbilityFeedbackActor::InitializeFeedback(const FLinearColor& Color, float Radius, float DurationSeconds)
{
    FeedbackColor = Color;
    TargetRadius = FMath::Clamp(Radius, 55.0f, 650.0f);
    Duration = FMath::Clamp(DurationSeconds, 0.12f, 0.65f);

    if (DynamicMaterial)
    {
        DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FeedbackColor);
    }
    if (PulseLight)
    {
        PulseLight->SetLightColor(FeedbackColor);
    }
}

void ANWAbilityFeedbackActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    Elapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(Elapsed / FMath::Max(0.01f, Duration), 0.0f, 1.0f);
    const float Ease = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.25f);

    // A esfera e deliberadamente achatada para virar um pulso/telegraph de chao,
    // evitando uma bolha opaca que esconda personagem e inimigos.
    const float XYScale = FMath::Lerp(0.08f, TargetRadius / 50.0f, Ease);
    const float ZScale = FMath::Lerp(0.018f, 0.035f, Ease);
    if (PulseMesh)
    {
        PulseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
        PulseMesh->SetRelativeScale3D(FVector(XYScale, XYScale, ZScale));
        PulseMesh->SetVisibility(Alpha < 0.72f, true);
    }

    if (PulseLight)
    {
        PulseLight->SetIntensity(FMath::Lerp(2400.0f, 0.0f, Alpha));
        PulseLight->SetAttenuationRadius(FMath::Max(220.0f, TargetRadius * 1.8f));
    }

    if (Alpha >= 1.0f)
    {
        Destroy();
    }
}
