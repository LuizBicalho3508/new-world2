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
#include "NWEnemy.h"
#include "NWEnemyAnimationDirector.h"
#include "NWEnemyVisualDirector.h"
#include "NWGameplaySafetyActor.h"
#include "NWNvidiaPerformanceDirector.h"
#include "NWPlayerEquipmentVisualDirector.h"
#include "NWPremiumEnvironmentDirector.h"
#include "NWPremiumGameplayDirector.h"
#include "NWPremiumHUDDirector.h"
#include "NWPremiumSkyDirector.h"
#include "NWPremiumV6CombatDirector.h"
#include "NWPremiumV6EnvironmentBooster.h"
#include "NWPremiumV6PlayerDirector.h"
#include "NWPremiumVFXDirector.h"
#include "NWProceduralWorldManager.h"
#include "NWStartupWarmupDirector.h"
#include "NWWorldEventDirector.h"
#include "ProceduralMeshComponent.h"

namespace
{
    void ReplaceActionMapping(UInputSettings* Settings, const FName ActionName, const TArray<FKey>& Keys)
    {
        if (!Settings) return;
        TArray<FInputActionKeyMapping> Existing;
        Settings->GetActionMappingByName(ActionName, Existing);
        for (const FInputActionKeyMapping& Mapping : Existing) Settings->RemoveActionMapping(Mapping, false);
        for (const FKey& Key : Keys) Settings->AddActionMapping(FInputActionKeyMapping(ActionName, Key), false);
    }

    void ReplaceAxisMapping(UInputSettings* Settings, const FName AxisName, const TArray<TPair<FKey, float>>& Keys)
    {
        if (!Settings) return;
        TArray<FInputAxisKeyMapping> Existing;
        Settings->GetAxisMappingByName(AxisName, Existing);
        for (const FInputAxisKeyMapping& Mapping : Existing) Settings->RemoveAxisMapping(Mapping, false);
        for (const TPair<FKey, float>& Entry : Keys) Settings->AddAxisMapping(FInputAxisKeyMapping(AxisName, Entry.Key, Entry.Value), false);
    }

    void NormalizePlayableInputMappings()
    {
        UInputSettings* Settings = UInputSettings::GetInputSettings();
        if (!Settings) return;

        // V6 tambem normaliza AXIS mappings. Hot reloads e configs antigas podiam
        // deixar A/S/D sem evento enquanto W ainda funcionava.
        ReplaceAxisMapping(Settings, TEXT("MoveForward"), { { EKeys::W, 1.0f }, { EKeys::S, -1.0f } });
        ReplaceAxisMapping(Settings, TEXT("MoveRight"), { { EKeys::D, 1.0f }, { EKeys::A, -1.0f } });
        ReplaceAxisMapping(Settings, TEXT("Turn"), { { EKeys::MouseX, 1.0f } });
        ReplaceAxisMapping(Settings, TEXT("LookUp"), { { EKeys::MouseY, -1.0f } });

        ReplaceActionMapping(Settings, TEXT("Jump"), { EKeys::SpaceBar });
        ReplaceActionMapping(Settings, TEXT("Sprint"), { EKeys::LeftShift, EKeys::RightShift });
        ReplaceActionMapping(Settings, TEXT("Crouch"), { EKeys::C, EKeys::LeftControl });
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

        Settings->ForceRebuildKeymaps();
        UE_LOG(LogTemp, Warning, TEXT("[INPUT-V6] W/S/A/D + mouse + actions reconstruidos em runtime; fallback de polling tambem ativo."));
    }

    template<typename TActorClass>
    bool HasActorOfClass(UWorld* World)
    {
        if (!World) return false;
        for (TActorIterator<TActorClass> It(World); It; ++It) if (IsValid(*It)) return true;
        return false;
    }

    template<typename TActorClass>
    TActorClass* SpawnSingletonActor(UWorld* World, const TCHAR* LogTag)
    {
        if (!World || HasActorOfClass<TActorClass>(World)) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        TActorClass* Spawned = World->SpawnActor<TActorClass>(TActorClass::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        if (Spawned)
        {
            UE_LOG(LogTemp, Display, TEXT("[BOOT] %s ativo: %s"), LogTag, *Spawned->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[BOOT] falha ao criar %s."), LogTag);
        }
        return Spawned;
    }
}

ANWGameMode::ANWGameMode()
{
    DefaultPawnClass = ANWCharacter::StaticClass();
}

void ANWGameMode::StartPlay()
{
    NormalizePlayableInputMappings();

    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        const TArray<FString> PathsToScan = { TEXT("/Game") };
        Registry.ScanPathsSynchronous(PathsToScan, true);
        UE_LOG(LogTemp, Display, TEXT("[BOOT] Asset Registry /Game sincronizado antes do play."));
    }

    ANWProceduralWorldManager* RuntimeWorldManager = EnsureWorldManager();
    if (RuntimeWorldManager) RuntimeWorldManager->DisableAutomaticEvolution();

    UWorld* World = GetWorld();
    if (World)
    {
        SpawnSingletonActor<ANWWorldEventDirector>(World, TEXT("WorldEventDirector"));
        SpawnSingletonActor<ANWGameplaySafetyActor>(World, TEXT("GameplaySafetyActor"));
        SpawnSingletonActor<ANWNvidiaPerformanceDirector>(World, TEXT("NvidiaPerformanceDirector"));
        SpawnSingletonActor<ANWPremiumSkyDirector>(World, TEXT("PremiumSkyDirector"));
        SpawnSingletonActor<ANWStartupWarmupDirector>(World, TEXT("StartupWarmupDirector"));

        SpawnSingletonActor<ANWPremiumEnvironmentDirector>(World, TEXT("PremiumEnvironmentDirector"));
        SpawnSingletonActor<ANWPremiumV6EnvironmentBooster>(World, TEXT("PremiumV6EnvironmentBooster"));
        SpawnSingletonActor<ANWEnemyVisualDirector>(World, TEXT("EnemyVisualDirector"));
        SpawnSingletonActor<ANWEnemyAnimationDirector>(World, TEXT("EnemyAnimationDirector"));

        // PlayerDirector vem antes do visual para mover os starters para a bag e
        // iniciar o personagem visualmente sem overlays equipados.
        SpawnSingletonActor<ANWPremiumV6PlayerDirector>(World, TEXT("PremiumV6PlayerDirector"));
        SpawnSingletonActor<ANWPlayerEquipmentVisualDirector>(World, TEXT("PlayerEquipmentVisualDirector"));

        SpawnSingletonActor<ANWPremiumGameplayDirector>(World, TEXT("PremiumGameplayDirector"));
        SpawnSingletonActor<ANWPremiumV6CombatDirector>(World, TEXT("PremiumV6CombatDirector"));
        SpawnSingletonActor<ANWPremiumVFXDirector>(World, TEXT("PremiumVFXDirector"));
        SpawnSingletonActor<ANWPremiumHUDDirector>(World, TEXT("PremiumHUDDirector"));

        UE_LOG(LogTemp, Warning, TEXT("[PREMIUM-V6] completo: movimento resiliente + bag comparativa + gear visual seguro + 21 skills tematicas + lush world + HUD V6 + RTX V5."));
    }

    Super::StartPlay();
    SpawnPlaytestEncounter();
}

void ANWGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer || NewPlayer->GetPawn()) return;
    ANWProceduralWorldManager* Manager = EnsureWorldManager();
    const float SpawnX = 900.0f;
    const float SpawnY = 900.0f;
    const float SpawnZ = Manager ? Manager->GetTerrainHeightAt(SpawnX, SpawnY) + 110.0f : 1200.0f;
    RestartPlayerAtTransform(NewPlayer, FTransform(FRotator::ZeroRotator, FVector(SpawnX, SpawnY, SpawnZ)));
}

void ANWGameMode::SpawnPlaytestEncounter()
{
    if (!GetWorld()) return;
    ANWProceduralWorldManager* Manager = EnsureWorldManager();
    if (!Manager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PLAYTEST] encontro inicial ignorado: WorldManager indisponivel."));
        return;
    }

    const FVector SpawnCenter(900.0f, 900.0f, 0.0f);
    constexpr float EncounterRadius = 1900.0f;
    constexpr int32 DesiredNearbyEnemies = 5;
    int32 NearbyRegularEnemies = 0;
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy) || Enemy->IsWorldBoss()) continue;
        FVector Delta = Enemy->GetActorLocation() - SpawnCenter; Delta.Z = 0.0f;
        if (Delta.SizeSquared() <= FMath::Square(EncounterRadius)) ++NearbyRegularEnemies;
    }

    const int32 Needed = FMath::Max(0, DesiredNearbyEnemies - NearbyRegularEnemies);
    if (Needed <= 0)
    {
        UE_LOG(LogTemp, Display, TEXT("[PLAYTEST] READY | %d inimigos ja estavam proximos ao spawn."), NearbyRegularEnemies);
        return;
    }

    struct FEncounterSpawn { ENWEnemyArchetype Archetype; FVector2D Offset; };
    const FEncounterSpawn Spawns[] = {
        { ENWEnemyArchetype::Zombie, FVector2D(760.0f, -360.0f) },
        { ENWEnemyArchetype::Ghost, FVector2D(900.0f, 300.0f) },
        { ENWEnemyArchetype::Brute, FVector2D(1120.0f, 0.0f) },
        { ENWEnemyArchetype::Zombie, FVector2D(1320.0f, -560.0f) },
        { ENWEnemyArchetype::Ghost, FVector2D(1480.0f, 520.0f) }
    };

    int32 Spawned = 0;
    for (const FEncounterSpawn& Entry : Spawns)
    {
        if (Spawned >= Needed) break;
        const float X = SpawnCenter.X + Entry.Offset.X;
        const float Y = SpawnCenter.Y + Entry.Offset.Y;
        const float Z = Manager->GetTerrainHeightAt(X, Y) + 125.0f;
        const FVector Location(X, Y, Z);
        const FVector FacingVector = SpawnCenter - FVector(X, Y, SpawnCenter.Z);
        const FTransform Transform(FacingVector.Rotation(), Location);
        ANWEnemy* Enemy = GetWorld()->SpawnActorDeferred<ANWEnemy>(ANWEnemy::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Enemy) continue;
        Enemy->ConfigureEnemy(Entry.Archetype, false, 1);
        UGameplayStatics::FinishSpawningActor(Enemy, Transform);
        ++Spawned;
        UE_LOG(LogTemp, Display, TEXT("[PLAYTEST-MOB] spawn archetype=%d em %s | hp=%.0f"), static_cast<int32>(Entry.Archetype), *Location.ToCompactString(), Enemy->GetMaxHealth());
    }
    UE_LOG(LogTemp, Warning, TEXT("[PLAYTEST] READY | encontro V6 novos=%d | existentes=%d | alvo=%d | skills/gear/inventory testaveis"), Spawned, NearbyRegularEnemies, DesiredNearbyEnemies);
}

ANWProceduralWorldManager* ANWGameMode::EnsureWorldManager()
{
    if (IsValid(WorldManager)) return WorldManager;
    if (!GetWorld()) return nullptr;

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
    WorldManager = GetWorld()->SpawnActorDeferred<ANWProceduralWorldManager>(ANWProceduralWorldManager::StaticClass(), ManagerTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!WorldManager) return nullptr;
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
