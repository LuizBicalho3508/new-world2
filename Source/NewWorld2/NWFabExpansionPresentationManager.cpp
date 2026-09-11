#include "NWFabExpansionPresentationManager.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NWCharacter.h"
#include "NWDungeonSite.h"
#include "NWEnemy.h"
#include "NWProceduralWorldManager.h"
#include "NWWorldEventDirector.h"
#include "Sound/SoundBase.h"

ANWFabExpansionPresentationManager::ANWFabExpansionPresentationManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.10f;
}

void ANWFabExpansionPresentationManager::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { return; }

    ScanExpansionAssets();
    ReportExpansionAssets();
    TryPlaceDesertRuinsLandmark();
}

void ANWFabExpansionPresentationManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetNetMode() == NM_DedicatedServer || !GetWorld()) { return; }

    ExpansionAccumulator += DeltaSeconds;
    if (ExpansionAccumulator < 0.35f) { return; }
    ExpansionAccumulator = 0.0f;

    UpdatePlayerWeaponSamples();
    UpdateEnemyVariants();
    UpdateDungeonDecor();
    UpdateLocalAudio();

    if (!bDesertLandmarkPlaced)
    {
        TryPlaceDesertRuinsLandmark();
    }
}

void ANWFabExpansionPresentationManager::ScanExpansionAssets()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    FARFilter StaticFilter;
    StaticFilter.PackagePaths.Add(FName(TEXT("/Game")));
    StaticFilter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    StaticFilter.bRecursivePaths = true;
    Registry.GetAssets(StaticFilter, StaticMeshAssets);

    FARFilter SkeletalFilter;
    SkeletalFilter.PackagePaths.Add(FName(TEXT("/Game")));
    SkeletalFilter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
    SkeletalFilter.bRecursivePaths = true;
    Registry.GetAssets(SkeletalFilter, SkeletalMeshAssets);

    FARFilter AnimFilter;
    AnimFilter.PackagePaths.Add(FName(TEXT("/Game")));
    AnimFilter.ClassPaths.Add(UAnimBlueprint::StaticClass()->GetClassPathName());
    AnimFilter.bRecursivePaths = true;
    Registry.GetAssets(AnimFilter, AnimBlueprintAssets);

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

    UE_LOG(LogTemp, Warning, TEXT("[FAB-EXPANSAO] catalogo: Static=%d Skeletal=%d AnimBP=%d Niagara=%d Audio=%d"),
        StaticMeshAssets.Num(), SkeletalMeshAssets.Num(), AnimBlueprintAssets.Num(), NiagaraAssets.Num(), SoundAssets.Num());
}

void ANWFabExpansionPresentationManager::ReportExpansionAssets() const
{
    auto Report = [this](const TCHAR* Label, const TArray<FString>& Keywords)
    {
        UE_LOG(LogTemp, Display, TEXT("[FAB-EXPANSAO] %-38s : %s"), Label,
            ContainsAnyAsset(Keywords) ? TEXT("DETECTADO NO PROJETO") : TEXT("ainda nao instalado em /Game"));
    };

    Report(TEXT("Orc warrior axe and shield"), { TEXT("Orc"), TEXT("Axe"), TEXT("Shield") });
    Report(TEXT("Thornblade Sword"), { TEXT("Thornblade") });
    Report(TEXT("Atmoshpheric Worlds Music"), { TEXT("Atmosh") });
    Report(TEXT("Advanced Landscape Auto Material"), { TEXT("Landscape"), TEXT("AutoMaterial") });
    Report(TEXT("Paragon Minions"), { TEXT("ParagonMinions") });
    Report(TEXT("Free Torch Fire"), { TEXT("Torch"), TEXT("Fire") });
    Report(TEXT("Free Arrow Trail"), { TEXT("Arrow"), TEXT("Trail") });
    Report(TEXT("Fantasy Desert Ruins"), { TEXT("Desert"), TEXT("Ruin") });
    Report(TEXT("Procedural Sea Waves"), { TEXT("Sea"), TEXT("Wave") });
    Report(TEXT("Ultimate Bridge Creator"), { TEXT("UltimateBridge"), TEXT("BridgeCreator") });
    Report(TEXT("Free Fantasy Weapon Sample"), { TEXT("Fantasy"), TEXT("Weapon") });
    Report(TEXT("Dark Knight Longsword"), { TEXT("DarkKnight"), TEXT("Longsword") });
    Report(TEXT("Paragon Terra"), { TEXT("ParagonTerra") });
    Report(TEXT("Dark Fantasy Statue/Pedestal"), { TEXT("Statue"), TEXT("Pedestal") });
}

void ANWFabExpansionPresentationManager::UpdatePlayerWeaponSamples()
{
    if (!GetWorld()) { return; }

    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character)) { continue; }

        const ENWWeaponType WeaponType = Character->GetActiveWeapon();
        if (WeaponType != ENWWeaponType::Greatsword && WeaponType != ENWWeaponType::DualSwords && WeaponType != ENWWeaponType::SwordShield)
        {
            continue;
        }

        UStaticMeshComponent* Right = FindStaticPresentationComponent(Character, TEXT("NW_WeaponVisual_R"));
        UStaticMeshComponent* Left = FindStaticPresentationComponent(Character, TEXT("NW_WeaponVisual_L"));
        if (!Right) { continue; }

        UStaticMesh* Sword = FindBestStaticMesh(
            { TEXT("Sword"), TEXT("Longsword"), TEXT("Long_Sword"), TEXT("Blade") },
            { TEXT("Thornblade"), TEXT("DarkKnight"), TEXT("Dark_Knight"), TEXT("FantasyWeapon"), TEXT("Fantasy_Weapon"),
              TEXT("Sample"), TEXT("PBR"), TEXT("Realistic"), TEXT("GameReady"), TEXT("Medieval"), TEXT("Metal") });

        if (Sword && Right->GetStaticMesh() != Sword)
        {
            Right->SetStaticMesh(Sword);
            Right->SetVisibility(true, true);
            UE_LOG(LogTemp, Display, TEXT("[FAB-ARMA] espada priorizada: %s"), *Sword->GetPathName());
        }

        if (Sword && WeaponType == ENWWeaponType::DualSwords && Left)
        {
            Left->SetStaticMesh(Sword);
            Left->SetVisibility(true, true);
        }

        if (WeaponType == ENWWeaponType::SwordShield && Left)
        {
            UStaticMesh* Shield = FindBestStaticMesh(
                { TEXT("Shield"), TEXT("Buckler") },
                { TEXT("Realistic"), TEXT("PBR"), TEXT("Medieval"), TEXT("Iron"), TEXT("Wood"), TEXT("Knight"), TEXT("Viking") });
            if (Shield)
            {
                Left->SetStaticMesh(Shield);
                Left->SetVisibility(true, true);
            }
        }
    }
}

void ANWFabExpansionPresentationManager::UpdateEnemyVariants()
{
    if (!GetWorld()) { return; }

    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy) || CustomizedEnemies.Contains(Enemy)) { continue; }

        USkeletalMesh* Mesh = nullptr;
        UClass* AnimClass = nullptr;
        bool bApplied = false;

        if (Enemy->IsWorldBoss() && (Enemy->GetUniqueID() % 4u) == 0u)
        {
            if (TryLoadTerra(Mesh, AnimClass) && Mesh && AnimClass)
            {
                bApplied = true;
            }
        }
        else if (!Enemy->IsWorldBoss() && Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Brute)
        {
            Mesh = FindBestSkeletalMesh(
                { TEXT("Minion") },
                { TEXT("ParagonMinions"), TEXT("Paragon"), TEXT("Enemy"), TEXT("Melee") },
                &AnimClass);

            if ((!Mesh || !AnimClass))
            {
                Mesh = FindBestSkeletalMesh(
                    { TEXT("Orc") },
                    { TEXT("Warrior"), TEXT("Axe"), TEXT("Shield"), TEXT("Animated"), TEXT("Rigged") },
                    &AnimClass);
            }
            bApplied = Mesh && AnimClass;
        }

        if (bApplied)
        {
            Enemy->GetMesh()->SetSkeletalMeshAsset(Mesh);
            Enemy->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            Enemy->GetMesh()->SetAnimInstanceClass(AnimClass);
            Enemy->GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, Enemy->IsWorldBoss() ? -105.0f : -90.0f));
            Enemy->GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
            Enemy->GetMesh()->SetRelativeScale3D(Enemy->IsWorldBoss() ? FVector(1.12f) : FVector(1.0f));
            Enemy->GetMesh()->SetVisibility(true, true);
            HideEnemyFallbackMeshes(Enemy);
            UE_LOG(LogTemp, Display, TEXT("[FAB-INIMIGO] %s -> %s"), Enemy->IsWorldBoss() ? TEXT("boss/Terra") : TEXT("minion/orc"), *Mesh->GetPathName());
        }

        CustomizedEnemies.Add(Enemy);
    }
}

void ANWFabExpansionPresentationManager::UpdateDungeonDecor()
{
    if (!GetWorld()) { return; }

    UNiagaraSystem* TorchFx = FindBestNiagara(
        { TEXT("Torch"), TEXT("Fire"), TEXT("Flame") },
        { TEXT("Torch"), TEXT("Realistic"), TEXT("Niagara") });

    UStaticMesh* DarkStatue = FindBestStaticMesh(
        { TEXT("Statue"), TEXT("Pedestal") },
        { TEXT("Dark"), TEXT("Fantasy"), TEXT("Stone"), TEXT("Ritual"), TEXT("Gothic"), TEXT("Sword"), TEXT("Crypt") });

    for (TActorIterator<ANWDungeonSite> It(GetWorld()); It; ++It)
    {
        ANWDungeonSite* Site = *It;
        if (!IsValid(Site) || Site->GetDungeonType() != ENWDungeonType::DarkCastle || DecoratedDungeons.Contains(Site)) { continue; }

        if (TorchFx)
        {
            static const FVector TorchOffsets[] = {
                FVector(1100.0f, 1100.0f, 260.0f), FVector(1100.0f, -1100.0f, 260.0f),
                FVector(-1100.0f, 1100.0f, 260.0f), FVector(-1100.0f, -1100.0f, 260.0f),
                FVector(360.0f, -1420.0f, 210.0f), FVector(-360.0f, -1420.0f, 210.0f)
            };

            for (const FVector& Offset : TorchOffsets)
            {
                UNiagaraComponent* Component = NewObject<UNiagaraComponent>(Site, MakeUniqueObjectName(Site, UNiagaraComponent::StaticClass(), FName(TEXT("NW_TorchFire"))), RF_Transient);
                if (!Component) { continue; }
                Site->AddInstanceComponent(Component);
                Component->SetupAttachment(Site->GetRootComponent());
                Component->SetAsset(TorchFx);
                Component->SetRelativeLocation(Offset);
                Component->SetAutoActivate(true);
                Component->RegisterComponent();
                Component->Activate(true);
            }
        }

        if (DarkStatue)
        {
            UStaticMeshComponent* Statue = NewObject<UStaticMeshComponent>(Site, MakeUniqueObjectName(Site, UStaticMeshComponent::StaticClass(), FName(TEXT("NW_DarkFantasyStatue"))), RF_Transient);
            if (Statue)
            {
                Site->AddInstanceComponent(Statue);
                Statue->SetupAttachment(Site->GetRootComponent());
                Statue->SetStaticMesh(DarkStatue);
                Statue->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Statue->SetRelativeLocation(FVector(0.0f, -960.0f, 25.0f));
                Statue->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
                Statue->RegisterComponent();
            }
        }

        DecoratedDungeons.Add(Site);
        UE_LOG(LogTemp, Display, TEXT("[FAB-DUNGEON] Castelo Sombrio decorado com Torch/Statue quando disponiveis."));
    }
}

void ANWFabExpansionPresentationManager::UpdateLocalAudio()
{
    if (!GetWorld()) { return; }

    ANWWorldEventDirector* Director = nullptr;
    for (TActorIterator<ANWWorldEventDirector> It(GetWorld()); It; ++It)
    {
        Director = *It;
        break;
    }
    if (!Director) { return; }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }

        ANWCharacter* Character = Cast<ANWCharacter>(PC->GetPawn());
        if (!Character) { continue; }

        const ENWBiomeType Biome = Director->GetBiomeAtLocation(Character->GetActorLocation());
        const ENWBiomeType* Previous = PreviousAudioBiomes.Find(Character);
        if (Previous && *Previous == Biome) { continue; }

        PreviousAudioBiomes.Add(Character, Biome);
        const TArray<FString> BiomeKeywords = GetBiomeMusicKeywords(Biome);
        if (USoundBase* Music = FindBestSound(BiomeKeywords, { TEXT("Atmoshpheric"), TEXT("Atmospheric"), TEXT("Worlds"), TEXT("Music"), TEXT("Fantasy"), TEXT("Loop") }))
        {
            if (ActiveBiomeMusic)
            {
                ActiveBiomeMusic->FadeOut(1.25f, 0.0f);
            }

            ActiveBiomeMusic = UGameplayStatics::SpawnSound2D(this, Music, 0.22f, 1.0f, 0.0f);
            UE_LOG(LogTemp, Display, TEXT("[FAB-MUSICA] bioma %d -> %s"), static_cast<int32>(Biome), *Music->GetPathName());
        }
        break;
    }
}

void ANWFabExpansionPresentationManager::TryPlaceDesertRuinsLandmark()
{
    if (bDesertLandmarkPlaced || !GetWorld()) { return; }

    UStaticMesh* RuinMesh = FindBestStaticMesh(
        { TEXT("Desert"), TEXT("Ruin"), TEXT("Temple") },
        { TEXT("Lost"), TEXT("Desert"), TEXT("Temple"), TEXT("Ruins"), TEXT("Fantasy"), TEXT("PBR"), TEXT("Realistic"), TEXT("Ancient") });
    if (!RuinMesh) { return; }

    ANWWorldEventDirector* Director = nullptr;
    ANWProceduralWorldManager* WorldManager = nullptr;
    for (TActorIterator<ANWWorldEventDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
    if (!Director || !WorldManager) { return; }

    FVector LandmarkLocation = FVector::ZeroVector;
    bool bFoundDesert = false;
    static const float Candidates[] = { -7600.0f, -5200.0f, -2800.0f, 2800.0f, 5200.0f, 7600.0f };
    for (const float X : Candidates)
    {
        for (const float Y : Candidates)
        {
            const FVector Probe(X, Y, 0.0f);
            if (Probe.Size2D() < 3600.0f || Director->GetBiomeAtLocation(Probe) != ENWBiomeType::Desert) { continue; }
            LandmarkLocation = Probe;
            bFoundDesert = true;
            break;
        }
        if (bFoundDesert) { break; }
    }
    if (!bFoundDesert) { return; }

    LandmarkLocation.Z = WorldManager->GetTerrainHeightAt(LandmarkLocation.X, LandmarkLocation.Y) + 20.0f;

    static const FVector Offsets[] = {
        FVector::ZeroVector, FVector(260.0f, 0.0f, 0.0f), FVector(-260.0f, 0.0f, 0.0f),
        FVector(0.0f, 260.0f, 0.0f), FVector(0.0f, -260.0f, 0.0f)
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Offsets); ++Index)
    {
        UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(TEXT("NW_DesertRuins"))), RF_Transient);
        if (!Component) { continue; }
        AddInstanceComponent(Component);
        Component->SetupAttachment(GetRootComponent());
        Component->SetStaticMesh(RuinMesh);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->RegisterComponent();
        Component->SetWorldLocation(LandmarkLocation + Offsets[Index]);
        Component->SetWorldRotation(FRotator(0.0f, Index * 67.0f, 0.0f));
        Component->SetWorldScale3D(FVector(Index == 0 ? 1.15f : 0.82f));
        DesertLandmarkParts.Add(Component);
        if (Index == 0) { DesertLandmarkCenter = Component; }
    }

    bDesertLandmarkPlaced = DesertLandmarkParts.Num() > 0;
    if (bDesertLandmarkPlaced)
    {
        UE_LOG(LogTemp, Warning, TEXT("[FAB-MUNDO] Ruinas do Deserto instaladas em %.0f, %.0f usando %s"), LandmarkLocation.X, LandmarkLocation.Y, *RuinMesh->GetPathName());
    }
}

UStaticMesh* ANWFabExpansionPresentationManager::FindBestStaticMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* BestMesh = nullptr;
    for (const FAssetData& Asset : StaticMeshAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset()))
        {
            BestScore = Score;
            BestMesh = Mesh;
        }
    }
    return BestScore > 0 ? BestMesh : nullptr;
}

USkeletalMesh* ANWFabExpansionPresentationManager::FindBestSkeletalMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, UClass** OutAnimClass) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* BestMesh = nullptr;
    UClass* BestAnimClass = nullptr;

    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }
        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh) { continue; }
        UClass* AnimClass = FindAnimationClassForMesh(Mesh, PreferredKeywords);
        if (!AnimClass) { continue; }
        BestScore = Score;
        BestMesh = Mesh;
        BestAnimClass = AnimClass;
    }

    if (OutAnimClass) { *OutAnimClass = BestAnimClass; }
    return BestMesh;
}

UClass* ANWFabExpansionPresentationManager::FindAnimationClassForMesh(USkeletalMesh* Mesh, const TArray<FString>& PreferredKeywords) const
{
    if (!Mesh || !Mesh->GetSkeleton()) { return nullptr; }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    UClass* BestClass = nullptr;
    for (const FAssetData& Asset : AnimBlueprintAssets)
    {
        UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset.GetAsset());
        if (!AnimBlueprint || AnimBlueprint->TargetSkeleton != Mesh->GetSkeleton() || !AnimBlueprint->GeneratedClass) { continue; }
        bool bPrimaryMatch = true;
        const int32 Score = ScoreAsset(Asset, {}, PreferredKeywords, bPrimaryMatch);
        if (Score > BestScore)
        {
            BestScore = Score;
            BestClass = AnimBlueprint->GeneratedClass;
        }
    }
    return BestClass;
}

UNiagaraSystem* ANWFabExpansionPresentationManager::FindBestNiagara(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    UNiagaraSystem* Best = nullptr;
    for (const FAssetData& Asset : NiagaraAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }
        if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset.GetAsset()))
        {
            BestScore = Score;
            Best = System;
        }
    }
    return BestScore > 0 ? Best : nullptr;
}

USoundBase* ANWFabExpansionPresentationManager::FindBestSound(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    USoundBase* Best = nullptr;
    for (const FAssetData& Asset : SoundAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }
        if (USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset()))
        {
            BestScore = Score;
            Best = Sound;
        }
    }
    return BestScore > 0 ? Best : nullptr;
}

int32 ANWFabExpansionPresentationManager::ScoreAsset(const FAssetData& Asset, const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, bool& bPrimaryMatch) const
{
    const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
    int32 Score = 0;
    bPrimaryMatch = PrimaryKeywords.IsEmpty();

    for (const FString& Keyword : PrimaryKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower()))
        {
            bPrimaryMatch = true;
            Score += 24;
        }
    }
    for (const FString& Keyword : PreferredKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 10; }
    }

    if (Searchable.Contains(TEXT("thornblade"))) { Score += 520; }
    if (Searchable.Contains(TEXT("darkknight")) || Searchable.Contains(TEXT("dark_knight"))) { Score += 500; }
    if (Searchable.Contains(TEXT("fantasyweapon")) || Searchable.Contains(TEXT("fantasy_weapon"))) { Score += 240; }
    if (Searchable.Contains(TEXT("paragonminions"))) { Score += 420; }
    if (Searchable.Contains(TEXT("paragonterra"))) { Score += 420; }
    if (Searchable.Contains(TEXT("arrowtrail")) || Searchable.Contains(TEXT("arrow_trail"))) { Score += 360; }
    if (Searchable.Contains(TEXT("torch")) && Searchable.Contains(TEXT("fire"))) { Score += 300; }
    if (Searchable.Contains(TEXT("desert")) && (Searchable.Contains(TEXT("ruin")) || Searchable.Contains(TEXT("temple")))) { Score += 320; }
    if (Searchable.Contains(TEXT("statue")) && Searchable.Contains(TEXT("dark"))) { Score += 280; }
    if (Searchable.Contains(TEXT("atmosh")) || Searchable.Contains(TEXT("atmospheric"))) { Score += 260; }
    if (Searchable.Contains(TEXT("music"))) { Score += 120; }
    if (Searchable.Contains(TEXT("realistic"))) { Score += 90; }
    if (Searchable.Contains(TEXT("pbr"))) { Score += 55; }
    if (Searchable.Contains(TEXT("medieval"))) { Score += 28; }
    if (Searchable.Contains(TEXT("gameready")) || Searchable.Contains(TEXT("game_ready"))) { Score += 24; }

    const bool bExplicitRealistic = Searchable.Contains(TEXT("realistic")) || Searchable.Contains(TEXT("pbr")) || Searchable.Contains(TEXT("thornblade"));
    if (Searchable.Contains(TEXT("lowpoly")) || Searchable.Contains(TEXT("low_poly")) || Searchable.Contains(TEXT("low-poly")))
    {
        Score += bExplicitRealistic ? -45 : -260;
    }
    if (Searchable.Contains(TEXT("stylized")) || Searchable.Contains(TEXT("cartoon")) || Searchable.Contains(TEXT("chibi"))) { Score -= 280; }

    return Score;
}

bool ANWFabExpansionPresentationManager::ContainsAnyAsset(const TArray<FString>& Keywords) const
{
    auto Matches = [&Keywords](const FAssetData& Asset)
    {
        const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        for (const FString& Keyword : Keywords)
        {
            if (Searchable.Contains(Keyword.ToLower())) { return true; }
        }
        return false;
    };

    for (const FAssetData& Asset : StaticMeshAssets) { if (Matches(Asset)) { return true; } }
    for (const FAssetData& Asset : SkeletalMeshAssets) { if (Matches(Asset)) { return true; } }
    for (const FAssetData& Asset : NiagaraAssets) { if (Matches(Asset)) { return true; } }
    for (const FAssetData& Asset : SoundAssets) { if (Matches(Asset)) { return true; } }
    return false;
}

UStaticMeshComponent* ANWFabExpansionPresentationManager::FindStaticPresentationComponent(ANWCharacter* Character, const FString& NameFragment) const
{
    if (!Character) { return nullptr; }
    TArray<UStaticMeshComponent*> Components;
    Character->GetComponents<UStaticMeshComponent>(Components);
    for (UStaticMeshComponent* Component : Components)
    {
        if (Component && Component->GetName().Contains(NameFragment, ESearchCase::IgnoreCase)) { return Component; }
    }
    return nullptr;
}

void ANWFabExpansionPresentationManager::HideEnemyFallbackMeshes(ANWEnemy* Enemy) const
{
    if (!Enemy) { return; }
    TArray<UStaticMeshComponent*> Components;
    Enemy->GetComponents<UStaticMeshComponent>(Components);
    for (UStaticMeshComponent* Component : Components)
    {
        if (Component) { Component->SetVisibility(false, true); }
    }
}

bool ANWFabExpansionPresentationManager::TryLoadTerra(USkeletalMesh*& OutMesh, UClass*& OutAnimClass) const
{
    OutMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ParagonTerra/Characters/Heroes/Terra/Meshes/Terra.Terra"));
    OutAnimClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/ParagonTerra/Characters/Heroes/Terra/Terra_AnimBlueprint.Terra_AnimBlueprint_C"));
    if (OutMesh && !OutAnimClass) { OutAnimClass = FindAnimationClassForMesh(OutMesh, { TEXT("Terra") }); }
    return OutMesh != nullptr;
}

TArray<FString> ANWFabExpansionPresentationManager::GetBiomeMusicKeywords(ENWBiomeType Biome) const
{
    switch (Biome)
    {
        case ENWBiomeType::Desert: return { TEXT("Desert"), TEXT("Western"), TEXT("Arab"), TEXT("World") };
        case ENWBiomeType::Snow: return { TEXT("Nordic"), TEXT("Snow"), TEXT("Ice"), TEXT("World") };
        case ENWBiomeType::Swamp: return { TEXT("Dark"), TEXT("Ambient"), TEXT("Swamp"), TEXT("World") };
        case ENWBiomeType::Volcanic: return { TEXT("Dark"), TEXT("Cinematic"), TEXT("Fire"), TEXT("World") };
        case ENWBiomeType::Haunted: return { TEXT("Dark"), TEXT("Fantasy"), TEXT("Ambient"), TEXT("World") };
        default: return { TEXT("Fantasy"), TEXT("Exploration"), TEXT("Ambient"), TEXT("World") };
    }
}
