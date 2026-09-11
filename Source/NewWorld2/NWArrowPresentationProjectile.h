#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWArrowPresentationProjectile.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWArrowPresentationProjectile : public AActor
{
    GENERATED_BODY()

public:
    ANWArrowPresentationProjectile();

    void InitializePresentation(
        UStaticMesh* InArrowMesh,
        UNiagaraSystem* InTrailSystem,
        ENWArrowElement InElement,
        const FVector& Direction,
        float Speed = 3800.0f,
        float Scale = 1.0f);

private:
    UPROPERTY(VisibleAnywhere, Category="Presentation")
    TObjectPtr<USphereComponent> RootSphere;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    TObjectPtr<UStaticMeshComponent> ArrowMesh;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    TObjectPtr<UNiagaraComponent> Trail;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    TObjectPtr<UPointLightComponent> ElementLight;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
};
