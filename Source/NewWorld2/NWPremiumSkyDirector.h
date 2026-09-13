#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWPremiumSkyDirector.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USceneComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class ANWProceduralWorldManager;
class ANWWorldEventDirector;

/**
 * V9 lighting: readable sun/sky, restrained volumetric fog and a small global
 * grade so PBR foliage/terrain keeps color without the overexposed gray V8 look.
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
    void ConfigureColorGrade();

    UPROPERTY(VisibleAnywhere, Category="Sky")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Sky")
    TObjectPtr<UVolumetricCloudComponent> VolumetricCloud;

    UPROPERTY(VisibleAnywhere, Category="Sky")
    TObjectPtr<UPostProcessComponent> PostProcess;

    TWeakObjectPtr<ANWProceduralWorldManager> WorldManager;
    TWeakObjectPtr<ANWWorldEventDirector> WorldDirector;

    bool bCloudConfigured = false;
    bool bColorGradeConfigured = false;
};
