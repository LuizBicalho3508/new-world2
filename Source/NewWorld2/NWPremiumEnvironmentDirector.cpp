#include "NWPremiumEnvironmentDirector.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "NWProceduralWorldManager.h"
#include "ProceduralMeshComponent.h"

ANWPremiumEnvironmentDirector::ANWPremiumEnvironmentDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    TreesPrimary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PremiumTreesPrimary"));
    TreesSecondary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PremiumTreesSecondary"));
    GroundCover = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PremiumGroundCover"));
    Bushes = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PremiumBushes"));
    RocksPrimary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PremiumRocksPrimary"));
    RocksSecondary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PremiumRocksSecondary"));

    UHierarchicalInstancedStaticMeshComponent* Components[] = {
        TreesPrimary, TreesSecondary, GroundCover, Bushes, RocksPrimary, RocksSecondary
    };
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCullDistances(900, 14500);
    }

    TreesPrimary->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TreesSecondary->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RocksPrimary->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RocksSecondary->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TreesPrimary->SetCollisionProfileName(TEXT("BlockAll"));
    TreesSecondary->SetCollisionProfileName(TEXT("BlockAll"));
    RocksPrimary->SetCollisionProfileName(TEXT("BlockAll"));
    RocksSecondary->SetCollisionProfileName(TEXT("BlockAll"));
    TreesPrimary->SetCanEverAffectNavigation(true);
    TreesSecondary->SetCanEverAffectNavigation(true);
    RocksPrimary->SetCanEverAffectNavigation(true);
    RocksSecondary->SetCanEverAffectNavigation(true);
    TreesPrimary->SetCastShadow(true);
    TreesSecondary->SetCastShadow(true);
    RocksPrimary->SetCastShadow(true);
    RocksSecondary->SetCastShadow(true);

    GroundCover->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bushes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GroundCover->SetCanEverAffectNavigation(false);
    Bushes->SetCanEverAffectNavigation(false);
    GroundCover->SetCastShadow(false);
    Bushes->SetCastShadow(false);
    GroundCover->SetCullDistances(350, 4800);
    Bushes->SetCullDistances(550, 6800);
    RocksPrimary->SetCullDistances(800, 12000);
    RocksSecondary->SetCullDistances(800, 12000);
}

void ANWPremiumEnvironmentDirector::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer || !GetWorld()) { return; }

    ScanInstalledAssets();

    ANWProceduralWorldManager* WorldManager = nullptr;
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        WorldManager = *It;
        break;
    }

    if (!WorldManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ENV-V10] WorldManager nao encontrado; ambiente estavel nao foi montado."));
        return;
    }

    HideLegacyPrototypeDecor(WorldManager);
    ApplyGroundMaterial(WorldManager);
    BuildPremiumEnvironment(WorldManager);
}

void ANWPremiumEnvironmentDirector::ScanInstalledAssets()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    FARFilter MeshFilter;
    MeshFilter.PackagePaths.Add(FName(TEXT("/Game")));
    MeshFilter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    MeshFilter.bRecursivePaths = true;
    Registry.GetAssets(MeshFilter, StaticMeshAssets);

    FARFilter MaterialFilter;
    MaterialFilter.PackagePaths.Add(FName(TEXT("/Game")));
    MaterialFilter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
    MaterialFilter.bRecursivePaths = true;
    MaterialFilter.bRecursiveClasses = true;
    Registry.GetAssets(MaterialFilter, MaterialAssets);

    UE_LOG(LogTemp, Display, TEXT("[ENV-V10] catalogo local: StaticMesh=%d | Material=%d"), StaticMeshAssets.Num(), MaterialAssets.Num());
}

void ANWPremiumEnvironmentDirector::HideLegacyPrototypeDecor(ANWProceduralWorldManager* WorldManager) const
{
    if (!WorldManager) { return; }

    static const TSet<FName> LegacyNames = {
        FName(TEXT("TreeTrunks")), FName(TEXT("TreeCrowns")), FName(TEXT("Bushes")), FName(TEXT("Rocks")),
        FName(TEXT("Crystals")), FName(TEXT("Buildings")), FName(TEXT("Structures"))
    };

    TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
    WorldManager->GetComponents<UHierarchicalInstancedStaticMeshComponent>(Components);
    int32 Hidden = 0;
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!Component || !LegacyNames.Contains(Component->GetFName())) { continue; }
        Component->ClearInstances();
        Component->SetVisibility(false, true);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ++Hidden;
    }

    UE_LOG(LogTemp, Display, TEXT("[ENV-V10] camada prototipo removida: %d HISMs antigos desligados."), Hidden);
}

void ANWPremiumEnvironmentDirector::ApplyGroundMaterial(ANWProceduralWorldManager* WorldManager)
{
    if (!WorldManager) { return; }
    UMaterialInterface* Ground = FindBestGroundMaterial();
    if (!Ground)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ENV-V10] material PBR de solo nao encontrado; vertex-color base permanece ativo."));
        return;
    }

    if (UProceduralMeshComponent* Terrain = WorldManager->FindComponentByClass<UProceduralMeshComponent>())
    {
        Terrain->SetMaterial(0, Ground);
        UE_LOG(LogTemp, Warning, TEXT("[ENV-V10] solo PBR aplicado com UV world-tiling: %s"), *Ground->GetPathName());
    }
}

void ANWPremiumEnvironmentDirector::PrepareMeshForInstancing(UStaticMesh* Mesh) const
{
    if (!Mesh) { return; }
    for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
    {
        if (UMaterialInterface* Material = Slot.MaterialInterface)
        {
            // Only ask for the HISM permutation for the few meshes that actually
            // survived V10 selection. The old scanner touched unrelated editor/FX
            // assets and created large shader/texture work during startup.
            Material->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes);
        }
    }
}

void ANWPremiumEnvironmentDirector::BuildPremiumEnvironment(ANWProceduralWorldManager* WorldManager)
{
    if (!WorldManager) { return; }

    UStaticMesh* TreeA = FindBestNatureMesh(
        { TEXT("Tree"), TEXT("Oak"), TEXT("Pine"), TEXT("Beech"), TEXT("Hornbeam"), TEXT("Birch"), TEXT("Fir"), TEXT("Spruce"), TEXT("Maple"), TEXT("Sapling"), TEXT("Seedling") },
        { TEXT("EuropeanBeech"), TEXT("EuropeanHornbeam"), TEXT("SimpleWind"), TEXT("Sapling"), TEXT("Seedling"), TEXT("Nature"), TEXT("Realistic") },
        100.0f, 5000.0f);

    const FString TreeAPath = TreeA ? TreeA->GetPathName() : FString();
    UStaticMesh* TreeB = FindBestNatureMesh(
        { TEXT("Tree"), TEXT("Oak"), TEXT("Pine"), TEXT("Beech"), TEXT("Hornbeam"), TEXT("Birch"), TEXT("Fir"), TEXT("Spruce"), TEXT("Maple"), TEXT("Sapling"), TEXT("Seedling") },
        { TEXT("EuropeanBeech"), TEXT("EuropeanHornbeam"), TEXT("SimpleWind"), TEXT("Sapling"), TEXT("Seedling"), TEXT("Nature"), TEXT("Realistic") },
        100.0f, 5000.0f, TreeAPath);

    UStaticMesh* Grass = FindBestNatureMesh(
        { TEXT("Grass"), TEXT("GroundCover"), TEXT("Meadow"), TEXT("Fern"), TEXT("Plant") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("KiteDemo"), TEXT("Forest"), TEXT("Nature"), TEXT("Realistic") },
        3.0f, 700.0f);

    UStaticMesh* Bush = FindBestNatureMesh(
        { TEXT("Bush"), TEXT("Shrub"), TEXT("Fern"), TEXT("Plant") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("KiteDemo"), TEXT("Forest"), TEXT("Nature"), TEXT("Realistic") },
        10.0f, 1400.0f);

    UStaticMesh* RockA = FindBestNatureMesh(
        { TEXT("Rock"), TEXT("Boulder"), TEXT("Cliff"), TEXT("Stone") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("KiteDemo"), TEXT("Nature"), TEXT("Realistic"), TEXT("PBR") },
        20.0f, 5000.0f);

    const FString RockAPath = RockA ? RockA->GetPathName() : FString();
    UStaticMesh* RockB = FindBestNatureMesh(
        { TEXT("Rock"), TEXT("Boulder"), TEXT("Cliff"), TEXT("Stone") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("KiteDemo"), TEXT("Nature"), TEXT("Realistic"), TEXT("PBR") },
        20.0f, 5000.0f, RockAPath);

    for (UStaticMesh* Mesh : { TreeA, TreeB, Grass, Bush, RockA, RockB }) { PrepareMeshForInstancing(Mesh); }

    TreesPrimary->SetStaticMesh(TreeA);
    TreesSecondary->SetStaticMesh(TreeB ? TreeB : TreeA);
    GroundCover->SetStaticMesh(Grass);
    Bushes->SetStaticMesh(Bush);
    RocksPrimary->SetStaticMesh(RockA);
    RocksSecondary->SetStaticMesh(RockB ? RockB : RockA);

    UE_LOG(LogTemp, Warning, TEXT("[ENV-V10] assets | treeA=%s | treeB=%s | grass=%s | bush=%s | rockA=%s | rockB=%s"),
        TreeA ? *TreeA->GetPathName() : TEXT("NAO ENCONTRADO"),
        TreeB ? *TreeB->GetPathName() : TEXT("NAO ENCONTRADO"),
        Grass ? *Grass->GetPathName() : TEXT("DESATIVADO - sem mesh natural segura"),
        Bush ? *Bush->GetPathName() : TEXT("DESATIVADO - sem mesh natural segura"),
        RockA ? *RockA->GetPathName() : TEXT("NAO ENCONTRADO"),
        RockB ? *RockB->GetPathName() : TEXT("NAO ENCONTRADO"));

    const float HalfExtent = FMath::Max(2200.0f, WorldManager->GetTerrainHalfExtent() - 900.0f);
    FRandomStream Random(0x4E573956);

    auto AddTreeInstances = [&](UHierarchicalInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, int32 Count, int32 SeedOffset)
    {
        if (!Component || !Mesh) { return; }
        FRandomStream LocalRandom(0x4E573956 + SeedOffset);
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < Count * 8 && Added < Count; ++Attempt)
        {
            const FVector2D P = RandomPoint(LocalRandom, HalfExtent, 650.0f);
            if (!IsPlacementClear(WorldManager, P.X, P.Y, 0.10f, 230.0f)) { continue; }
            if (!IsTerrainUsable(WorldManager, P.X, P.Y, 0.78f)) { continue; }

            const float HeightTarget = LocalRandom.FRandRange(680.0f, 1220.0f);
            const float Scale = ComputeScaleForHeight(Mesh, HeightTarget) * LocalRandom.FRandRange(0.90f, 1.10f);
            const float GroundZ = WorldManager->GetTerrainHeightAt(P.X, P.Y);
            const float ZOffset = ComputeGroundOffset(Mesh, Scale);
            Component->AddInstance(FTransform(FRotator(0.0f, LocalRandom.FRandRange(0.0f, 360.0f), 0.0f), FVector(P.X, P.Y, GroundZ + ZOffset), FVector(Scale)));
            ++Added;
        }
    };

    AddTreeInstances(TreesPrimary, TreeA, TreeA ? 86 : 0, 11);
    AddTreeInstances(TreesSecondary, TreeB ? TreeB : TreeA, (TreeB || TreeA) ? 48 : 0, 29);

    if (GroundCover->GetStaticMesh())
    {
        UStaticMesh* Mesh = GroundCover->GetStaticMesh();
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < 2200 && Added < 380; ++Attempt)
        {
            const FVector2D P = RandomPoint(Random, HalfExtent, 100.0f);
            if (!IsPlacementClear(WorldManager, P.X, P.Y, 0.54f, 50.0f)) { continue; }
            if (!IsTerrainUsable(WorldManager, P.X, P.Y, 0.80f)) { continue; }
            const float Scale = ComputeScaleForMaxDimension(Mesh, Random.FRandRange(42.0f, 92.0f)) * Random.FRandRange(0.88f, 1.15f);
            const float GroundZ = WorldManager->GetTerrainHeightAt(P.X, P.Y);
            GroundCover->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(P.X, P.Y, GroundZ + ComputeGroundOffset(Mesh, Scale)), FVector(Scale)));
            ++Added;
        }
    }

    if (Bushes->GetStaticMesh())
    {
        UStaticMesh* Mesh = Bushes->GetStaticMesh();
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < 700 && Added < 85; ++Attempt)
        {
            const FVector2D P = RandomPoint(Random, HalfExtent, 420.0f);
            if (!IsPlacementClear(WorldManager, P.X, P.Y, 0.30f, 120.0f)) { continue; }
            if (!IsTerrainUsable(WorldManager, P.X, P.Y, 0.76f)) { continue; }
            const float Scale = ComputeScaleForMaxDimension(Mesh, Random.FRandRange(90.0f, 205.0f));
            const float GroundZ = WorldManager->GetTerrainHeightAt(P.X, P.Y);
            Bushes->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(P.X, P.Y, GroundZ + ComputeGroundOffset(Mesh, Scale)), FVector(Scale)));
            ++Added;
        }
    }

    auto AddRockInstances = [&](UHierarchicalInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, int32 Count, int32 SeedOffset)
    {
        if (!Component || !Mesh) { return; }
        FRandomStream LocalRandom(0x4E573958 + SeedOffset);
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < Count * 7 && Added < Count; ++Attempt)
        {
            const FVector2D P = RandomPoint(LocalRandom, HalfExtent, 580.0f);
            if (!IsPlacementClear(WorldManager, P.X, P.Y, 0.42f, 80.0f)) { continue; }
            if (!IsTerrainUsable(WorldManager, P.X, P.Y, 0.58f)) { continue; }
            const float Scale = ComputeScaleForMaxDimension(Mesh, LocalRandom.FRandRange(120.0f, 390.0f));
            const float GroundZ = WorldManager->GetTerrainHeightAt(P.X, P.Y);
            const FVector Normal = EstimateTerrainNormal(WorldManager, P.X, P.Y);
            FRotator Rotation = Normal.Rotation();
            Rotation.Pitch -= 90.0f;
            Rotation.Yaw = LocalRandom.FRandRange(0.0f, 360.0f);
            Component->AddInstance(FTransform(Rotation, FVector(P.X, P.Y, GroundZ + ComputeGroundOffset(Mesh, Scale)), FVector(Scale)));
            ++Added;
        }
    };

    AddRockInstances(RocksPrimary, RockA, RockA ? 38 : 0, 7);
    AddRockInstances(RocksSecondary, RockB ? RockB : RockA, (RockB || RockA) ? 22 : 0, 19);

    UE_LOG(LogTemp, Warning,
        TEXT("[ENV-V10] ambiente estavel pronto | trees=%d | grass=%d | bushes=%d | rocks=%d | editor-icons/heavy-forest bloqueados"),
        TreesPrimary->GetInstanceCount() + TreesSecondary->GetInstanceCount(),
        GroundCover->GetInstanceCount(), Bushes->GetInstanceCount(),
        RocksPrimary->GetInstanceCount() + RocksSecondary->GetInstanceCount());
}

UStaticMesh* ANWPremiumEnvironmentDirector::FindBestNatureMesh(
    const TArray<FString>& PrimaryKeywords,
    const TArray<FString>& PreferredKeywords,
    float MinDimension,
    float MaxDimension,
    const FString& ExcludedPath) const
{
    struct FScoredCandidate
    {
        int32 Score = 0;
        FAssetData Asset;
        FString Path;
    };

    TArray<FScoredCandidate> Candidates;
    for (const FAssetData& Asset : StaticMeshAssets)
    {
        const FString AssetPath = Asset.PackageName.ToString() + TEXT(".") + Asset.AssetName.ToString();
        if (!ExcludedPath.IsEmpty() && AssetPath == ExcludedPath) { continue; }
        const int32 Score = ScoreNatureAsset(Asset, PrimaryKeywords, PreferredKeywords);
        if (Score < 20) { continue; }
        Candidates.Add({ Score, Asset, AssetPath });
    }

    Candidates.Sort([](const FScoredCandidate& A, const FScoredCandidate& B)
    {
        if (A.Score != B.Score) { return A.Score > B.Score; }
        return A.Path < B.Path;
    });

    // Loading every improving candidate caused UE to build multiple 300-700 MB
    // tree meshes and 8K textures before the game appeared. V10 only loads the
    // best path-ranked candidates until one passes the physical bounds check.
    const int32 MaxLoads = FMath::Min(10, Candidates.Num());
    for (int32 Index = 0; Index < MaxLoads; ++Index)
    {
        UStaticMesh* Mesh = Cast<UStaticMesh>(Candidates[Index].Asset.GetAsset());
        if (!Mesh) { continue; }
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const float MaxDim = FMath::Max3(static_cast<float>(Bounds.BoxExtent.X * 2.0), static_cast<float>(Bounds.BoxExtent.Y * 2.0), static_cast<float>(Bounds.BoxExtent.Z * 2.0));
        if (!FMath::IsFinite(MaxDim) || MaxDim < MinDimension || MaxDim > MaxDimension) { continue; }
        return Mesh;
    }
    return nullptr;
}

UMaterialInterface* ANWPremiumEnvironmentDirector::FindBestGroundMaterial() const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    UMaterialInterface* Best = nullptr;

    for (const FAssetData& Asset : MaterialAssets)
    {
        const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (Searchable.Contains(TEXT("character")) || Searchable.Contains(TEXT("weapon")) || Searchable.Contains(TEXT("ui/"))) { continue; }
        if (Searchable.Contains(TEXT("landscape")) || Searchable.Contains(TEXT("layerblend"))) { continue; }

        const bool bGround = Searchable.Contains(TEXT("ground")) || Searchable.Contains(TEXT("soil")) ||
            Searchable.Contains(TEXT("dirt")) || Searchable.Contains(TEXT("forestfloor")) ||
            Searchable.Contains(TEXT("forest_floor")) || Searchable.Contains(TEXT("surface")) ||
            Searchable.Contains(TEXT("gravel"));
        if (!bGround) { continue; }

        int32 Score = 30;
        if (Searchable.Contains(TEXT("forestfloor")) || Searchable.Contains(TEXT("forest_floor")) || Searchable.Contains(TEXT("soil"))) { Score += 65; }
        if (Searchable.Contains(TEXT("grass")) || Searchable.Contains(TEXT("moss"))) { Score += 35; }
        if (Searchable.Contains(TEXT("megascans")) || Searchable.Contains(TEXT("quixel"))) { Score += 130; }
        if (Searchable.Contains(TEXT("realistic")) || Searchable.Contains(TEXT("photogram"))) { Score += 90; }
        if (Searchable.Contains(TEXT("fab")) || Searchable.Contains(TEXT("nature")) || Searchable.Contains(TEXT("forest"))) { Score += 45; }
        if (Searchable.Contains(TEXT("pbr")) || Searchable.Contains(TEXT("4k"))) { Score += 25; }
        if (Searchable.Contains(TEXT("stylized")) || Searchable.Contains(TEXT("lowpoly")) || Searchable.Contains(TEXT("cartoon"))) { Score -= 500; }
        if (Score <= BestScore) { continue; }

        if (UMaterialInterface* Material = Cast<UMaterialInterface>(Asset.GetAsset()))
        {
            BestScore = Score;
            Best = Material;
        }
    }
    return BestScore >= 70 ? Best : nullptr;
}

int32 ANWPremiumEnvironmentDirector::ScoreNatureAsset(const FAssetData& Asset, const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const
{
    const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();

    static const TCHAR* HardBlocked[] = {
        TEXT("globalfoliageactor"), TEXT("icon_"), TEXT("/icon"), TEXT("sock"), TEXT("socket"),
        TEXT("/ui/"), TEXT("editor"), TEXT("preview"), TEXT("thumbnail"), TEXT("collision"), TEXT("proxy"),
        TEXT("niagaraexamples"), TEXT("free_magic"), TEXT("paragongreystone/fx"), TEXT("/fx/"),
        TEXT("weapon"), TEXT("character"), TEXT("skeletal"), TEXT("test/"), TEXT("/demo/")
    };
    for (const TCHAR* Token : HardBlocked)
    {
        if (Searchable.Contains(Token)) { return TNumericLimits<int32>::Lowest(); }
    }

    bool bPrimary = false;
    int32 Score = 0;
    for (const FString& Keyword : PrimaryKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower())) { bPrimary = true; Score += 36; }
    }
    if (!bPrimary) { return TNumericLimits<int32>::Lowest(); }

    for (const FString& Keyword : PreferredKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 14; }
    }

    if (Searchable.Contains(TEXT("europeanbeech")) || Searchable.Contains(TEXT("european_beech"))) { Score += 125; }
    if (Searchable.Contains(TEXT("europeanhornbeam")) || Searchable.Contains(TEXT("european_hornbeam"))) { Score += 120; }
    if (Searchable.Contains(TEXT("simplewind"))) { Score += 150; }
    if (Searchable.Contains(TEXT("seedling"))) { Score += 210; }
    if (Searchable.Contains(TEXT("sapling"))) { Score += 190; }
    if (Searchable.Contains(TEXT("megascans")) || Searchable.Contains(TEXT("quixel"))) { Score += 90; }
    if (Searchable.Contains(TEXT("photogram")) || Searchable.Contains(TEXT("realistic"))) { Score += 65; }
    if (Searchable.Contains(TEXT("pbr"))) { Score += 18; }
    if (Searchable.Contains(TEXT("fab"))) { Score += 12; }

    // Full forest/impostor assets were the main V9 startup regression: the log
    // showed 300-700 MB mesh builds and 8K texture builds exceeding 1-4 GB each.
    if (Searchable.Contains(TEXT("forest_")) || Searchable.Contains(TEXT("_forest"))) { Score -= 520; }
    if (Searchable.Contains(TEXT("impostor"))) { Score -= 420; }
    if (Searchable.Contains(TEXT("nanite"))) { Score -= 120; }
    if (Searchable.Contains(TEXT("8k"))) { Score -= 180; }
    if (Searchable.Contains(TEXT("billboard"))) { Score -= 180; }

    if (Searchable.Contains(TEXT("kitedemo"))) { Score -= 35; }
    if (Searchable.Contains(TEXT("lowpoly")) || Searchable.Contains(TEXT("low_poly")) || Searchable.Contains(TEXT("low-poly"))) { Score -= 500; }
    if (Searchable.Contains(TEXT("stylized")) || Searchable.Contains(TEXT("stylised"))) { Score -= 450; }
    if (Searchable.Contains(TEXT("cartoon")) || Searchable.Contains(TEXT("toon")) || Searchable.Contains(TEXT("voxel"))) { Score -= 600; }
    return Score;
}

float ANWPremiumEnvironmentDirector::ComputeScaleForHeight(UStaticMesh* Mesh, float TargetHeight) const
{
    if (!Mesh) { return 1.0f; }
    const float Height = static_cast<float>(Mesh->GetBounds().BoxExtent.Z * 2.0);
    return (!FMath::IsFinite(Height) || Height <= 1.0f) ? 1.0f : FMath::Clamp(TargetHeight / Height, 0.02f, 8.0f);
}

float ANWPremiumEnvironmentDirector::ComputeScaleForMaxDimension(UStaticMesh* Mesh, float TargetDimension) const
{
    if (!Mesh) { return 1.0f; }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const float MaxDim = FMath::Max3(static_cast<float>(Bounds.BoxExtent.X * 2.0), static_cast<float>(Bounds.BoxExtent.Y * 2.0), static_cast<float>(Bounds.BoxExtent.Z * 2.0));
    return (!FMath::IsFinite(MaxDim) || MaxDim <= 1.0f) ? 1.0f : FMath::Clamp(TargetDimension / MaxDim, 0.02f, 8.0f);
}

float ANWPremiumEnvironmentDirector::ComputeGroundOffset(UStaticMesh* Mesh, float UniformScale) const
{
    if (!Mesh) { return 0.0f; }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    return -static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z) * UniformScale;
}

FVector ANWPremiumEnvironmentDirector::EstimateTerrainNormal(ANWProceduralWorldManager* WorldManager, float X, float Y) const
{
    return WorldManager ? WorldManager->GetTerrainNormalAt(X, Y) : FVector::UpVector;
}

bool ANWPremiumEnvironmentDirector::IsTerrainUsable(ANWProceduralWorldManager* WorldManager, float X, float Y, float MinimumNormalZ) const
{
    return EstimateTerrainNormal(WorldManager, X, Y).Z >= MinimumNormalZ;
}

bool ANWPremiumEnvironmentDirector::IsPlacementClear(ANWProceduralWorldManager* WorldManager, float X, float Y, float MaxPathInfluence, float SettlementExtra) const
{
    if (!WorldManager) { return false; }
    if (WorldManager->IsInsideSettlementClearance(X, Y, SettlementExtra)) { return false; }
    return WorldManager->GetTravelPathInfluence(X, Y) <= MaxPathInfluence;
}

FVector2D ANWPremiumEnvironmentDirector::RandomPoint(FRandomStream& Random, float HalfExtent, float ClearRadius) const
{
    for (int32 Attempt = 0; Attempt < 20; ++Attempt)
    {
        const FVector2D P(Random.FRandRange(-HalfExtent, HalfExtent), Random.FRandRange(-HalfExtent, HalfExtent));
        if (P.Size() >= ClearRadius) { return P; }
    }
    return FVector2D(ClearRadius, 0.0f);
}
