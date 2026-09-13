#include "NWPremiumSkyDirector.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
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
    PrimaryActorTick.TickInterval = 0.50f;
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

    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PremiumWorldGradeV9"));
    PostProcess->SetupAttachment(SceneRoot);
    PostProcess->bUnbound = true;
    PostProcess->BlendWeight = 1.0f;
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
    ConfigureColorGrade();
    ApplyPremiumLighting();
}

void ANWPremiumSkyDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!WorldManager.IsValid() || !WorldDirector.IsValid()) RefreshReferences();
    if (!bCloudConfigured) ConfigureClouds();
    if (!bColorGradeConfigured) ConfigureColorGrade();
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
        UE_LOG(LogTemp, Warning, TEXT("[SKY-V9] nuvens volumetricas ativas: %s"), *CloudMaterial->GetPathName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SKY-V9] cloud material padrao indisponivel; SkyAtmosphere permanece ativo."));
    }
}

void ANWPremiumSkyDirector::ConfigureColorGrade()
{
    if (!PostProcess || bColorGradeConfigured) { return; }

    FPostProcessSettings& Settings = PostProcess->Settings;
    Settings.bOverride_ColorSaturation = true;
    Settings.ColorSaturation = FVector4(1.07f, 1.07f, 1.06f, 1.0f);
    Settings.bOverride_ColorContrast = true;
    Settings.ColorContrast = FVector4(1.035f, 1.035f, 1.035f, 1.0f);
    Settings.bOverride_ColorGamma = true;
    Settings.ColorGamma = FVector4(0.99f, 0.99f, 0.985f, 1.0f);
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = 0.18f;
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = 0.38f;

    bColorGradeConfigured = true;
    UE_LOG(LogTemp, Warning, TEXT("[SKY-V9] color grade moderado ativo | saturation=1.07 contrast=1.035 bloom=0.38."));
}

void ANWPremiumSkyDirector::ApplyPremiumLighting()
{
    if (!WorldManager.IsValid()) return;

    const float WorldTimeHours = WorldDirector.IsValid() ? WorldDirector->GetWorldTimeHours() : 10.0f;
    const float SolarAngle = (WorldTimeHours - 12.0f) / 12.0f * PI;
    const float DayFactor = FMath::Clamp(FMath::Cos(SolarAngle) * 1.06f + 0.17f, 0.18f, 1.0f);
    const float SunPitch = 90.0f - (WorldTimeHours / 24.0f) * 360.0f;

    TArray<UDirectionalLightComponent*> Suns;
    WorldManager->GetComponents<UDirectionalLightComponent>(Suns);
    for (UDirectionalLightComponent* Sun : Suns)
    {
        if (!Sun) continue;
        Sun->SetWorldRotation(FRotator(SunPitch, -32.0f, 0.0f));
        Sun->SetIntensity(FMath::Max(2.0f, 11.6f * DayFactor));
        Sun->SetLightColor(DayFactor < 0.32f ? FLinearColor(1.0f, 0.60f, 0.34f) : FLinearColor(1.0f, 0.965f, 0.885f));
        Sun->SetVolumetricScatteringIntensity(FMath::Lerp(0.90f, 1.65f, DayFactor));
        Sun->SetAtmosphereSunLight(true);
        Sun->SetAtmosphereSunLightIndex(0);
        Sun->SetAtmosphereSunDiskColorScale(FLinearColor(1.14f, 1.02f, 0.86f, 1.0f));
        Sun->bCastShadowsOnAtmosphere = true;
        Sun->bCastShadowsOnClouds = true;
        Sun->bCastCloudShadows = true;
        Sun->bEnableLightShaftOcclusion = true;
        Sun->OcclusionMaskDarkness = 0.52f;
        Sun->OcclusionDepthRange = 22000.0f;
        Sun->SetEnableLightShaftBloom(true);
        Sun->SetBloomScale(0.52f);
        Sun->SetBloomThreshold(0.16f);
        Sun->SetBloomMaxBrightness(42.0f);
        Sun->SetBloomTint(FColor(255, 229, 178));
    }

    TArray<USkyLightComponent*> Skies;
    WorldManager->GetComponents<USkyLightComponent>(Skies);
    for (USkyLightComponent* Sky : Skies)
    {
        if (!Sky) continue;
        Sky->SetIntensity(FMath::Lerp(0.72f, 1.46f, DayFactor));
    }

    TArray<UExponentialHeightFogComponent*> Fogs;
    WorldManager->GetComponents<UExponentialHeightFogComponent>(Fogs);
    for (UExponentialHeightFogComponent* Fog : Fogs)
    {
        if (!Fog) continue;
        Fog->SetFogDensity(0.0024f);
        Fog->SetStartDistance(140.0f);
        Fog->SetVolumetricFog(true);
        Fog->SetVolumetricFogStartDistance(0.0f);
        Fog->SetVolumetricFogNearFadeInDistance(160.0f);
        Fog->SetVolumetricFogDistance(14500.0f);
        Fog->SetVolumetricFogScatteringDistribution(0.74f);
        Fog->SetVolumetricFogAlbedo(FColor(242, 244, 236));
        Fog->SetVolumetricFogExtinctionScale(0.46f);
        Fog->SetVolumetricFogEmissive(FLinearColor(0.006f, 0.007f, 0.008f));
    }

    static bool bLogged = false;
    if (!bLogged)
    {
        bLogged = true;
        UE_LOG(LogTemp, Warning, TEXT("[SKY-V9] luz balanceada | sun=%.2f skylight=%.2f | fog menos lavado | grade ativo"),
            FMath::Max(2.0f, 11.6f * DayFactor), FMath::Lerp(0.72f, 1.46f, DayFactor));
    }
}
