#include "NWGameMode.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/InputSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "NWCharacter.h"
#include "NWContentPresentationManager.h"
#include "NWFabExpansionPresentationManager.h"
#include "NWGameplaySafetyActor.h"
#include "NWLightingSafetyActor.h"
#include "NWProceduralWorldManager.h"
#include "NWWorldEventDirector.h"
#include "ProceduralMeshComponent.h"

namespace
{
    void ReplaceActionMapping(UInputSettings* Settings, const FName ActionName, const TArray<FKey>& Keys)
    {
        if (!Settings) { return; }

        TArray<FInputActionKeyMapping> Existing;
        Settings->GetActionMappingByName(ActionName, Existing);
        for (const FInputActionKeyMapping& Mapping : Existing)
        {
            Settings->RemoveActionMapping(Mapping, false);
        }

        for (const FKey& Key : Keys)
        {
            Settings->AddActionMapping(FInputActionKeyMapping(ActionName, Key), false);
        }
    }

    void NormalizePlayableInputMappings()
    {
        UInputSettings* Settings = UInputSettings::GetInputSettings();
        if (!Settings) { return; }

        ReplaceActionMapping(Settings, TEXT("Jump"), { EKeys::SpaceBar });
        ReplaceActionMapping(Settings, TEXT("Sprint"), { EKeys::LeftShift, EKeys::RightShift });
        ReplaceActionMapping(Settings, TEXT("Attack"), { EKeys::LeftMouseButton });
        ReplaceActionMapping(Settings, TEXT("Block"), { EKeys::RightMouseButton });
        ReplaceActionMapping(Settings, TEXT("Dodge"), { EKeys::LeftAlt });
        ReplaceActionMapping(Settings, TEXT("Ability1"), { EKeys::Q });
        ReplaceActionMapping(Settings, TEXT("Ability2"), { EKeys::E });
        ReplaceActionMapping(Settings, TEXT("Ability3"), { EKeys::R });
        ReplaceActionMapping(Settings, TEXT("PrimaryWeapon"), { EKeys::One });
        ReplaceActionMapping(Settings, TEXT("SecondaryWeapon"), { EKeys::Two });
        ReplaceActionMapping(Settings, TEXT("QuickSwap"), { EKeys::F });
        ReplaceActionMapping(Settings, TEXT("CyclePrimaryWeapon"), { EKeys::Z });
        ReplaceActionMapping(Settings, TEXT("CycleSecondaryWeapon"), { EKeys::X });
        ReplaceActionMapping(Settings, TEXT("CycleArrowElement"), { EKeys::V });
        ReplaceActionMapping(Settings, TEXT("PickupLoot"), { EKeys::G });
        ReplaceActionMapping(Settings, TEXT("Inventory"), { EKeys::I });
        ReplaceActionMapping(Settings, TEXT("InventoryPrev"), { EKeys::Up });
        ReplaceActionMapping(Settings, TEXT("InventoryNext"), { EKeys::Down });
        ReplaceActionMapping(Settings, TEXT("InventoryEquip"), { EKeys::Enter });
        ReplaceActionMapping(Settings, TEXT("FastTravelNext"), { EKeys::T });
        ReplaceActionMapping(Settings, TEXT("FastTravelConfirm"), { EKeys::Y });

        // R e exclusivamente habilidade 3 no modo jogavel. Regenerar o mundo e uma
        // acao de debug isolada em F10; isso impede o antigo mapping local de fazer
        // o mapa inteiro piscar/recarregar quando o jogador usa a skill R.
        ReplaceActionMapping(Settings, TEXT("RegenerateWorld"), { EKeys::F10 });

        UE_LOG(LogTemp, Display, TEXT("[INPUT] mappings jogaveis normalizados: Q/E/R, RMB, Shift, 1/2, I; epoch somente F10."));
    }
}

ANWGameMode::ANWGameMode()
{
    DefaultPawnClass = ANWCharacter::StaticClass();
}

void ANWGameMode::StartPlay()
{
    NormalizePlayableInputMappings();

    // O modo -game pode chegar aqui enquanto o Asset Registry ainda esta indexando
    // milhares de arquivos adicionados pelo Fab. Fazemos uma varredura sincrona uma
    // unica vez ANTES de criar o mundo e os presentation managers. Assim os 6k+
    // assets instalados realmente entram nos catalogos em vez de aparecer Static=0.
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        const TArray<FString> PathsToScan = { TEXT("/Game") };
        Registry.ScanPathsSynchronous(PathsToScan, true);
        UE_LOG(LogTemp, Display, TEXT("[BOOT] Asset Registry /Game sincronizado antes do play."));
    }

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

        bool bHasGameplaySafety = false;
        for (TActorIterator<ANWGameplaySafetyActor> It(GetWorld()); It; ++It)
        {
            bHasGameplaySafety = true;
            break;
        }

        if (!bHasGameplaySafety)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            GetWorld()->SpawnActor<ANWGameplaySafetyActor>(ANWGameplaySafetyActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
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
    const float SpawnZ = Manager ? Manager->GetTerrainHeightAt(0.0f, 0.0f) + 110.0f : 1200.0f;
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
            Terrain->RecreatePhysicsState();
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
