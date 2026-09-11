#include "NWGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "NWCharacter.h"
#include "NWContentPresentationManager.h"
#include "NWProceduralWorldManager.h"
#include "NWWorldEventDirector.h"

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
            GetWorld()->SpawnActor<ANWContentPresentationManager>(ANWContentPresentationManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
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
        return WorldManager;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    WorldManager = GetWorld()->SpawnActor<ANWProceduralWorldManager>(
        ANWProceduralWorldManager::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams);

    return WorldManager;
}
