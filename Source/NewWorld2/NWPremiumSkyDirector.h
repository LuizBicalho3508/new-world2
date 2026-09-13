#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWPremiumSkyDirector.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class USceneComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class ANWProceduralWorldManager;
class ANWWorldEventDirector;

/**
 * Iluminacao V4: sol legivel, light shafts, fog volumetrico e nuvens.
 * Substitui o safety actor antigo para nao haver dois sistemas brigando pela
 * intensidade do sol a cada frame/tick.
 */
UCLASS()
class NEWORLD2_API ANWPremiumSkyDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWPremiumSkyDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void RefreshReferences();
    void ConfigureClouds();
    void ApplyPremiumLighting();

    UPROPERTY(VisibleAnywhere, Category="Sky")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Sky")
    TObjectPtr<UVolumetricCloudComponent> VolumetricCloud;

    TWeakObjectPtr<ANWProceduralWorldManager> WorldManager;
    TWeakObjectPtr<ANWWorldEventDirector> WorldDirector;

    bool bCloudConfigured = false;
};
