#include "NWPremiumSkyDirector.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "NWProceduralWorldManager.h"
#include "NWWorldEventDirector.h"

ANWPremiumSkyDirector::ANWPremiumSkyDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.35f;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    VolumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("PremiumVolumetricCloud"));
    VolumetricCloud->SetupAttachment(SceneRoot);
    VolumetricCloud->SetLayerBottomAltitude(1.35f);
    VolumetricCloud->SetLayerHeight(8.5f);
    VolumetricCloud->SetGroundAlbedo(FColor(104, 110, 91));
    VolumetricCloud->SetbUsePerSampleAtmosphericLightTransmittance(true);
}

void ANWPremiumSkyDirector::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer)
    {
        SetActorTickEnabled(false);
        return;
    }
    RefreshReferences();
    ConfigureClouds();
    ApplyPremiumLighting();
}

void ANWPremiumSkyDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!WorldManager.IsValid() || !WorldDirector.IsValid()) RefreshReferences();
    if (!bCloudConfigured) ConfigureClouds();
    ApplyPremiumLighting();
}

void ANWPremiumSkyDirector::RefreshReferences()
{
    if (!GetWorld()) return;
    if (!WorldManager.IsValid())
    {
        for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
    }
    if (!WorldDirector.IsValid())
    {
        for (TActorIterator<ANWWorldEventDirector> It(GetWorld()); It; ++It) { WorldDirector = *It; break; }
    }
}

void ANWPremiumSkyDirector::ConfigureClouds()
{
    if (!VolumetricCloud || bCloudConfigured) return;
    UMaterialInterface* CloudMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
    if (!CloudMaterial)
    {
        CloudMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricClouds.m_SimpleVolumetricClouds"));
    }
    if (CloudMaterial)
    {
        VolumetricCloud->SetMaterial(CloudMaterial);
        VolumetricCloud->SetVisibility(true, true);
        bCloudConfigured = true;
        UE_LOG(LogTemp, Warning, TEXT("[SKY-V6] nuvens volumetricas ativas: %s"), *CloudMaterial->GetPathName());
    }
    else UE_LOG(LogTemp, Warning, TEXT("[SKY-V6] cloud material padrao indisponivel; SkyAtmosphere permanece ativo."));
}

void ANWPremiumSkyDirector::ApplyPremiumLighting()
{
    if (!WorldManager.IsValid()) return;

    const float WorldTimeHours = WorldDirector.IsValid() ? WorldDirector->GetWorldTimeHours() : 10.0f;
    const float SolarAngle = (WorldTimeHours - 12.0f) / 12.0f * PI;
    const float DayFactor = FMath::Clamp(FMath::Cos(SolarAngle) * 1.10f + 0.18f, 0.18f, 1.0f);
    const float SunPitch = 90.0f - (WorldTimeHours / 24.0f) * 360.0f;

    TArray<UDirectionalLightComponent*> Suns;
    WorldManager->GetComponents<UDirectionalLightComponent>(Suns);
    for (UDirectionalLightComponent* Sun : Suns)
    {
        if (!Sun) continue;
        Sun->SetWorldRotation(FRotator(SunPitch, -32.0f, 0.0f));
        Sun->SetIntensity(FMath::Max(2.0f, 12.8f * DayFactor));
        Sun->SetLightColor(DayFactor < 0.32f ? FLinearColor(1.0f, 0.60f, 0.34f) : FLinearColor(1.0f, 0.975f, 0.90f));
        Sun->SetVolumetricScatteringIntensity(FMath::Lerp(1.0f, 2.05f, DayFactor));
        Sun->SetAtmosphereSunLight(true);
        Sun->SetAtmosphereSunLightIndex(0);
        Sun->SetAtmosphereSunDiskColorScale(FLinearColor(1.18f, 1.04f, 0.86f, 1.0f));
        Sun->bCastShadowsOnAtmosphere = true;
        Sun->bCastShadowsOnClouds = true;
        Sun->bCastCloudShadows = true;
        Sun->bEnableLightShaftOcclusion = true;
        Sun->OcclusionMaskDarkness = 0.48f;
        Sun->OcclusionDepthRange = 22000.0f;
        Sun->SetEnableLightShaftBloom(true);
        Sun->SetBloomScale(0.78f);
        Sun->SetBloomThreshold(0.10f);
        Sun->SetBloomMaxBrightness(55.0f);
        Sun->SetBloomTint(FColor(255, 229, 178));
    }

    TArray<USkyLightComponent*> Skies;
    WorldManager->GetComponents<USkyLightComponent>(Skies);
    for (USkyLightComponent* Sky : Skies)
    {
        if (!Sky) continue;
        Sky->SetIntensity(FMath::Lerp(0.82f, 1.82f, DayFactor));
    }

    TArray<UExponentialHeightFogComponent*> Fogs;
    WorldManager->GetComponents<UExponentialHeightFogComponent>(Fogs);
    for (UExponentialHeightFogComponent* Fog : Fogs)
    {
        if (!Fog) continue;
        Fog->SetFogDensity(0.0030f);
        Fog->SetStartDistance(55.0f);
        Fog->SetVolumetricFog(true);
        Fog->SetVolumetricFogStartDistance(0.0f);
        Fog->SetVolumetricFogNearFadeInDistance(100.0f);
        Fog->SetVolumetricFogDistance(15500.0f);
        Fog->SetVolumetricFogScatteringDistribution(0.78f);
        Fog->SetVolumetricFogAlbedo(FColor(250, 249, 242));
        Fog->SetVolumetricFogExtinctionScale(0.55f);
        Fog->SetVolumetricFogEmissive(FLinearColor(0.008f, 0.009f, 0.010f));
    }

    static bool bLogged = false;
    if (!bLogged)
    {
        bLogged = true;
        UE_LOG(LogTemp, Warning, TEXT("[SKY-V6] sol/shafts/clouds premium | sun=%.2f skylight=%.2f bloom=0.78 SSR configurado"),
            FMath::Max(2.0f, 12.8f * DayFactor), FMath::Lerp(0.82f, 1.82f, DayFactor));
    }
}
