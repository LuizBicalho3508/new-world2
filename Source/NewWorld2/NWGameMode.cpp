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
#include "NWEnemy.h"
#include "NWGameplaySafetyActor.h"
#include "NWLightingSafetyActor.h"
#include "NWPremiumGameplayDirector.h"
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

    // Uma unica sincronizacao de metadata antes do play. Nenhum sistema de combate
    // deve fazer varredura global do Asset Registry depois que o jogador assume controle.
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        const TArray<FString> PathsToScan = { TEXT("/Game") };
        Registry.ScanPathsSynchronous(PathsToScan, true);
        UE_LOG(LogTemp, Display, TEXT("[BOOT] Asset Registry /Game sincronizado antes do play."));
    }

    ANWProceduralWorldManager* RuntimeWorldManager = EnsureWorldManager();
    if (RuntimeWorldManager)
    {
        RuntimeWorldManager->DisableAutomaticEvolution();
    }

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
            GetWorld()->SpawnActor<ANWContentPresentationManager>(
                ANWContentPresentationManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
            UE_LOG(LogTemp, Display, TEXT("[VISUAL] presentation manager unico ativo; sem sobrescritas concorrentes de arma/armadura."));
        }

        bool bHasPremiumDirector = false;
        for (TActorIterator<ANWPremiumGameplayDirector> It(GetWorld()); It; ++It)
        {
            bHasPremiumDirector = true;
            break;
        }
        if (!bHasPremiumDirector)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            GetWorld()->SpawnActor<ANWPremiumGameplayDirector>(
                ANWPremiumGameplayDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        }
    }

    Super::StartPlay();
    SpawnPlaytestEncounter();
}

void ANWGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer || NewPlayer->GetPawn())
    {
        return;
    }

    ANWProceduralWorldManager* Manager = EnsureWorldManager();
    const float SpawnX = 900.0f;
    const float SpawnY = 900.0f;
    const float SpawnZ = Manager ? Manager->GetTerrainHeightAt(SpawnX, SpawnY) + 110.0f : 1200.0f;
    const FTransform SpawnTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(SpawnX, SpawnY, SpawnZ));
    RestartPlayerAtTransform(NewPlayer, SpawnTransform);
}

void ANWGameMode::SpawnPlaytestEncounter()
{
    if (!GetWorld())
    {
        return;
    }

    ANWProceduralWorldManager* Manager = EnsureWorldManager();
    if (!Manager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PLAYTEST] encontro inicial ignorado: WorldManager indisponivel."));
        return;
    }

    const FVector SpawnCenter(900.0f, 900.0f, 0.0f);
    constexpr float EncounterRadius = 1650.0f;
    int32 NearbyRegularEnemies = 0;

    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy) || Enemy->IsWorldBoss())
        {
            continue;
        }

        FVector Delta = Enemy->GetActorLocation() - SpawnCenter;
        Delta.Z = 0.0f;
        if (Delta.SizeSquared() <= FMath::Square(EncounterRadius))
        {
            ++NearbyRegularEnemies;
        }
    }

    const int32 Needed = FMath::Max(0, 3 - NearbyRegularEnemies);
    if (Needed <= 0)
    {
        UE_LOG(LogTemp, Display, TEXT("[PLAYTEST] READY | %d inimigos ja estavam proximos ao spawn."), NearbyRegularEnemies);
        return;
    }

    struct FEncounterSpawn
    {
        ENWEnemyArchetype Archetype;
        FVector2D Offset;
    };

    const FEncounterSpawn Spawns[] = {
        { ENWEnemyArchetype::Zombie, FVector2D(780.0f, 160.0f) },
        { ENWEnemyArchetype::Ghost, FVector2D(-650.0f, 520.0f) },
        { ENWEnemyArchetype::Brute, FVector2D(260.0f, -820.0f) }
    };

    int32 Spawned = 0;
    for (const FEncounterSpawn& Entry : Spawns)
    {
        if (Spawned >= Needed)
        {
            break;
        }

        const float X = SpawnCenter.X + Entry.Offset.X;
        const float Y = SpawnCenter.Y + Entry.Offset.Y;
        const float Z = Manager->GetTerrainHeightAt(X, Y) + 125.0f;
        const FVector Location(X, Y, Z);
        const FTransform Transform((SpawnCenter - FVector(X, Y, 0.0f)).Rotation(), Location);

        ANWEnemy* Enemy = GetWorld()->SpawnActorDeferred<ANWEnemy>(
            ANWEnemy::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Enemy)
        {
            continue;
        }

        Enemy->ConfigureEnemy(Entry.Archetype, false, 1);
        UGameplayStatics::FinishSpawningActor(Enemy, Transform);
        ++Spawned;
    }

    UE_LOG(LogTemp, Warning, TEXT("[PLAYTEST] READY | encontro inicial=%d | inimigos proximos existentes=%d | WASD/Mouse LMB/RMB Q/E/R G I T/Y"),
        Spawned, NearbyRegularEnemies);
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
        ANWProceduralWorldManager::StaticClass(), ManagerTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!WorldManager)
    {
        return nullptr;
    }

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
