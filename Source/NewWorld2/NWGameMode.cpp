#include "NWGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "NWCharacter.h"
#include "NWContentPresentationManager.h"
#include "NWFabExpansionPresentationManager.h"
#include "NWLightingSafetyActor.h"
#include "NWProceduralWorldManager.h"
#include "NWWorldEventDirector.h"
#include "ProceduralMeshComponent.h"

ANWGameMode::ANWGameMode()
{
    DefaultPawnClass = ANWCharacter::StaticClass();
}

void ANWGameMode::StartPlay()
{
    EnsureWorldManager();

    if (GetWorld())
    {
        bool bHasDirector = false;
        for (TActorIterator<ANWWorldEventDirector> It(GetWorld()); It; ++It)
        {
            bHasDirector = true;
            break;
        }

        if (!bHasDirector)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            GetWorld()->SpawnActor<ANWWorldEventDirector>(ANWWorldEventDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        }

        bool bHasLightingSafety = false;
        for (TActorIterator<ANWLightingSafetyActor> It(GetWorld()); It; ++It)
        {
            bHasLightingSafety = true;
            break;
        }

        if (!bHasLightingSafety)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            GetWorld()->SpawnActor<ANWLightingSafetyActor>(ANWLightingSafetyActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        }

        bool bHasPresentationManager = false;
        for (TActorIterator<ANWContentPresentationManager> It(GetWorld()); It; ++It)
        {
            bHasPresentationManager = true;
            break;
        }

        if (!bHasPresentationManager)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            GetWorld()->SpawnActor<ANWFabExpansionPresentationManager>(ANWFabExpansionPresentationManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        }
    }

    Super::StartPlay();
}

void ANWGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer || NewPlayer->GetPawn())
    {
        return;
    }

    ANWProceduralWorldManager* Manager = EnsureWorldManager();
    const float SpawnZ = Manager ? Manager->GetTerrainHeightAt(0.0f, 0.0f) + 260.0f : 1200.0f;
    const FTransform SpawnTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, SpawnZ));

    RestartPlayerAtTransform(NewPlayer, SpawnTransform);
}

ANWProceduralWorldManager* ANWGameMode::EnsureWorldManager()
{
    if (IsValid(WorldManager))
    {
        return WorldManager;
    }

    if (!GetWorld())
    {
        return nullptr;
    }

    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        WorldManager = *It;
        if (UProceduralMeshComponent* Terrain = WorldManager->FindComponentByClass<UProceduralMeshComponent>())
        {
            Terrain->bUseAsyncCooking = false;
            Terrain->bUseComplexAsSimpleCollision = true;
            Terrain->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Terrain->SetCollisionProfileName(TEXT("BlockAll"));
        }
        return WorldManager;
    }

    const FTransform ManagerTransform(FRotator::ZeroRotator, FVector::ZeroVector);
    WorldManager = GetWorld()->SpawnActorDeferred<ANWProceduralWorldManager>(
        ANWProceduralWorldManager::StaticClass(),
        ManagerTransform,
        this,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!WorldManager)
    {
        return nullptr;
    }

    // O terreno procedural e pequeno (65x65 vertices), entao cooking sincrono e barato
    // e evita o player nascer antes da colisao fisica estar pronta.
    if (UProceduralMeshComponent* Terrain = WorldManager->FindComponentByClass<UProceduralMeshComponent>())
    {
        Terrain->bUseAsyncCooking = false;
        Terrain->bUseComplexAsSimpleCollision = true;
        Terrain->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Terrain->SetCollisionProfileName(TEXT("BlockAll"));
    }

    UGameplayStatics::FinishSpawningActor(WorldManager, ManagerTransform);
    return WorldManager;
}
