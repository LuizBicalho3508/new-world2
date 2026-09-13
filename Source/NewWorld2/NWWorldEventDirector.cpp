#include "NWWorldEventDirector.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NWCharacter.h"
#include "NWDungeonSite.h"
#include "NWEnemy.h"
#include "NWProceduralWorldManager.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ANWWorldEventDirector::ANWWorldEventDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.10f;
    bReplicates = true;
    SetReplicateMovement(false);
    NetUpdateFrequency = 2.0f;

    FastTravelNames = {
        TEXT("Refugio Central"),
        TEXT("Portao do Castelo Sombrio"),
        TEXT("Entrada da Caverna Ancestral"),
        TEXT("Fronteira Norte"),
        TEXT("Fronteira Sul")
    };
    FastTravelLocations = {
        FVector(0.0f, 0.0f, 0.0f),
        FVector(-6500.0f, -5400.0f, 0.0f),
        FVector(6450.0f, 5450.0f, 0.0f),
        FVector(0.0f, 7600.0f, 0.0f),
        FVector(0.0f, -7600.0f, 0.0f)
    };
}

void ANWWorldEventDirector::BeginPlay()
{
    Super::BeginPlay();
    DiscoverPresentationAssets();
    ApplyEnvironmentPresentation();

    if (HasAuthority())
    {
        SpawnWorldDungeons();
        SpawnRoamingUndead();
        MaintainWorldBosses();
        GetWorldTimerManager().SetTimer(WeatherTimer, this, &ANWWorldEventDirector::ChangeWeather, WeatherChangeIntervalSeconds, true, WeatherChangeIntervalSeconds);
        GetWorldTimerManager().SetTimer(BossMaintenanceTimer, this, &ANWWorldEventDirector::MaintainWorldBosses, 90.0f, true, 90.0f);
    }
}

void ANWWorldEventDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority()) { UpdateDayNight(DeltaSeconds); }

    EnvironmentAccumulator += DeltaSeconds;
    WeatherFxAccumulator += DeltaSeconds;
    if (EnvironmentAccumulator >= 0.50f)
    {
        EnvironmentAccumulator = 0.0f;
        ApplyEnvironmentPresentation();
    }

    UpdateLocalPlayerPresentation(DeltaSeconds);
}

void ANWWorldEventDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWWorldEventDirector, WorldTimeHours);
    DOREPLIFETIME(ANWWorldEventDirector, CurrentWeather);
}

FString ANWWorldEventDirector::GetFastTravelDestinationName(int32 Index) const
{
    return FastTravelNames.IsValidIndex(Index) ? FastTravelNames[Index] : FString(TEXT("Destino invalido"));
}

FVector ANWWorldEventDirector::GetFastTravelDestinationLocation(int32 Index) const
{
    return FastTravelLocations.IsValidIndex(Index) ? FastTravelLocations[Index] : FVector::ZeroVector;
}

bool ANWWorldEventDirector::IsValidFastTravelDestination(int32 Index) const
{
    return FastTravelNames.IsValidIndex(Index) && FastTravelLocations.IsValidIndex(Index);
}

void ANWWorldEventDirector::DiscoverPresentationAssets()
{
    NiagaraAssets.Reset();
    SoundAssets.Reset();

    if (!bEnableDynamicPresentationAssets)
    {
        UE_LOG(LogTemp, Display, TEXT("[PRESENTATION] modo estavel: busca automatica Niagara/Audio desativada; sem compilacao de FX aleatorio durante combate."));
        return;
    }

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    FARFilter NiagaraFilter;
    NiagaraFilter.PackagePaths.Add(FName(TEXT("/Game")));
    NiagaraFilter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
    NiagaraFilter.bRecursivePaths = true;
    Registry.GetAssets(NiagaraFilter, NiagaraAssets);

    FARFilter SoundFilter;
    SoundFilter.PackagePaths.Add(FName(TEXT("/Game")));
    SoundFilter.ClassPaths.Add(USoundBase::StaticClass()->GetClassPathName());
    SoundFilter.bRecursivePaths = true;
    SoundFilter.bRecursiveClasses = true;
    Registry.GetAssets(SoundFilter, SoundAssets);

    UE_LOG(LogTemp, Display, TEXT("[PRESENTATION] Assets dinamicos detectados: Niagara=%d | Audio=%d"), NiagaraAssets.Num(), SoundAssets.Num());
}

void ANWWorldEventDirector::UpdateDayNight(float DeltaSeconds)
{
    const float SafeDuration = FMath::Max(300.0f, FullDayDurationSeconds);
    WorldTimeHours = FMath::Fmod(WorldTimeHours + DeltaSeconds * (24.0f / SafeDuration), 24.0f);
}

void ANWWorldEventDirector::ApplyEnvironmentPresentation()
{
    if (!GetWorld()) { return; }

    // IMPORTANTE: sol e skylight tem um unico escritor: NWLightingSafetyActor.
    // Antes, este ator escrevia a rotacao oposta no mesmo frame e causava o piscar
    // observado no playtest. Aqui o diretor trata apenas neblina/clima.
    float FogDensity = 0.006f;
    switch (CurrentWeather)
    {
        case ENWWeatherType::Rain: FogDensity = 0.010f; break;
        case ENWWeatherType::Storm: FogDensity = 0.016f; break;
        case ENWWeatherType::Snow: FogDensity = 0.012f; break;
        case ENWWeatherType::Sandstorm: FogDensity = 0.018f; break;
        case ENWWeatherType::HeavyFog: FogDensity = 0.026f; break;
        default: break;
    }

    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        TArray<UExponentialHeightFogComponent*> Fogs;
        It->GetComponents<UExponentialHeightFogComponent>(Fogs);
        for (UExponentialHeightFogComponent* Fog : Fogs)
        {
            if (Fog) { Fog->SetFogDensity(FogDensity); }
        }
    }
}

void ANWWorldEventDirector::ChangeWeather()
{
    if (!HasAuthority()) { return; }

    const int32 Current = static_cast<int32>(CurrentWeather);
    const int32 Step = 1 + FMath::RandRange(0, 4);
    CurrentWeather = static_cast<ENWWeatherType>((Current + Step) % 6);
    ForceNetUpdate();
    ApplyEnvironmentPresentation();

    UE_LOG(LogTemp, Warning, TEXT("[CLIMA] Mudanca global: %s | hora %.1f"), *WeatherToString(CurrentWeather), WorldTimeHours);
}

void ANWWorldEventDirector::OnRep_Weather()
{
    ApplyEnvironmentPresentation();
}

ENWBiomeType ANWWorldEventDirector::GetBiomeAtLocation(const FVector& Location) const
{
    const int32 CellX = FMath::FloorToInt(Location.X / FMath::Max(1000.0f, BiomeCellSize));
    const int32 CellY = FMath::FloorToInt(Location.Y / FMath::Max(1000.0f, BiomeCellSize));
    uint32 Hash = HashCombine(GetTypeHash(CellX), GetTypeHash(CellY));
    Hash = HashCombine(Hash, 0x4E573242u);
    return static_cast<ENWBiomeType>(Hash % 6u);
}

ENWWeatherType ANWWorldEventDirector::GetEffectiveWeatherForBiome(ENWBiomeType Biome) const
{
    switch (Biome)
    {
        case ENWBiomeType::Desert:
            if (CurrentWeather == ENWWeatherType::Rain || CurrentWeather == ENWWeatherType::Snow) { return ENWWeatherType::Sandstorm; }
            break;
        case ENWBiomeType::Snow:
            if (CurrentWeather == ENWWeatherType::Rain || CurrentWeather == ENWWeatherType::Storm) { return ENWWeatherType::Snow; }
            break;
        case ENWBiomeType::Swamp:
            if (CurrentWeather == ENWWeatherType::Clear) { return ENWWeatherType::HeavyFog; }
            break;
        case ENWBiomeType::Haunted:
            if (CurrentWeather == ENWWeatherType::Clear || CurrentWeather == ENWWeatherType::Rain) { return ENWWeatherType::HeavyFog; }
            break;
        default:
            break;
    }
    return CurrentWeather;
}

void ANWWorldEventDirector::UpdateLocalPlayerPresentation(float DeltaSeconds)
{
    if (!GetWorld()) { return; }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }

        ANWCharacter* Character = Cast<ANWCharacter>(PC->GetPawn());
        if (!Character) { continue; }

        if (bEnableDynamicPresentationAssets)
        {
            DetectAbilityUse(Character);
        }

        const ENWBiomeType Biome = GetBiomeAtLocation(Character->GetActorLocation());
        const ENWWeatherType EffectiveWeather = GetEffectiveWeatherForBiome(Biome);

        const ENWBiomeType* PreviousBiome = PreviousBiomes.Find(Character);
        if (!PreviousBiome || *PreviousBiome != Biome)
        {
            PreviousBiomes.Add(Character, Biome);
            UE_LOG(LogTemp, Display, TEXT("[BIOMA] Entrou em: %s"), *BiomeToString(Biome));

            if (bEnableDynamicPresentationAssets)
            {
                if (USoundBase* Ambient = FindBestSound({ BiomeToString(Biome), TEXT("Ambience"), TEXT("Ambient") }))
                {
                    UGameplayStatics::PlaySound2D(this, Ambient, 0.24f);
                }
            }
        }

        const ENWWeatherType* PreviousWeather = PreviousEffectiveWeather.Find(Character);
        const bool bWeatherChanged = !PreviousWeather || *PreviousWeather != EffectiveWeather;
        if (bWeatherChanged)
        {
            PreviousEffectiveWeather.Add(Character, EffectiveWeather);
            if (bEnableDynamicPresentationAssets)
            {
                PlayWeatherPresentation(Character, EffectiveWeather);
            }
        }
        else if (bEnableDynamicPresentationAssets && WeatherFxAccumulator >= 8.0f && EffectiveWeather != ENWWeatherType::Clear)
        {
            PlayWeatherPresentation(Character, EffectiveWeather);
        }
    }

    if (WeatherFxAccumulator >= 8.0f) { WeatherFxAccumulator = 0.0f; }
}

void ANWWorldEventDirector::DetectAbilityUse(ANWCharacter* Character)
{
    if (!bEnableDynamicPresentationAssets || !Character) { return; }

    const ENWWeaponType ActiveWeapon = Character->GetActiveWeapon();
    ENWWeaponType* PreviousWeapon = PreviousWeapons.Find(Character);
    TArray<float>* Previous = PreviousAbilityCooldowns.Find(Character);
    if (!Previous || !PreviousWeapon || *PreviousWeapon != ActiveWeapon)
    {
        TArray<float> Initial;
        Initial.SetNumZeroed(3);
        for (int32 Index = 0; Index < 3; ++Index) { Initial[Index] = Character->GetAbilityCooldownRemaining(Index); }
        PreviousAbilityCooldowns.Add(Character, Initial);
        PreviousWeapons.Add(Character, ActiveWeapon);
        return;
    }

    for (int32 Index = 0; Index < 3; ++Index)
    {
        const float Current = Character->GetAbilityCooldownRemaining(Index);
        if (Current > (*Previous)[Index] + 0.35f && Current > 0.45f)
        {
            PlayAbilityPresentation(Character, Index);
        }
        (*Previous)[Index] = Current;
    }
}

TArray<FString> ANWWorldEventDirector::GetAbilityKeywords(ENWWeaponType Weapon, int32 AbilityIndex) const
{
    if (AbilityIndex == 2)
    {
        return { TEXT("Heal"), TEXT("Holy"), TEXT("Life"), TEXT("Aura"), TEXT("Nature"), TEXT("Buff") };
    }

    switch (Weapon)
    {
        case ENWWeaponType::Staff:
            return AbilityIndex == 0
                ? TArray<FString>{ TEXT("Arcane"), TEXT("Magic"), TEXT("Projectile"), TEXT("Lightning"), TEXT("Fire") }
                : TArray<FString>{ TEXT("Explosion"), TEXT("AoE"), TEXT("Arcane"), TEXT("Ice"), TEXT("Lightning") };
        case ENWWeaponType::Greatsword: return { TEXT("Sword"), TEXT("Slash"), TEXT("Impact"), TEXT("Shockwave"), TEXT("Earth") };
        case ENWWeaponType::DualSwords: return { TEXT("Slash"), TEXT("Blade"), TEXT("Spin"), TEXT("Wind"), TEXT("Blood") };
        case ENWWeaponType::SwordShield: return { TEXT("Shield"), TEXT("Impact"), TEXT("Holy"), TEXT("Guard"), TEXT("Shockwave") };
        case ENWWeaponType::Daggers: return { TEXT("Shadow"), TEXT("Blood"), TEXT("Poison"), TEXT("Blade"), TEXT("Slash") };
        case ENWWeaponType::Bow: return { TEXT("Arrow"), TEXT("Projectile"), TEXT("Wind"), TEXT("Nature"), TEXT("Rain") };
        case ENWWeaponType::Firearm: return { TEXT("Muzzle"), TEXT("Gun"), TEXT("Fire"), TEXT("Smoke"), TEXT("Explosion") };
        default: return { TEXT("Magic"), TEXT("Impact") };
    }
}

void ANWWorldEventDirector::PlayAbilityPresentation(ANWCharacter* Character, int32 AbilityIndex)
{
    if (!bEnableDynamicPresentationAssets || !Character) { return; }

    TArray<FString> Keywords = GetAbilityKeywords(Character->GetActiveWeapon(), AbilityIndex);
    if (Character->GetActiveWeapon() == ENWWeaponType::Bow)
    {
        Keywords.Add(Character->GetArrowElementLabel());
        switch (Character->GetArrowElement())
        {
            case ENWArrowElement::Fire: Keywords.Add(TEXT("Fire")); Keywords.Add(TEXT("Flame")); break;
            case ENWArrowElement::Poison: Keywords.Add(TEXT("Poison")); Keywords.Add(TEXT("Venom")); break;
            case ENWArrowElement::Lightning: Keywords.Add(TEXT("Lightning")); Keywords.Add(TEXT("Electric")); break;
            case ENWArrowElement::Frost: Keywords.Add(TEXT("Frost")); Keywords.Add(TEXT("Ice")); break;
            default: break;
        }
    }

    const FNWWeaponDefinition WeaponDef = NWCombat::GetWeaponDefinition(Character->GetActiveWeapon());
    const bool bArea = WeaponDef.Abilities.IsValidIndex(AbilityIndex) && WeaponDef.Abilities[AbilityIndex].Kind == ENWAbilityKind::AreaDamage;
    const FVector Origin = Character->GetActorLocation() + Character->GetActorForwardVector() * (bArea ? 180.0f : 110.0f) + FVector(0.0f, 0.0f, 82.0f);

    if (UNiagaraSystem* Effect = FindBestNiagara(Keywords))
    {
        const float ScaleValue = bArea ? 2.0f : (AbilityIndex == 2 ? 1.5f : 1.25f);
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Origin, Character->GetActorRotation(), FVector(ScaleValue));
    }

    TArray<FString> SoundKeywords = Keywords;
    SoundKeywords.Add(AbilityIndex == 2 ? TEXT("Heal") : TEXT("Cast"));
    if (USoundBase* Sound = FindBestSound(SoundKeywords))
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, Origin, 0.65f, AbilityIndex == 2 ? 1.06f : 1.0f);
    }
}

void ANWWorldEventDirector::PlayWeatherPresentation(ANWCharacter* Character, ENWWeatherType EffectiveWeather)
{
    if (!bEnableDynamicPresentationAssets || !Character || EffectiveWeather == ENWWeatherType::Clear) { return; }

    TArray<FString> Keywords;
    switch (EffectiveWeather)
    {
        case ENWWeatherType::Rain: Keywords = { TEXT("Rain"), TEXT("Weather") }; break;
        case ENWWeatherType::Storm: Keywords = { TEXT("Storm"), TEXT("Rain"), TEXT("Lightning") }; break;
        case ENWWeatherType::Snow: Keywords = { TEXT("Snow"), TEXT("Blizzard") }; break;
        case ENWWeatherType::Sandstorm: Keywords = { TEXT("Sand"), TEXT("Dust"), TEXT("Storm") }; break;
        case ENWWeatherType::HeavyFog: Keywords = { TEXT("Fog"), TEXT("Mist"), TEXT("Smoke") }; break;
        default: break;
    }

    if (UNiagaraSystem* Effect = FindBestNiagara(Keywords))
    {
        const FVector Location = Character->GetActorLocation() + FVector(0.0f, 0.0f, 450.0f);
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Location, FRotator::ZeroRotator, FVector(2.5f));
    }
}

UNiagaraSystem* ANWWorldEventDirector::FindBestNiagara(const TArray<FString>& Keywords) const
{
    if (!bEnableDynamicPresentationAssets) { return nullptr; }

    int32 BestScore = 0;
    FAssetData BestAsset;
    for (const FAssetData& Asset : NiagaraAssets)
    {
        const FString Searchable = Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString();
        int32 Score = 0;
        for (const FString& Keyword : Keywords)
        {
            if (Searchable.Contains(Keyword, ESearchCase::IgnoreCase)) { Score += 10; }
        }
        if (Score > BestScore)
        {
            BestScore = Score;
            BestAsset = Asset;
        }
    }
    return BestScore >= 20 ? Cast<UNiagaraSystem>(BestAsset.GetAsset()) : nullptr;
}

USoundBase* ANWWorldEventDirector::FindBestSound(const TArray<FString>& Keywords) const
{
    if (!bEnableDynamicPresentationAssets) { return nullptr; }

    int32 BestScore = 0;
    FAssetData BestAsset;
    for (const FAssetData& Asset : SoundAssets)
    {
        const FString Searchable = Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString();
        int32 Score = 0;
        for (const FString& Keyword : Keywords)
        {
            if (Searchable.Contains(Keyword, ESearchCase::IgnoreCase)) { Score += 10; }
        }
        if (Score > BestScore)
        {
            BestScore = Score;
            BestAsset = Asset;
        }
    }
    return BestScore >= 20 ? Cast<USoundBase>(BestAsset.GetAsset()) : nullptr;
}

void ANWWorldEventDirector::SpawnWorldDungeons()
{
    if (!HasAuthority() || !GetWorld()) { return; }

    ANWProceduralWorldManager* WorldManager = nullptr;
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        WorldManager = *It;
        break;
    }

    const struct FDungeonSpawn
    {
        ENWDungeonType Type;
        FVector2D XY;
        int32 Seed;
        int32 Tier;
    } Spawns[] = {
        { ENWDungeonType::DarkCastle, FVector2D(-7200.0f, -6200.0f), 88421, 3 },
        { ENWDungeonType::AncientCave, FVector2D(7150.0f, 6250.0f), 42177, 3 }
    };

    for (const FDungeonSpawn& Spawn : Spawns)
    {
        const float Z = WorldManager ? WorldManager->GetTerrainHeightAt(Spawn.XY.X, Spawn.XY.Y) : 0.0f;
        const FTransform Transform(FRotator::ZeroRotator, FVector(Spawn.XY.X, Spawn.XY.Y, Z + 30.0f));
        ANWDungeonSite* Site = GetWorld()->SpawnActorDeferred<ANWDungeonSite>(
            ANWDungeonSite::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Site) { continue; }

        // Configura antes de BeginPlay para evitar a antiga populacao default ser criada
        // e destruida no mesmo frame antes da configuracao real da dungeon.
        Site->ConfigureDungeon(Spawn.Type, Spawn.Seed, Spawn.Tier);
        UGameplayStatics::FinishSpawningActor(Site, Transform);
        DungeonSites.Add(Site);
    }
}

void ANWWorldEventDirector::SpawnRoamingUndead()
{
    if (!HasAuthority() || !GetWorld() || RoamingEnemyCount <= 0) { return; }

    ANWProceduralWorldManager* WorldManager = nullptr;
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }

    FRandomStream Random(0x5A17 + FMath::RoundToInt(WorldTimeHours * 100.0f));
    for (int32 Index = 0; Index < RoamingEnemyCount; ++Index)
    {
        const ENWEnemyArchetype Archetype = Index < FMath::CeilToInt(RoamingEnemyCount * 0.60f)
            ? ENWEnemyArchetype::Zombie
            : ENWEnemyArchetype::Ghost;
        const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
        const float Radius = Random.FRandRange(3400.0f, 8500.0f);
        const float X = FMath::Cos(Angle) * Radius;
        const float Y = FMath::Sin(Angle) * Radius;
        const float Z = WorldManager ? WorldManager->GetTerrainHeightAt(X, Y) : 0.0f;
        const FTransform Transform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(X, Y, Z + 130.0f));

        ANWEnemy* Enemy = GetWorld()->SpawnActorDeferred<ANWEnemy>(
            ANWEnemy::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Enemy) { continue; }
        Enemy->ConfigureEnemy(Archetype, false, 1);
        UGameplayStatics::FinishSpawningActor(Enemy, Transform);
        RoamingEnemies.Add(Enemy);
    }

    UE_LOG(LogTemp, Display, TEXT("[MOBS] %d zumbis/fantasmas roaming criados dentro do budget do playtest."), RoamingEnemies.Num());
}

void ANWWorldEventDirector::MaintainWorldBosses()
{
    if (!HasAuthority()) { return; }
    WorldBosses.RemoveAll([](const TObjectPtr<ANWEnemy>& Enemy) { return !IsValid(Enemy); });
    while (WorldBosses.Num() < DesiredWorldBossCount)
    {
        SpawnOneWorldBoss();
    }
}

void ANWWorldEventDirector::SpawnOneWorldBoss()
{
    if (!GetWorld()) { return; }

    ANWProceduralWorldManager* WorldManager = nullptr;
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }

    const int32 Seed = FMath::RoundToInt(GetWorld()->GetTimeSeconds() * 100.0f) + WorldBosses.Num() * 7919 + 771;
    FRandomStream Random(Seed);
    const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
    const float Radius = Random.FRandRange(5200.0f, 8600.0f);
    const float X = FMath::Cos(Angle) * Radius;
    const float Y = FMath::Sin(Angle) * Radius;
    const float Z = WorldManager ? WorldManager->GetTerrainHeightAt(X, Y) : 0.0f;
    const ENWEnemyArchetype Archetype = static_cast<ENWEnemyArchetype>(Random.RandRange(0, 2));
    const int32 Tier = Random.RandRange(2, 4);
    const FTransform Transform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(X, Y, Z + 160.0f));

    ANWEnemy* Boss = GetWorld()->SpawnActorDeferred<ANWEnemy>(
        ANWEnemy::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Boss) { return; }

    Boss->ConfigureEnemy(Archetype, true, Tier);
    UGameplayStatics::FinishSpawningActor(Boss, Transform);
    WorldBosses.Add(Boss);
    UE_LOG(LogTemp, Warning, TEXT("[WORLD BOSS] novo boss tier %d surgiu em %.0f, %.0f."), Tier, X, Y);
}

FString ANWWorldEventDirector::BiomeToString(ENWBiomeType Biome) const
{
    switch (Biome)
    {
        case ENWBiomeType::Desert: return TEXT("Deserto");
        case ENWBiomeType::Snow: return TEXT("Neve");
        case ENWBiomeType::Swamp: return TEXT("Pantano");
        case ENWBiomeType::Volcanic: return TEXT("Vulcanico");
        case ENWBiomeType::Haunted: return TEXT("Amaldicoado");
        default: return TEXT("Floresta Temperada");
    }
}

FString ANWWorldEventDirector::WeatherToString(ENWWeatherType Weather) const
{
    switch (Weather)
    {
        case ENWWeatherType::Rain: return TEXT("Chuva");
        case ENWWeatherType::Storm: return TEXT("Tempestade");
        case ENWWeatherType::Snow: return TEXT("Nevasca");
        case ENWWeatherType::Sandstorm: return TEXT("Tempestade de Areia");
        case ENWWeatherType::HeavyFog: return TEXT("Neblina Densa");
        default: return TEXT("Ceu Limpo");
    }
}
