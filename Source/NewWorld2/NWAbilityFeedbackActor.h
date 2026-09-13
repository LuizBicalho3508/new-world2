#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWAbilityFeedbackActor.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

/** Visual curto e deterministico para casts do vertical slice.
 * Nao depende de Niagara/Fab/shader descoberto por keyword em runtime.
 */
UCLASS()
class NEWORLD2_API ANWAbilityFeedbackActor : public AActor
{
    GENERATED_BODY()

public:
    ANWAbilityFeedbackActor();
    virtual void Tick(float DeltaSeconds) override;

    void InitializeFeedback(const FLinearColor& Color, float Radius, float DurationSeconds = 0.28f);

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> PulseMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPointLightComponent> PulseLight;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

    FLinearColor FeedbackColor = FLinearColor(0.2f, 0.55f, 1.0f, 1.0f);
    float TargetRadius = 180.0f;
    float Duration = 0.28f;
    float Elapsed = 0.0f;
};
