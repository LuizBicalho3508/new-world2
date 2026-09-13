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
    // O WorldEventDirector atualiza ambiente em baixa frequencia. Fazer a mesma
    // correcao 60 vezes por segundo desperdicava Game/Render Thread no i7 antigo.
    // 4 Hz mantem sol e skylight coerentes sem disputa perceptivel.
    PrimaryActorTick.TickInterval = 0.25f;
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
        // Auto exposure esta desativado no perfil de playtest, portanto usamos
        // energia mais previsivel para evitar ceu estourado e terreno lavado.
        Sun->SetIntensity(FMath::Max(0.75f, 4.5f * DayFactor));
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
            Sky->SetIntensity(FMath::Max(0.22f, 0.12f + DayFactor * 0.48f));
        }
    }
}
