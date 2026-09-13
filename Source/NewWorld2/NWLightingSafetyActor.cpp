#include "NWLightingSafetyActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NWProceduralWorldManager.h"
#include "NWWorldEventDirector.h"

ANWLightingSafetyActor::ANWLightingSafetyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    // O WorldEventDirector tambem atualiza luz/clima. Executamos no fim do frame
    // para que a correcao de orientacao do sol seja sempre a ultima escrita e nao
    // haja alternancia visivel entre duas rotacoes diferentes.
    PrimaryActorTick.TickInterval = 0.0f;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ANWLightingSafetyActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshReferences();
    ApplyLightingFix();
}

void ANWLightingSafetyActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!WorldManager.IsValid() || !WorldDirector.IsValid())
    {
        RefreshReferences();
    }

    ApplyLightingFix();
}

void ANWLightingSafetyActor::RefreshReferences()
{
    if (!GetWorld())
    {
        return;
    }

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

void ANWLightingSafetyActor::ApplyLightingFix()
{
    if (!WorldManager.IsValid())
    {
        return;
    }

    const float WorldTimeHours = WorldDirector.IsValid() ? WorldDirector->GetWorldTimeHours() : 9.0f;
    const float SolarAngle = (WorldTimeHours - 12.0f) / 12.0f * PI;
    const float DayFactor = FMath::Clamp(FMath::Cos(SolarAngle) * 1.15f + 0.08f, 0.025f, 1.0f);

    // 06:00 = horizonte, 12:00 = sol apontando para baixo, 18:00 = horizonte.
    // O calculo anterior fazia 09:00-12:00 apontar a luz para cima e deixava o terreno preto.
    const float SunPitch = 90.0f - (WorldTimeHours / 24.0f) * 360.0f;

    TArray<UDirectionalLightComponent*> Suns;
    WorldManager->GetComponents<UDirectionalLightComponent>(Suns);
    for (UDirectionalLightComponent* Sun : Suns)
    {
        if (!Sun)
        {
            continue;
        }

        Sun->SetWorldRotation(FRotator(SunPitch, -35.0f, 0.0f));
        Sun->SetIntensity(FMath::Max(1.25f, 8.0f * DayFactor));
        Sun->SetLightColor(DayFactor < 0.30f
            ? FLinearColor(1.0f, 0.50f, 0.30f)
            : FLinearColor(1.0f, 0.94f, 0.82f));
    }

    TArray<USkyLightComponent*> Skies;
    WorldManager->GetComponents<USkyLightComponent>(Skies);
    for (USkyLightComponent* Sky : Skies)
    {
        if (Sky)
        {
            Sky->SetIntensity(FMath::Max(0.35f, 0.18f + DayFactor * 0.82f));
        }
    }
}
