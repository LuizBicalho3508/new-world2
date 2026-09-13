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
    PrimaryActorTick.TickInterval = 0.25f;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    VolumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("PremiumVolumetricCloud"));
    VolumetricCloud->SetupAttachment(SceneRoot);
    VolumetricCloud->SetLayerBottomAltitude(1.6f);
    VolumetricCloud->SetLayerHeight(7.0f);
    VolumetricCloud->SetGroundAlbedo(FColor(88, 86, 78));
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
    if (!WorldManager.IsValid() || !WorldDirector.IsValid())
    {
        RefreshReferences();
    }
    if (!bCloudConfigured)
    {
        ConfigureClouds();
    }
    ApplyPremiumLighting();
}

void ANWPremiumSkyDirector::RefreshReferences()
{
    if (!GetWorld()) { return; }

    if (!WorldManager.IsValid())
    {
        for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
        {
            WorldManager = *It;
            break;
        }
    }

    if (!WorldDirector.IsValid())
    {
        for (TActorIterator<ANWWorldEventDirector> It(GetWorld()); It; ++It)
        {
            WorldDirector = *It;
            break;
        }
    }
}

void ANWPremiumSkyDirector::ConfigureClouds()
{
    if (!VolumetricCloud || bCloudConfigured) { return; }

    UMaterialInterface* CloudMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));

    if (!CloudMaterial)
    {
        CloudMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricClouds.m_SimpleVolumetricClouds"));
    }

    if (CloudMaterial)
    {
        VolumetricCloud->SetMaterial(CloudMaterial);
        VolumetricCloud->SetVisibility(true, true);
        bCloudConfigured = true;
        UE_LOG(LogTemp, Warning, TEXT("[SKY-V4] nuvens volumetricas ativas: %s"), *CloudMaterial->GetPathName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SKY-V4] material padrao de VolumetricCloud nao encontrado; ceu atmosferico permanece ativo."));
    }
}

void ANWPremiumSkyDirector::ApplyPremiumLighting()
{
    if (!WorldManager.IsValid()) { return; }

    const float WorldTimeHours = WorldDirector.IsValid() ? WorldDirector->GetWorldTimeHours() : 10.0f;
    const float SolarAngle = (WorldTimeHours - 12.0f) / 12.0f * PI;
    const float DayFactor = FMath::Clamp(FMath::Cos(SolarAngle) * 1.08f + 0.16f, 0.16f, 1.0f);
    const float SunPitch = 90.0f - (WorldTimeHours / 24.0f) * 360.0f;

    TArray<UDirectionalLightComponent*> Suns;
    WorldManager->GetComponents<UDirectionalLightComponent>(Suns);
    for (UDirectionalLightComponent* Sun : Suns)
    {
        if (!Sun) { continue; }

        Sun->SetWorldRotation(FRotator(SunPitch, -35.0f, 0.0f));
        Sun->SetIntensity(FMath::Max(1.8f, 10.5f * DayFactor));
        Sun->SetLightColor(DayFactor < 0.32f
            ? FLinearColor(1.0f, 0.62f, 0.38f)
            : FLinearColor(1.0f, 0.96f, 0.88f));
        Sun->SetVolumetricScatteringIntensity(FMath::Lerp(0.85f, 1.65f, DayFactor));
        Sun->SetAtmosphereSunLight(true);
        Sun->SetAtmosphereSunLightIndex(0);
        Sun->SetAtmosphereSunDiskColorScale(FLinearColor(1.08f, 0.98f, 0.86f, 1.0f));
        Sun->bCastShadowsOnAtmosphere = true;
        Sun->bCastShadowsOnClouds = true;
        Sun->bCastCloudShadows = true;
        Sun->bEnableLightShaftOcclusion = true;
        Sun->OcclusionMaskDarkness = 0.58f;
        Sun->OcclusionDepthRange = 18000.0f;
        Sun->SetEnableLightShaftBloom(true);
        Sun->SetBloomScale(0.52f);
        Sun->SetBloomThreshold(0.18f);
        Sun->SetBloomMaxBrightness(40.0f);
        Sun->SetBloomTint(FColor(255, 224, 178));
    }

    TArray<USkyLightComponent*> Skies;
    WorldManager->GetComponents<USkyLightComponent>(Skies);
    for (USkyLightComponent* Sky : Skies)
    {
        if (!Sky) { continue; }
        Sky->SetIntensity(FMath::Lerp(0.72f, 1.55f, DayFactor));
    }

    TArray<UExponentialHeightFogComponent*> Fogs;
    WorldManager->GetComponents<UExponentialHeightFogComponent>(Fogs);
    for (UExponentialHeightFogComponent* Fog : Fogs)
    {
        if (!Fog) { continue; }
        Fog->SetFogDensity(0.0035f);
        Fog->SetStartDistance(40.0f);
        Fog->SetVolumetricFog(true);
        Fog->SetVolumetricFogStartDistance(0.0f);
        Fog->SetVolumetricFogNearFadeInDistance(80.0f);
        Fog->SetVolumetricFogDistance(14000.0f);
        Fog->SetVolumetricFogScatteringDistribution(0.72f);
        Fog->SetVolumetricFogAlbedo(FColor(245, 247, 255));
        Fog->SetVolumetricFogExtinctionScale(0.62f);
        Fog->SetVolumetricFogEmissive(FLinearColor(0.006f, 0.008f, 0.012f));
    }

    static bool bLogged = false;
    if (!bLogged)
    {
        bLogged = true;
        UE_LOG(LogTemp, Warning, TEXT("[SKY-V4] sol premium ativo | intensidade=%.2f | skylight=%.2f | shafts+fog volumetrico+clouds"),
            FMath::Max(1.8f, 10.5f * DayFactor), FMath::Lerp(0.72f, 1.55f, DayFactor));
    }
}
