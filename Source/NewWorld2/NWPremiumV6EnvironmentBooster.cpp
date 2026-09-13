#include "NWPremiumV6EnvironmentBooster.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "NWProceduralWorldManager.h"

ANWPremiumV6EnvironmentBooster::ANWPremiumV6EnvironmentBooster()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    DenseGrass = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("V6DenseGrass"));
    WildPlants = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("V6WildPlants"));
    GroundStones = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("V6GroundStones"));

    for (UHierarchicalInstancedStaticMeshComponent* C : { DenseGrass, WildPlants, GroundStones })
    {
        C->SetupAttachment(SceneRoot);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetGenerateOverlapEvents(false);
        C->SetCanEverAffectNavigation(false);
    }
    DenseGrass->SetCastShadow(false);
    DenseGrass->SetCullDistances(350, 5200);
    WildPlants->SetCastShadow(true);
    WildPlants->SetCullDistances(550, 7200);
    GroundStones->SetCastShadow(true);
    GroundStones->SetCullDistances(550, 7800);
}

void ANWPremiumV6EnvironmentBooster::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer || !GetWorld()) return;
    ScanAssets();
    PrepareExistingNatureMaterials();

    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        Populate(*It);
        return;
    }
}

void ANWPremiumV6EnvironmentBooster::ScanAssets()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Registry.GetAssets(Filter, StaticAssets);
}

UStaticMesh* ANWPremiumV6EnvironmentBooster::FindBestMesh(const TArray<FString>& Keywords, float MinDim, float MaxDim, const FString& Exclude) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* Best = nullptr;
    for (const FAssetData& Asset : StaticAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (!Exclude.IsEmpty() && Search == Exclude.ToLower()) continue;
        if (Search.Contains(TEXT("lowpoly")) || Search.Contains(TEXT("stylized")) || Search.Contains(TEXT("cartoon")) ||
            Search.Contains(TEXT("preview")) || Search.Contains(TEXT("collision")) || Search.Contains(TEXT("proxy"))) continue;

        bool bMatch = false;
        int32 Score = 0;
        for (const FString& K : Keywords) if (Search.Contains(K.ToLower())) { bMatch = true; Score += 42; }
        if (!bMatch) continue;
        if (Search.Contains(TEXT("megascans")) || Search.Contains(TEXT("quixel"))) Score += 150;
        if (Search.Contains(TEXT("realistic")) || Search.Contains(TEXT("photogram"))) Score += 90;
        if (Search.Contains(TEXT("kite")) || Search.Contains(TEXT("nature")) || Search.Contains(TEXT("forest"))) Score += 48;
        if (Search.Contains(TEXT("fab")) || Search.Contains(TEXT("pbr"))) Score += 32;
        if (Score <= BestScore) continue;

        UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
        if (!Mesh) continue;
        const FBoxSphereBounds B = Mesh->GetBounds();
        const float D = FMath::Max3(static_cast<float>(B.BoxExtent.X * 2.0), static_cast<float>(B.BoxExtent.Y * 2.0), static_cast<float>(B.BoxExtent.Z * 2.0));
        if (!FMath::IsFinite(D) || D < MinDim || D > MaxDim) continue;
        BestScore = Score;
        Best = Mesh;
    }
    return Best;
}

void ANWPremiumV6EnvironmentBooster::PrepareMeshForInstancing(UStaticMesh* Mesh) const
{
    if (!Mesh) return;
    for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
    {
        if (UMaterialInterface* Material = Slot.MaterialInterface)
        {
            // KiteDemo antigo nao traz este usage flag salvo para UE5.8. A checagem
            // garante o permutation correto em memoria antes de o HISM entrar na tela.
            Material->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes);
        }
    }
}

void ANWPremiumV6EnvironmentBooster::PrepareExistingNatureMaterials() const
{
    TArray<UHierarchicalInstancedStaticMeshComponent*> Existing;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        Existing.Reset();
        It->GetComponents<UHierarchicalInstancedStaticMeshComponent>(Existing);
        for (UHierarchicalInstancedStaticMeshComponent* HISM : Existing)
        {
            if (HISM && HISM->GetStaticMesh()) PrepareMeshForInstancing(HISM->GetStaticMesh());
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[ENV-V6] materiais de natureza existentes preparados para InstancedStaticMeshes em memoria."));
}

void ANWPremiumV6EnvironmentBooster::Populate(ANWProceduralWorldManager* Manager)
{
    if (!Manager) return;
    UStaticMesh* Grass = FindBestMesh({ TEXT("grass"), TEXT("fieldgrass"), TEXT("meadow"), TEXT("groundcover") }, 3.0f, 900.0f);
    UStaticMesh* Plant = FindBestMesh({ TEXT("fern"), TEXT("plant"), TEXT("bush"), TEXT("shrub"), TEXT("flower") }, 8.0f, 1600.0f);
    UStaticMesh* Stone = FindBestMesh({ TEXT("smallrock"), TEXT("small_rock"), TEXT("stone"), TEXT("pebble"), TEXT("boulder") }, 8.0f, 1600.0f);

    DenseGrass->SetStaticMesh(Grass);
    WildPlants->SetStaticMesh(Plant ? Plant : Grass);
    GroundStones->SetStaticMesh(Stone);
    PrepareMeshForInstancing(DenseGrass->GetStaticMesh());
    PrepareMeshForInstancing(WildPlants->GetStaticMesh());
    PrepareMeshForInstancing(GroundStones->GetStaticMesh());

    const float HalfExtent = FMath::Max(2000.0f, Manager->GetTerrainHalfExtent() - 520.0f);
    const FVector2D Spawn(900.0f, 900.0f);
    FRandomStream Random(0x56364E57);

    auto AddInstances = [&](UHierarchicalInstancedStaticMeshComponent* Component, int32 TargetCount, float TargetMin, float TargetMax, float MinNormalZ, float SpawnClear)
    {
        UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
        if (!Mesh) return 0;
        int32 Added = 0;
        for (int32 Attempt = 0; Attempt < TargetCount * 6 && Added < TargetCount; ++Attempt)
        {
            const float X = Random.FRandRange(-HalfExtent, HalfExtent);
            const float Y = Random.FRandRange(-HalfExtent, HalfExtent);
            if ((FVector2D(X, Y) - Spawn).Size() < SpawnClear) continue;
            const FVector Normal = EstimateNormal(Manager, X, Y);
            if (Normal.Z < MinNormalZ) continue;
            const float Scale = ScaleForDimension(Mesh, Random.FRandRange(TargetMin, TargetMax)) * Random.FRandRange(0.82f, 1.22f);
            const float Z = Manager->GetTerrainHeightAt(X, Y) + GroundOffset(Mesh, Scale);
            const FRotator Rot(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f);
            Component->AddInstance(FTransform(Rot, FVector(X, Y, Z), FVector(Scale)));
            ++Added;
        }
        return Added;
    };

    const int32 GrassCount = AddInstances(DenseGrass, 2100, 38.0f, 86.0f, 0.74f, 330.0f);
    const int32 PlantCount = AddInstances(WildPlants, 620, 70.0f, 180.0f, 0.70f, 470.0f);
    const int32 StoneCount = AddInstances(GroundStones, 260, 34.0f, 130.0f, 0.62f, 440.0f);

    UE_LOG(LogTemp, Warning, TEXT("[ENV-V6] camada lush pronta | grass=%d wildPlants=%d groundStones=%d | HISM/culling ativo."),
        GrassCount, PlantCount, StoneCount);
}

FVector ANWPremiumV6EnvironmentBooster::EstimateNormal(ANWProceduralWorldManager* Manager, float X, float Y) const
{
    constexpr float S = 100.0f;
    const float L = Manager->GetTerrainHeightAt(X - S, Y);
    const float R = Manager->GetTerrainHeightAt(X + S, Y);
    const float D = Manager->GetTerrainHeightAt(X, Y - S);
    const float U = Manager->GetTerrainHeightAt(X, Y + S);
    return FVector(L - R, D - U, S * 2.0f).GetSafeNormal();
}

float ANWPremiumV6EnvironmentBooster::ScaleForDimension(UStaticMesh* Mesh, float Target) const
{
    if (!Mesh) return 1.0f;
    const FBoxSphereBounds B = Mesh->GetBounds();
    const float D = FMath::Max3(static_cast<float>(B.BoxExtent.X * 2.0), static_cast<float>(B.BoxExtent.Y * 2.0), static_cast<float>(B.BoxExtent.Z * 2.0));
    return (!FMath::IsFinite(D) || D <= 1.0f) ? 1.0f : FMath::Clamp(Target / D, 0.02f, 6.0f);
}

float ANWPremiumV6EnvironmentBooster::GroundOffset(UStaticMesh* Mesh, float Scale) const
{
    if (!Mesh) return 0.0f;
    const FBoxSphereBounds B = Mesh->GetBounds();
    return -static_cast<float>(B.Origin.Z - B.BoxExtent.Z) * Scale;
}
