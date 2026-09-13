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

    auto ConfigureNatureComponent = [this](const TCHAR* Name, bool bCollision)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name);
        Component->SetupAttachment(SceneRoot);
        Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        if (bCollision)
        {
            Component->SetCollisionProfileName(TEXT("BlockAll"));
        }
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(bCollision);
        Component->SetCullDistances(900, 14500);
        return Component;
    };

    TreesPrimary = ConfigureNatureComponent(TEXT("PremiumTreesPrimary"), true);
    TreesSecondary = ConfigureNatureComponent(TEXT("PremiumTreesSecondary"), true);
    GroundCover = ConfigureNatureComponent(TEXT("PremiumGroundCover"), false);
    Bushes = ConfigureNatureComponent(TEXT("PremiumBushes"), false);
    RocksPrimary = ConfigureNatureComponent(TEXT("PremiumRocksPrimary"), true);
    RocksSecondary = ConfigureNatureComponent(TEXT("PremiumRocksSecondary"), true);

    GroundCover->SetCullDistances(500, 6500);
    Bushes->SetCullDistances(700, 8500);
    RocksPrimary->SetCullDistances(800, 13000);
    RocksSecondary->SetCullDistances(800, 13000);
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
        UE_LOG(LogTemp, Warning, TEXT("[ENV-PREMIUM] WorldManager nao encontrado; ambiente realista nao foi montado."));
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

    UE_LOG(LogTemp, Display, TEXT("[ENV-PREMIUM] catalogo local: StaticMesh=%d | Material=%d"), StaticMeshAssets.Num(), MaterialAssets.Num());
}

void ANWPremiumEnvironmentDirector::HideLegacyPrototypeDecor(ANWProceduralWorldManager* WorldManager) const
{
    if (!WorldManager) { return; }

    static const TSet<FName> LegacyNames = {
        TEXT("TreeTrunks"), TEXT("TreeCrowns"), TEXT("Bushes"), TEXT("Rocks"),
        TEXT("Crystals"), TEXT("Buildings"), TEXT("Structures")
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

    UE_LOG(LogTemp, Display, TEXT("[ENV-PREMIUM] decoracao prototipo removida: %d componentes de primitives desligados."), Hidden);
}

void ANWPremiumEnvironmentDirector::ApplyGroundMaterial(ANWProceduralWorldManager* WorldManager)
{
    if (!WorldManager) { return; }
    UMaterialInterface* Ground = FindBestGroundMaterial();
    if (!Ground)
    {
        UE_LOG(LogTemp, Display, TEXT("[ENV-PREMIUM] material de solo realista nao encontrado; terreno base mantido sem inserir material aleatorio."));
        return;
    }

    if (UProceduralMeshComponent* Terrain = WorldManager->FindComponentByClass<UProceduralMeshComponent>())
    {
        Terrain->SetMaterial(0, Ground);
        UE_LOG(LogTemp, Display, TEXT("[ENV-PREMIUM] solo realista: %s"), *Ground->GetPathName());
    }
}

void ANWPremiumEnvironmentDirector::BuildPremiumEnvironment(ANWProceduralWorldManager* WorldManager)
{
    if (!WorldManager) { return; }

    UStaticMesh* TreeA = FindBestNatureMesh(
        { TEXT("Tree"), TEXT("Oak"), TEXT("Pine"), TEXT("Beech"), TEXT("Birch"), TEXT("Fir"), TEXT("Spruce"), TEXT("Maple") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("Fab"), TEXT("Forest"), TEXT("Nature"), TEXT("Realistic"), TEXT("PBR"), TEXT("Nanite") },
        150.0f, 9000.0f);

    const FString TreeAPath = TreeA ? TreeA->GetPathName() : FString();
    UStaticMesh* TreeB = FindBestNatureMesh(
        { TEXT("Tree"), TEXT("Oak"), TEXT("Pine"), TEXT("Beech"), TEXT("Birch"), TEXT("Fir"), TEXT("Spruce"), TEXT("Maple") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("Fab"), TEXT("Forest"), TEXT("Nature"), TEXT("Realistic"), TEXT("PBR"), TEXT("Nanite") },
        150.0f, 9000.0f, TreeAPath);

    UStaticMesh* Grass = FindBestNatureMesh(
        { TEXT("Grass"), TEXT("GroundCover"), TEXT("Meadow"), TEXT("Fern"), TEXT("Foliage"), TEXT("Plant") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("Fab"), TEXT("Forest"), TEXT("Nature"), TEXT("Realistic"), TEXT("PBR") },
        3.0f, 900.0f);

    UStaticMesh* Bush = FindBestNatureMesh(
        { TEXT("Bush"), TEXT("Shrub"), TEXT("Fern"), TEXT("Plant"), TEXT("Foliage") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("Fab"), TEXT("Forest"), TEXT("Nature"), TEXT("Realistic"), TEXT("PBR") },
        10.0f, 1800.0f);

    UStaticMesh* RockA = FindBestNatureMesh(
        { TEXT("Rock"), TEXT("Boulder"), TEXT("Cliff"), TEXT("Stone") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("Fab"), TEXT("Nature"), TEXT("Realistic"), TEXT("PBR"), TEXT("Nanite") },
        20.0f, 7000.0f);

    const FString RockAPath = RockA ? RockA->GetPathName() : FString();
    UStaticMesh* RockB = FindBestNatureMesh(
        { TEXT("Rock"), TEXT("Boulder"), TEXT("Cliff"), TEXT("Stone") },
        { TEXT("Megascans"), TEXT("Quixel"), TEXT("Fab"), TEXT("Nature"), TEXT("Realistic"), TEXT("PBR"), TEXT("Nanite") },
        20.0f, 7000.0f, RockAPath);

    TreesPrimary->SetStaticMesh(TreeA);
    TreesSecondary->SetStaticMesh(TreeB ? TreeB : TreeA);
    GroundCover->SetStaticMesh(Grass ? Grass : Bush);
    Bushes->SetStaticMesh(Bush ? Bush : Grass);
    RocksPrimary->SetStaticMesh(RockA);
    RocksSecondary->SetStaticMesh(RockB ? RockB : RockA);

    UE_LOG(LogTemp, Warning, TEXT("[ENV-PREMIUM] assets escolhidos | treeA=%s | treeB=%s | grass=%s | bush=%s | rockA=%s | rockB=%s"),
        TreeA ? *TreeA->GetPathName() : TEXT("NAO ENCONTRADO"),
        TreeB ? *TreeB->GetPathName() : TEXT("NAO ENCONTRADO"),
        Grass ? *Grass->GetPathName() : TEXT("NAO ENCONTRADO"),
        Bush ? *Bush->GetPathName() : TEXT("NAO ENCONTRADO"),
        RockA ? *RockA->GetPathName() : TEXT("NAO ENCONTRADO"),
        RockB ? *RockB->GetPathName() : TEXT("NAO ENCONTRADO"));

    const float HalfExtent = FMath::Max(2200.0f, WorldManager->GetTerrainHalfExtent() - 900.0f);
    const FVector2D SpawnCenter(900.0f, 900.0f);
    FRandomStream Random(0x4E573250);

    auto AddTreeInstances = [&](UHierarchicalInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, int32 Count, int32 SeedOffset)
    {
        if (!Component || !Mesh) { return; }
        FRandomStream LocalRandom(0x4E573250 + SeedOffset);
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < Count * 5 && Added < Count; ++Attempt)
        {
            FVector2D P = RandomPoint(LocalRandom, HalfExtent, 720.0f);
            if ((P - SpawnCenter).Size() < 650.0f) { continue; }
            if (!IsTerrainUsable(WorldManager, P.X, P.Y, 0.70f)) { continue; }

            const float HeightTarget = LocalRandom.FRandRange(760.0f, 1550.0f);
            const float Scale = ComputeScaleForHeight(Mesh, HeightTarget) * LocalRandom.FRandRange(0.88f, 1.12f);
            const float GroundZ = WorldManager->GetTerrainHeightAt(P.X, P.Y);
            const float ZOffset = ComputeGroundOffset(Mesh, Scale);
            const FRotator Rotation(0.0f, LocalRandom.FRandRange(0.0f, 360.0f), 0.0f);
            Component->AddInstance(FTransform(Rotation, FVector(P.X, P.Y, GroundZ + ZOffset), FVector(Scale)));
            ++Added;
        }
    };

    AddTreeInstances(TreesPrimary, TreeA, TreeA ? 150 : 0, 11);
    AddTreeInstances(TreesSecondary, TreeB ? TreeB : TreeA, (TreeB || TreeA) ? 95 : 0, 29);

    if (GroundCover->GetStaticMesh())
    {
        UStaticMesh* Mesh = GroundCover->GetStaticMesh();
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < 4200 && Added < 1050; ++Attempt)
        {
            FVector2D P = RandomPoint(Random, HalfExtent, 120.0f);
            if (!IsTerrainUsable(WorldManager, P.X, P.Y, 0.76f)) { continue; }
            const float Scale = ComputeScaleForMaxDimension(Mesh, Random.FRandRange(45.0f, 105.0f)) * Random.FRandRange(0.85f, 1.20f);
            const float GroundZ = WorldManager->GetTerrainHeightAt(P.X, P.Y);
            const float ZOffset = ComputeGroundOffset(Mesh, Scale);
            GroundCover->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(P.X, P.Y, GroundZ + ZOffset), FVector(Scale)));
            ++Added;
        }
    }

    if (Bushes->GetStaticMesh())
    {
        UStaticMesh* Mesh = Bushes->GetStaticMesh();
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < 1000 && Added < 220; ++Attempt)
        {
            FVector2D P = RandomPoint(Random, HalfExtent, 480.0f);
            if ((P - SpawnCenter).Size() < 520.0f) { continue; }
            if (!IsTerrainUsable(WorldManager, P.X, P.Y, 0.72f)) { continue; }
            const float Scale = ComputeScaleForMaxDimension(Mesh, Random.FRandRange(95.0f, 220.0f));
            const float GroundZ = WorldManager->GetTerrainHeightAt(P.X, P.Y);
            const float ZOffset = ComputeGroundOffset(Mesh, Scale);
            Bushes->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(P.X, P.Y, GroundZ + ZOffset), FVector(Scale)));
            ++Added;
        }
    }

    auto AddRockInstances = [&](UHierarchicalInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, int32 Count, int32 SeedOffset)
    {
        if (!Component || !Mesh) { return; }
        FRandomStream LocalRandom(0x4E573252 + SeedOffset);
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < Count * 5 && Added < Count; ++Attempt)
        {
            FVector2D P = RandomPoint(LocalRandom, HalfExtent, 620.0f);
            if ((P - SpawnCenter).Size() < 620.0f) { continue; }
            if (!IsTerrainUsable(WorldManager, P.X, P.Y, 0.62f)) { continue; }
            const float Scale = ComputeScaleForMaxDimension(Mesh, LocalRandom.FRandRange(120.0f, 520.0f));
            const float GroundZ = WorldManager->GetTerrainHeightAt(P.X, P.Y);
            const float ZOffset = ComputeGroundOffset(Mesh, Scale);
            const FVector Normal = EstimateTerrainNormal(WorldManager, P.X, P.Y);
            FRotator Rotation = Normal.Rotation();
            Rotation.Pitch -= 90.0f;
            Rotation.Yaw = LocalRandom.FRandRange(0.0f, 360.0f);
            Component->AddInstance(FTransform(Rotation, FVector(P.X, P.Y, GroundZ + ZOffset), FVector(Scale)));
            ++Added;
        }
    };

    AddRockInstances(RocksPrimary, RockA, RockA ? 80 : 0, 7);
    AddRockInstances(RocksSecondary, RockB ? RockB : RockA, (RockB || RockA) ? 55 : 0, 19);

    UE_LOG(LogTemp, Warning, TEXT("[ENV-PREMIUM] mundo natural pronto | trees=%d | grass=%d | bushes=%d | rocks=%d"),
        TreesPrimary->GetInstanceCount() + TreesSecondary->GetInstanceCount(),
        GroundCover->GetInstanceCount(),
        Bushes->GetInstanceCount(),
        RocksPrimary->GetInstanceCount() + RocksSecondary->GetInstanceCount());
}

UStaticMesh* ANWPremiumEnvironmentDirector::FindBestNatureMesh(
    const TArray<FString>& PrimaryKeywords,
    const TArray<FString>& PreferredKeywords,
    float MinDimension,
    float MaxDimension,
    const FString& ExcludedPath) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* BestMesh = nullptr;

    for (const FAssetData& Asset : StaticMeshAssets)
    {
        const FString AssetPath = Asset.GetObjectPathString();
        if (!ExcludedPath.IsEmpty() && AssetPath == ExcludedPath) { continue; }

        const int32 Score = ScoreNatureAsset(Asset, PrimaryKeywords, PreferredKeywords);
        if (Score <= BestScore || Score < 20) { continue; }

        UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
        if (!Mesh) { continue; }

        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const float MaxDim = FMath::Max3(
            static_cast<float>(Bounds.BoxExtent.X * 2.0),
            static_cast<float>(Bounds.BoxExtent.Y * 2.0),
            static_cast<float>(Bounds.BoxExtent.Z * 2.0));
        if (!FMath::IsFinite(MaxDim) || MaxDim < MinDimension || MaxDim > MaxDimension) { continue; }

        BestScore = Score;
        BestMesh = Mesh;
    }

    return BestMesh;
}

UMaterialInterface* ANWPremiumEnvironmentDirector::FindBestGroundMaterial() const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    UMaterialInterface* Best = nullptr;

    for (const FAssetData& Asset : MaterialAssets)
    {
        const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        const bool bGround = Searchable.Contains(TEXT("ground")) || Searchable.Contains(TEXT("soil")) ||
            Searchable.Contains(TEXT("dirt")) || Searchable.Contains(TEXT("forestfloor")) ||
            Searchable.Contains(TEXT("forest_floor")) || Searchable.Contains(TEXT("grass_ground"));
        if (!bGround) { continue; }

        int32 Score = 30;
        if (Searchable.Contains(TEXT("megascans")) || Searchable.Contains(TEXT("quixel"))) { Score += 120; }
        if (Searchable.Contains(TEXT("realistic")) || Searchable.Contains(TEXT("photogram"))) { Score += 90; }
        if (Searchable.Contains(TEXT("fab")) || Searchable.Contains(TEXT("nature")) || Searchable.Contains(TEXT("forest"))) { Score += 45; }
        if (Searchable.Contains(TEXT("pbr")) || Searchable.Contains(TEXT("4k"))) { Score += 25; }
        if (Searchable.Contains(TEXT("stylized")) || Searchable.Contains(TEXT("lowpoly")) || Searchable.Contains(TEXT("cartoon"))) { Score -= 400; }
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
    bool bPrimary = false;
    int32 Score = 0;

    for (const FString& Keyword : PrimaryKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower()))
        {
            bPrimary = true;
            Score += 36;
        }
    }
    if (!bPrimary) { return TNumericLimits<int32>::Lowest(); }

    for (const FString& Keyword : PreferredKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 14; }
    }

    if (Searchable.Contains(TEXT("megascans")) || Searchable.Contains(TEXT("quixel"))) { Score += 150; }
    if (Searchable.Contains(TEXT("photogram")) || Searchable.Contains(TEXT("realistic"))) { Score += 95; }
    if (Searchable.Contains(TEXT("nanite"))) { Score += 35; }
    if (Searchable.Contains(TEXT("pbr")) || Searchable.Contains(TEXT("4k"))) { Score += 24; }
    if (Searchable.Contains(TEXT("fab"))) { Score += 18; }

    if (Searchable.Contains(TEXT("lowpoly")) || Searchable.Contains(TEXT("low_poly")) || Searchable.Contains(TEXT("low-poly"))) { Score -= 500; }
    if (Searchable.Contains(TEXT("stylized")) || Searchable.Contains(TEXT("stylised"))) { Score -= 450; }
    if (Searchable.Contains(TEXT("cartoon")) || Searchable.Contains(TEXT("toon")) || Searchable.Contains(TEXT("voxel"))) { Score -= 600; }
    if (Searchable.Contains(TEXT("collision")) || Searchable.Contains(TEXT("proxy")) || Searchable.Contains(TEXT("preview"))) { Score -= 300; }

    return Score;
}

float ANWPremiumEnvironmentDirector::ComputeScaleForHeight(UStaticMesh* Mesh, float TargetHeight) const
{
    if (!Mesh) { return 1.0f; }
    const float Height = static_cast<float>(Mesh->GetBounds().BoxExtent.Z * 2.0);
    if (!FMath::IsFinite(Height) || Height <= 1.0f) { return 1.0f; }
    return FMath::Clamp(TargetHeight / Height, 0.02f, 8.0f);
}

float ANWPremiumEnvironmentDirector::ComputeScaleForMaxDimension(UStaticMesh* Mesh, float TargetDimension) const
{
    if (!Mesh) { return 1.0f; }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const float MaxDim = FMath::Max3(
        static_cast<float>(Bounds.BoxExtent.X * 2.0),
        static_cast<float>(Bounds.BoxExtent.Y * 2.0),
        static_cast<float>(Bounds.BoxExtent.Z * 2.0));
    if (!FMath::IsFinite(MaxDim) || MaxDim <= 1.0f) { return 1.0f; }
    return FMath::Clamp(TargetDimension / MaxDim, 0.02f, 8.0f);
}

float ANWPremiumEnvironmentDirector::ComputeGroundOffset(UStaticMesh* Mesh, float UniformScale) const
{
    if (!Mesh) { return 0.0f; }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const float LocalBottom = static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z);
    return -LocalBottom * UniformScale;
}

FVector ANWPremiumEnvironmentDirector::EstimateTerrainNormal(ANWProceduralWorldManager* WorldManager, float X, float Y) const
{
    if (!WorldManager) { return FVector::UpVector; }
    constexpr float Step = 120.0f;
    const float Left = WorldManager->GetTerrainHeightAt(X - Step, Y);
    const float Right = WorldManager->GetTerrainHeightAt(X + Step, Y);
    const float Down = WorldManager->GetTerrainHeightAt(X, Y - Step);
    const float Up = WorldManager->GetTerrainHeightAt(X, Y + Step);
    return FVector(Left - Right, Down - Up, Step * 2.0f).GetSafeNormal();
}

bool ANWPremiumEnvironmentDirector::IsTerrainUsable(ANWProceduralWorldManager* WorldManager, float X, float Y, float MinimumNormalZ) const
{
    return EstimateTerrainNormal(WorldManager, X, Y).Z >= MinimumNormalZ;
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
