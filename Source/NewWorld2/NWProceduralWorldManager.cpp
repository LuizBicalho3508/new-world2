#include "NWProceduralWorldManager.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "NWCivilian.h"
#include "NWEnemy.h"
#include "NWSettlementCore.h"
#include "PCGComponent.h"
#include "PCGGraphInterface.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    float Smooth01(float Value)
    {
        const float T = FMath::Clamp(Value, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }
}

ANWProceduralWorldManager::ANWProceduralWorldManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
    TerrainMesh->SetupAttachment(SceneRoot);
    TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
    TerrainMesh->bUseAsyncCooking = true;

    TreeTrunks = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TreeTrunks"));
    TreeTrunks->SetupAttachment(SceneRoot);
    TreeTrunks->SetCollisionProfileName(TEXT("BlockAll"));

    TreeCrowns = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TreeCrowns"));
    TreeCrowns->SetupAttachment(SceneRoot);
    TreeCrowns->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Bushes = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Bushes"));
    Bushes->SetupAttachment(SceneRoot);
    Bushes->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Rocks = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Rocks"));
    Rocks->SetupAttachment(SceneRoot);
    Rocks->SetCollisionProfileName(TEXT("BlockAll"));

    Crystals = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Crystals"));
    Crystals->SetupAttachment(SceneRoot);
    Crystals->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Crystals->SetCollisionResponseToAllChannels(ECR_Ignore);
    Crystals->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    Buildings = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Buildings"));
    Buildings->SetupAttachment(SceneRoot);
    Buildings->SetCollisionProfileName(TEXT("BlockAll"));

    Structures = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Structures"));
    Structures->SetupAttachment(SceneRoot);
    Structures->SetCollisionProfileName(TEXT("BlockAll"));

    RuntimePCG = CreateDefaultSubobject<UPCGComponent>(TEXT("RuntimePCG"));
    RuntimePCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateAtRuntime;

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(SceneRoot);
    SunLight->SetRelativeRotation(FRotator(-42.0f, -35.0f, 0.0f));
    SunLight->SetIntensity(8.0f);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->SetIntensity(0.75f);

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(SceneRoot);

    HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
    HeightFog->SetupAttachment(SceneRoot);
    HeightFog->SetFogDensity(0.006f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (CylinderMesh.Succeeded()) { TreeTrunks->SetStaticMesh(CylinderMesh.Object); }
    if (ConeMesh.Succeeded()) { TreeCrowns->SetStaticMesh(ConeMesh.Object); }
    if (SphereMesh.Succeeded()) { Bushes->SetStaticMesh(SphereMesh.Object); Crystals->SetStaticMesh(SphereMesh.Object); }
    if (CubeMesh.Succeeded()) { Rocks->SetStaticMesh(CubeMesh.Object); Buildings->SetStaticMesh(CubeMesh.Object); Structures->SetStaticMesh(CubeMesh.Object); }
}

void ANWProceduralWorldManager::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoDiscoverInstalledFreeAssets)
    {
        TryApplyInstalledFreeWorldAssets();
    }
    else
    {
        ApplySafeFallbackMaterials();
        UE_LOG(LogTemp, Display, TEXT("[VISUAL] fallback base seguro; PremiumEnvironmentDirector assume a camada visual final."));
    }

    ConfigureRuntimePCG();
    BuildWorld();

    UE_LOG(LogTemp, Warning,
        TEXT("[WORLD-V9] macro mundo HIBRIDO fixo | terrain=%dx%d cell=%.0f amplitude=%.0f | trilhas e nucleos nivelados | epoch altera eventos, nao geografia."),
        TerrainResolution, TerrainResolution, TerrainCellSize, TerrainAmplitude);

    if (HasAuthority())
    {
        if (EvolutionIntervalSeconds > 0.0f)
        {
            GetWorldTimerManager().SetTimer(EvolutionTimer, this, &ANWProceduralWorldManager::AdvanceEpoch, EvolutionIntervalSeconds, true);
        }

        if (InvasionIntervalSeconds > 0.0f)
        {
            GetWorldTimerManager().SetTimer(
                InvasionTimer,
                this,
                &ANWProceduralWorldManager::SpawnInvasionWave,
                InvasionIntervalSeconds,
                true,
                InvasionIntervalSeconds);
        }
    }
}

UMaterialInstanceDynamic* ANWProceduralWorldManager::CreateFallbackMaterial(const FLinearColor& Color, const FName Name)
{
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!BaseMaterial) { return nullptr; }

    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this, Name);
    if (MID) { MID->SetVectorParameterValue(FName(TEXT("Color")), Color); }
    return MID;
}

void ANWProceduralWorldManager::ApplySafeFallbackMaterials()
{
    TerrainFallbackMaterial = CreateFallbackMaterial(FLinearColor(0.17f, 0.245f, 0.105f, 1.0f), TEXT("MID_NW_Terrain"));
    TrunkFallbackMaterial = CreateFallbackMaterial(FLinearColor(0.20f, 0.085f, 0.035f, 1.0f), TEXT("MID_NW_Trunk"));
    FoliageFallbackMaterial = CreateFallbackMaterial(FLinearColor(0.045f, 0.21f, 0.06f, 1.0f), TEXT("MID_NW_Foliage"));
    RockFallbackMaterial = CreateFallbackMaterial(FLinearColor(0.24f, 0.26f, 0.27f, 1.0f), TEXT("MID_NW_Rock"));
    CrystalFallbackMaterial = CreateFallbackMaterial(FLinearColor(0.08f, 0.42f, 0.70f, 1.0f), TEXT("MID_NW_Crystal"));
    BuildingFallbackMaterial = CreateFallbackMaterial(FLinearColor(0.30f, 0.18f, 0.10f, 1.0f), TEXT("MID_NW_Building"));
    StructureFallbackMaterial = CreateFallbackMaterial(FLinearColor(0.20f, 0.22f, 0.24f, 1.0f), TEXT("MID_NW_Structure"));

    if (TerrainFallbackMaterial) { TerrainMesh->SetMaterial(0, TerrainFallbackMaterial); }
    if (TrunkFallbackMaterial) { TreeTrunks->SetMaterial(0, TrunkFallbackMaterial); }
    if (FoliageFallbackMaterial)
    {
        TreeCrowns->SetMaterial(0, FoliageFallbackMaterial);
        Bushes->SetMaterial(0, FoliageFallbackMaterial);
    }
    if (RockFallbackMaterial) { Rocks->SetMaterial(0, RockFallbackMaterial); }
    if (CrystalFallbackMaterial) { Crystals->SetMaterial(0, CrystalFallbackMaterial); }
    if (BuildingFallbackMaterial) { Buildings->SetMaterial(0, BuildingFallbackMaterial); }
    if (StructureFallbackMaterial) { Structures->SetMaterial(0, StructureFallbackMaterial); }
}

void ANWProceduralWorldManager::ConfigureRuntimePCG()
{
    if (!RuntimePCG) { return; }

    RuntimePCG->SetIsPartitioned(bEnableRuntimePartitionedPCG);
    RuntimePCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateAtRuntime;
    RuntimePCG->Seed = GetMacroWorldSeed();

    UPCGGraphInterface* Graph = RuntimePCGGraph.LoadSynchronous();
    if (Graph)
    {
        RuntimePCG->SetGraphLocal(Graph);
        RuntimePCG->GenerateLocal(true);
        UE_LOG(LogTemp, Display, TEXT("[PCG-V9] micro-detail ativo, particionado=%s, macro seed=%d"),
            bEnableRuntimePartitionedPCG ? TEXT("sim") : TEXT("nao"), GetMacroWorldSeed());
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[PCG-V9] sem grafo runtime: macro fixo C++ + HISM premium; zero regeneracao procedural pesada."));
    }
}

void ANWProceduralWorldManager::TryApplyInstalledFreeWorldAssets()
{
    const TArray<FName> NatureRoots = {
        FName(TEXT("/Game/EuropeanBeech")), FName(TEXT("/Game/EuropeanHornbeam")),
        FName(TEXT("/Game/Megascans")), FName(TEXT("/Game/Quixel")), FName(TEXT("/Game/KiteDemo"))
    };

    if (UStaticMesh* Tree = FindInstalledStaticMesh(NatureRoots, { TEXT("Tree"), TEXT("Beech"), TEXT("Hornbeam"), TEXT("Oak"), TEXT("Pine") }))
    {
        TreeTrunks->SetStaticMesh(Tree);
        TreeCrowns->SetVisibility(false, true);
        bUsingRealisticTreeMesh = true;
    }
    if (UStaticMesh* Bush = FindInstalledStaticMesh(NatureRoots, { TEXT("Bush"), TEXT("Shrub"), TEXT("Fern"), TEXT("GroundPlant") })) { Bushes->SetStaticMesh(Bush); }
    if (UStaticMesh* Rock = FindInstalledStaticMesh(NatureRoots, { TEXT("Rock"), TEXT("Boulder"), TEXT("Cliff") })) { Rocks->SetStaticMesh(Rock); }

    const TArray<FName> BuildingRoots = {
        FName(TEXT("/Game/ElderBoom")), FName(TEXT("/Game/MedievalVillage")), FName(TEXT("/Game/Medieval")), FName(TEXT("/Game/Village"))
    };
    if (UStaticMesh* Building = FindInstalledStaticMesh(BuildingRoots, { TEXT("House"), TEXT("Building"), TEXT("Cottage"), TEXT("Hut") }))
    {
        Buildings->SetStaticMesh(Building);
        bUsingRealisticBuildingMesh = true;
    }
}

UStaticMesh* ANWProceduralWorldManager::FindInstalledStaticMesh(const TArray<FName>& Roots, const TArray<FString>& Keywords) const
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    for (const FName& Root : Roots)
    {
        FARFilter Filter;
        Filter.PackagePaths.Add(Root);
        Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
        Filter.bRecursivePaths = true;
        TArray<FAssetData> Assets;
        Registry.GetAssets(Filter, Assets);
        for (const FAssetData& Asset : Assets)
        {
            const FString Name = Asset.AssetName.ToString();
            for (const FString& Keyword : Keywords)
            {
                if (Name.Contains(Keyword, ESearchCase::IgnoreCase))
                {
                    if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset())) { return Mesh; }
                }
            }
        }
    }
    return nullptr;
}

void ANWProceduralWorldManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWProceduralWorldManager, WorldEpoch);
}

void ANWProceduralWorldManager::AdvanceEpoch()
{
    if (!HasAuthority()) { return; }

    ClearSpawnedActors();
    ++WorldEpoch;

    // V9: the map itself is authored/stable. Epoch is gameplay state only.
    SpawnWorldActors();
    UE_LOG(LogTemp, Display, TEXT("[WORLD-V9] epoch %d: encontros/populacao renovados; terreno, trilhas e vegetacao macro permaneceram fixos."), WorldEpoch);
}

float ANWProceduralWorldManager::GetTerrainHeightAt(float X, float Y) const
{
    return SampleHeight(X - GetActorLocation().X, Y - GetActorLocation().Y) + GetActorLocation().Z;
}

FVector ANWProceduralWorldManager::GetTerrainNormalAt(float X, float Y) const
{
    constexpr float Step = 120.0f;
    const float L = GetTerrainHeightAt(X - Step, Y);
    const float R = GetTerrainHeightAt(X + Step, Y);
    const float D = GetTerrainHeightAt(X, Y - Step);
    const float U = GetTerrainHeightAt(X, Y + Step);
    return FVector(L - R, D - U, Step * 2.0f).GetSafeNormal();
}

float ANWProceduralWorldManager::GetTravelPathInfluence(float X, float Y) const
{
    if (!bUseHybridMacroWorld) { return 0.0f; }
    const float Distance = DistanceToTravelNetwork(X - GetActorLocation().X, Y - GetActorLocation().Y);
    const float Feather = 440.0f;
    return 1.0f - Smooth01((Distance - TravelPathHalfWidth) / Feather);
}

bool ANWProceduralWorldManager::IsInsideSettlementClearance(float X, float Y, float ExtraRadius) const
{
    const FVector2D P(X - GetActorLocation().X, Y - GetActorLocation().Y);
    if ((P - FVector2D(900.0f, 900.0f)).Size() <= 820.0f + ExtraRadius) { return true; }
    for (const FVector2D& Center : GetSettlementCenters())
    {
        if ((P - Center).Size() <= SettlementFlattenRadius + ExtraRadius) { return true; }
    }
    return false;
}

TArray<FVector2D> ANWProceduralWorldManager::GetStableSettlementCenters() const
{
    TArray<FVector2D> Result = GetSettlementCenters();
    const FVector2D Origin(GetActorLocation().X, GetActorLocation().Y);
    for (FVector2D& P : Result) { P += Origin; }
    return Result;
}

void ANWProceduralWorldManager::OnRep_WorldEpoch()
{
    // Macro geography is immutable in V9. Clients only observe the gameplay epoch.
    UE_LOG(LogTemp, Display, TEXT("[WORLD-V9] cliente recebeu epoch=%d sem regenerar o mapa."), WorldEpoch);
}

void ANWProceduralWorldManager::BuildWorld()
{
    BuildTerrain();
    BuildDecorations();
    if (HasAuthority()) { SpawnWorldActors(); }
}

void ANWProceduralWorldManager::BuildTerrain()
{
    TerrainMesh->ClearAllMeshSections();

    const int32 VerticesPerSide = TerrainResolution + 1;
    const int32 VertexCount = VerticesPerSide * VerticesPerSide;
    const float HalfWorld = TerrainResolution * TerrainCellSize * 0.5f;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    Vertices.Reserve(VertexCount);
    Normals.Reserve(VertexCount);
    UV0.Reserve(VertexCount);
    Colors.Reserve(VertexCount);
    Tangents.Reserve(VertexCount);
    Triangles.Reserve(TerrainResolution * TerrainResolution * 6);

    for (int32 Y = 0; Y <= TerrainResolution; ++Y)
    {
        for (int32 X = 0; X <= TerrainResolution; ++X)
        {
            const float LocalX = -HalfWorld + X * TerrainCellSize;
            const float LocalY = -HalfWorld + Y * TerrainCellSize;
            const float Height = SampleHeight(LocalX, LocalY);

            Vertices.Add(FVector(LocalX, LocalY, Height));

            // World-space tiling instead of stretching one texture across ~19 km.
            UV0.Add(FVector2D(LocalX / 700.0f, LocalY / 700.0f));

            const float HeightLeft = SampleHeight(LocalX - TerrainCellSize, LocalY);
            const float HeightRight = SampleHeight(LocalX + TerrainCellSize, LocalY);
            const float HeightDown = SampleHeight(LocalX, LocalY - TerrainCellSize);
            const float HeightUp = SampleHeight(LocalX, LocalY + TerrainCellSize);
            const FVector Normal = FVector(HeightLeft - HeightRight, HeightDown - HeightUp, 2.0f * TerrainCellSize).GetSafeNormal();
            Normals.Add(Normal);
            Tangents.Add(FProcMeshTangent(FVector(1.0f, 0.0f, 0.0f), false));

            const float Path = GetTravelPathInfluence(LocalX + GetActorLocation().X, LocalY + GetActorLocation().Y);
            const float Slope = 1.0f - FMath::Clamp(Normal.Z, 0.0f, 1.0f);
            const float Height01 = FMath::Clamp((Height / FMath::Max(1.0f, TerrainAmplitude) + 1.0f) * 0.5f, 0.0f, 1.0f);
            // Vertex color is a stable fallback and also gives future landscape-style materials useful masks.
            FLinearColor Ground(0.15f + Height01 * 0.07f, 0.255f + Height01 * 0.10f, 0.095f + Height01 * 0.045f, 1.0f);
            const FLinearColor Dirt(0.29f, 0.205f, 0.12f, 1.0f);
            const FLinearColor Rock(0.31f, 0.30f, 0.275f, 1.0f);
            Ground = FMath::Lerp(Ground, Dirt, Path * 0.80f);
            Ground = FMath::Lerp(Ground, Rock, FMath::Clamp((Slope - 0.12f) * 2.8f, 0.0f, 0.72f));
            Colors.Add(Ground);
        }
    }

    for (int32 Y = 0; Y < TerrainResolution; ++Y)
    {
        for (int32 X = 0; X < TerrainResolution; ++X)
        {
            const int32 I0 = Y * VerticesPerSide + X;
            const int32 I1 = I0 + 1;
            const int32 I2 = I0 + VerticesPerSide;
            const int32 I3 = I2 + 1;
            Triangles.Add(I0); Triangles.Add(I2); Triangles.Add(I1);
            Triangles.Add(I1); Triangles.Add(I2); Triangles.Add(I3);
        }
    }

    TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, Colors, Tangents, true);
    if (TerrainFallbackMaterial) { TerrainMesh->SetMaterial(0, TerrainFallbackMaterial); }
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
    TerrainMesh->bUseComplexAsSimpleCollision = true;
    TerrainMesh->RecreatePhysicsState();
    UE_LOG(LogTemp, Warning, TEXT("[TERRAIN-V9] macro fixo criado: %d vertices | %d triangulos | UV world-tiling | trilhas/nucleos suavizados."),
        Vertices.Num(), Triangles.Num() / 3);
}

void ANWProceduralWorldManager::BuildDecorations()
{
    TreeTrunks->ClearInstances();
    TreeCrowns->ClearInstances();
    Bushes->ClearInstances();
    Rocks->ClearInstances();
    Crystals->ClearInstances();
    Buildings->ClearInstances();
    Structures->ClearInstances();

    const float HalfWorld = TerrainResolution * TerrainCellSize * 0.5f - 600.0f;
    FRandomStream Random(GetMacroWorldSeed() + 101);

    auto IsMacroClear = [&](const FVector2D& Point, float SettlementExtra, float MaxPathInfluence)
    {
        const float WorldX = Point.X + GetActorLocation().X;
        const float WorldY = Point.Y + GetActorLocation().Y;
        return !IsInsideSettlementClearance(WorldX, WorldY, SettlementExtra)
            && GetTravelPathInfluence(WorldX, WorldY) <= MaxPathInfluence;
    };

    for (int32 Index = 0; Index < TreeCount; ++Index)
    {
        FVector2D Point;
        bool bValid = false;
        for (int32 Attempt = 0; Attempt < 12; ++Attempt)
        {
            Point = RandomGroundPoint(Random, HalfWorld, 1050.0f);
            if (IsMacroClear(Point, 260.0f, 0.12f)) { bValid = true; break; }
        }
        if (!bValid) { continue; }

        const float GroundZ = SampleHeight(Point.X, Point.Y);
        const float UniformScale = Random.FRandRange(0.70f, 1.35f);
        const float Yaw = Random.FRandRange(0.0f, 360.0f);
        if (bUsingRealisticTreeMesh)
        {
            TreeTrunks->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Point.X, Point.Y, GroundZ), FVector(UniformScale)));
        }
        else
        {
            TreeTrunks->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Point.X, Point.Y, GroundZ + 150.0f * UniformScale), FVector(0.30f * UniformScale, 0.30f * UniformScale, 3.0f * UniformScale)));
            TreeCrowns->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Point.X, Point.Y, GroundZ + 430.0f * UniformScale), FVector(1.55f * UniformScale, 1.55f * UniformScale, 2.25f * UniformScale)));
        }
    }

    for (int32 Index = 0; Index < BushCount; ++Index)
    {
        const FVector2D Point = RandomGroundPoint(Random, HalfWorld, 650.0f);
        if (!IsMacroClear(Point, 120.0f, 0.38f)) { continue; }
        const float GroundZ = SampleHeight(Point.X, Point.Y);
        const FVector Scale(Random.FRandRange(0.35f, 0.85f), Random.FRandRange(0.35f, 0.85f), Random.FRandRange(0.22f, 0.62f));
        Bushes->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(Point.X, Point.Y, GroundZ + (bUsingRealisticTreeMesh ? 0.0f : 45.0f)), Scale));
    }

    for (int32 Index = 0; Index < RockCount; ++Index)
    {
        const FVector2D Point = RandomGroundPoint(Random, HalfWorld, 700.0f);
        if (!IsMacroClear(Point, 80.0f, 0.55f)) { continue; }
        const float GroundZ = SampleHeight(Point.X, Point.Y);
        const FVector Scale(Random.FRandRange(0.35f, 1.25f), Random.FRandRange(0.35f, 1.15f), Random.FRandRange(0.25f, 0.95f));
        Rocks->AddInstance(FTransform(FRotator(Random.FRandRange(-12.0f, 12.0f), Random.FRandRange(0.0f, 360.0f), Random.FRandRange(-12.0f, 12.0f)), FVector(Point.X, Point.Y, GroundZ + 45.0f * Scale.Z), Scale));
    }

    for (int32 Index = 0; Index < CrystalCount; ++Index)
    {
        const FVector2D Point = RandomGroundPoint(Random, HalfWorld, 900.0f);
        if (!IsMacroClear(Point, 0.0f, 0.70f)) { continue; }
        const float GroundZ = SampleHeight(Point.X, Point.Y);
        const float Scale = Random.FRandRange(0.18f, 0.42f);
        Crystals->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(Point.X, Point.Y, GroundZ + 55.0f), FVector(Scale, Scale, Scale * Random.FRandRange(1.4f, 2.2f))));
    }

    BuildSettlements();
}

void ANWProceduralWorldManager::BuildSettlements()
{
    const TArray<FVector2D> Centers = GetSettlementCenters();
    FRandomStream Random(GetMacroWorldSeed() + 981);

    for (const FVector2D& Center : Centers)
    {
        for (int32 House = 0; House < 12; ++House)
        {
            const float Angle = (2.0f * PI * House / 12.0f) + Random.FRandRange(-0.12f, 0.12f);
            const float Radius = Random.FRandRange(650.0f, 1320.0f);
            const float X = Center.X + FMath::Cos(Angle) * Radius;
            const float Y = Center.Y + FMath::Sin(Angle) * Radius;
            const float GroundZ = SampleHeight(X, Y);

            if (bUsingRealisticBuildingMesh)
            {
                const float UniformScale = Random.FRandRange(0.82f, 1.12f);
                Buildings->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f), FVector(X, Y, GroundZ), FVector(UniformScale)));
            }
            else
            {
                const FVector Scale(Random.FRandRange(2.0f, 3.2f), Random.FRandRange(1.8f, 3.0f), Random.FRandRange(1.8f, 3.6f));
                Buildings->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f), FVector(X, Y, GroundZ + 50.0f * Scale.Z), Scale));
            }
        }

        for (int32 Wall = 0; Wall < 18; ++Wall)
        {
            const float Angle = 2.0f * PI * Wall / 18.0f;
            const float Radius = 1700.0f;
            const float X = Center.X + FMath::Cos(Angle) * Radius;
            const float Y = Center.Y + FMath::Sin(Angle) * Radius;
            const float GroundZ = SampleHeight(X, Y);
            Structures->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f), FVector(X, Y, GroundZ + 115.0f), FVector(2.7f, 0.35f, 2.3f)));
        }

        const float TowerZ = SampleHeight(Center.X, Center.Y);
        Structures->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Center.X, Center.Y, TowerZ + 210.0f), FVector(3.2f, 3.2f, 4.2f)));
    }
}

void ANWProceduralWorldManager::SpawnWorldActors()
{
    SpawnSettlementsAndCivilians();
    SpawnAmbientEnemies();
}

void ANWProceduralWorldManager::SpawnAmbientEnemies()
{
    if (!HasAuthority() || !GetWorld()) { return; }
    const float HalfWorld = TerrainResolution * TerrainCellSize * 0.5f - 1000.0f;
    FRandomStream Random(GetWorldSeed() + 4049);

    for (int32 Index = 0; Index < AmbientEnemyCount; ++Index)
    {
        const FVector2D Point = RandomGroundPoint(Random, HalfWorld, 1700.0f);
        const float GroundZ = SampleHeight(Point.X, Point.Y);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ANWEnemy* Enemy = GetWorld()->SpawnActor<ANWEnemy>(ANWEnemy::StaticClass(), FVector(Point.X, Point.Y, GroundZ + 125.0f), FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), Params);
        if (Enemy) { SpawnedEnemies.Add(Enemy); }
    }
}

void ANWProceduralWorldManager::SpawnSettlementsAndCivilians()
{
    if (!HasAuthority() || !GetWorld()) { return; }
    const TArray<FVector2D> Centers = GetSettlementCenters();
    FRandomStream Random(GetWorldSeed() + 707);

    for (int32 SettlementIndex = 0; SettlementIndex < Centers.Num(); ++SettlementIndex)
    {
        const FVector2D Center = Centers[SettlementIndex];
        const float CoreGroundZ = SampleHeight(Center.X, Center.Y);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ANWSettlementCore* Core = GetWorld()->SpawnActor<ANWSettlementCore>(ANWSettlementCore::StaticClass(), FVector(Center.X, Center.Y, CoreGroundZ + 230.0f), FRotator::ZeroRotator, Params);
        if (Core)
        {
            Core->SetSettlementIndex(SettlementIndex + 1);
            SpawnedSettlements.Add(Core);
        }

        for (int32 CivilianIndex = 0; CivilianIndex < CiviliansPerSettlement; ++CivilianIndex)
        {
            const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
            const float Radius = Random.FRandRange(250.0f, 1050.0f);
            const float X = Center.X + FMath::Cos(Angle) * Radius;
            const float Y = Center.Y + FMath::Sin(Angle) * Radius;
            const float GroundZ = SampleHeight(X, Y);
            ANWCivilian* Civilian = GetWorld()->SpawnActor<ANWCivilian>(ANWCivilian::StaticClass(), FVector(X, Y, GroundZ + 105.0f), FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), Params);
            if (Civilian)
            {
                Civilian->SetHomeLocation(FVector(X, Y, GroundZ + 105.0f));
                SpawnedCivilians.Add(Civilian);
            }
        }
    }
}

void ANWProceduralWorldManager::SpawnInvasionWave()
{
    if (!HasAuthority() || !GetWorld()) { return; }
    const TArray<FVector2D> Centers = GetSettlementCenters();
    FRandomStream Random(GetWorldSeed() + FMath::RoundToInt(GetWorld()->GetTimeSeconds()) * 17);
    int32 SpawnedThisWave = 0;

    for (int32 SettlementIndex = 0; SettlementIndex < Centers.Num(); ++SettlementIndex)
    {
        const FVector2D Center = Centers[SettlementIndex];
        for (int32 Index = 0; Index < InvasionWaveSizePerSettlement; ++Index)
        {
            const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
            const float Radius = Random.FRandRange(2600.0f, 3600.0f);
            const float X = Center.X + FMath::Cos(Angle) * Radius;
            const float Y = Center.Y + FMath::Sin(Angle) * Radius;
            const float GroundZ = SampleHeight(X, Y);
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            ANWEnemy* Enemy = GetWorld()->SpawnActor<ANWEnemy>(ANWEnemy::StaticClass(), FVector(X, Y, GroundZ + 125.0f), FRotator(0.0f, FMath::RadiansToDegrees(Angle + PI), 0.0f), Params);
            if (Enemy)
            {
                SpawnedEnemies.Add(Enemy);
                ++SpawnedThisWave;
            }
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[INVASAO] Nova onda: %d inimigos marchando contra %d assentamentos."), SpawnedThisWave, Centers.Num());
}

void ANWProceduralWorldManager::ClearSpawnedActors()
{
    if (!HasAuthority()) { return; }
    for (ANWEnemy* Enemy : SpawnedEnemies) { if (IsValid(Enemy)) { Enemy->Destroy(); } }
    for (ANWCivilian* Civilian : SpawnedCivilians) { if (IsValid(Civilian)) { Civilian->Destroy(); } }
    for (ANWSettlementCore* Settlement : SpawnedSettlements) { if (IsValid(Settlement)) { Settlement->Destroy(); } }
    SpawnedEnemies.Reset();
    SpawnedCivilians.Reset();
    SpawnedSettlements.Reset();
}

void ANWProceduralWorldManager::RelocatePlayersAfterEpoch()
{
    // Kept for API/backward compatibility. V9 does not move players because the map does not regenerate.
}

float ANWProceduralWorldManager::SampleRawMacroHeight(float X, float Y) const
{
    const int32 Seed = GetMacroWorldSeed();
    const FVector2D Offset(Seed * 0.0173f, Seed * 0.0119f);
    const FVector2D P1 = FVector2D(X, Y) * 0.000255f + Offset;
    const FVector2D P2 = FVector2D(X, Y) * 0.000610f + Offset * 1.73f;
    const FVector2D P3 = FVector2D(X, Y) * 0.001350f + Offset * 2.41f;

    const float Large = FMath::PerlinNoise2D(P1) * TerrainAmplitude * 0.76f;
    const float Medium = FMath::PerlinNoise2D(P2) * TerrainAmplitude * 0.19f;
    const float Detail = FMath::PerlinNoise2D(P3) * TerrainAmplitude * 0.045f;
    return Large + Medium + Detail;
}

float ANWProceduralWorldManager::DistanceToSegment2D(const FVector2D& P, const FVector2D& A, const FVector2D& B) const
{
    const FVector2D AB = B - A;
    const float Denom = AB.SizeSquared();
    if (Denom <= KINDA_SMALL_NUMBER) { return (P - A).Size(); }
    const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Denom, 0.0f, 1.0f);
    return (P - (A + AB * T)).Size();
}

float ANWProceduralWorldManager::DistanceToTravelNetwork(float X, float Y) const
{
    const FVector2D P(X, Y);
    const FVector2D Spawn(900.0f, 900.0f);
    const TArray<FVector2D> Centers = GetSettlementCenters();
    float Best = TNumericLimits<float>::Max();
    if (Centers.Num() >= 2)
    {
        Best = FMath::Min(Best, DistanceToSegment2D(P, Spawn, Centers[0]));
        Best = FMath::Min(Best, DistanceToSegment2D(P, Spawn, Centers[1]));
        Best = FMath::Min(Best, DistanceToSegment2D(P, Centers[0], Centers[1]));
    }
    return Best;
}

float ANWProceduralWorldManager::SampleHeight(float X, float Y) const
{
    float Height = SampleRawMacroHeight(X, Y);
    if (!bUseHybridMacroWorld) { return Height; }

    const FVector2D P(X, Y);
    const FVector2D Spawn(900.0f, 900.0f);
    const TArray<FVector2D> Centers = GetSettlementCenters();

    // Stable spawn clearing.
    const float SpawnDistance = (P - Spawn).Size();
    const float SpawnBlend = 1.0f - Smooth01((SpawnDistance - 620.0f) / 520.0f);
    const float SpawnTarget = SampleRawMacroHeight(Spawn.X, Spawn.Y);
    Height = FMath::Lerp(Height, SpawnTarget, SpawnBlend * 0.88f);

    // Stable settlement pads with feathered edges.
    for (const FVector2D& Center : Centers)
    {
        const float Distance = (P - Center).Size();
        const float Blend = 1.0f - Smooth01((Distance - SettlementFlattenRadius * 0.62f) / (SettlementFlattenRadius * 0.52f));
        const float Target = SampleRawMacroHeight(Center.X, Center.Y);
        Height = FMath::Lerp(Height, Target, Blend * 0.92f);
    }

    // Authored road network. We flatten toward a linear grade between endpoints,
    // which removes the short-frequency bumps visible in the V8 playtest.
    auto BlendRoad = [&](const FVector2D& A, const FVector2D& B)
    {
        const FVector2D AB = B - A;
        const float Denom = AB.SizeSquared();
        if (Denom <= KINDA_SMALL_NUMBER) { return; }
        const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Denom, 0.0f, 1.0f);
        const FVector2D Closest = A + AB * T;
        const float Distance = (P - Closest).Size();
        const float Influence = 1.0f - Smooth01((Distance - TravelPathHalfWidth) / 440.0f);
        if (Influence <= 0.0f) { return; }
        const float HA = SampleRawMacroHeight(A.X, A.Y);
        const float HB = SampleRawMacroHeight(B.X, B.Y);
        const float RoadTarget = FMath::Lerp(HA, HB, T);
        Height = FMath::Lerp(Height, RoadTarget, Influence * 0.83f);
    };

    if (Centers.Num() >= 2)
    {
        BlendRoad(Spawn, Centers[0]);
        BlendRoad(Spawn, Centers[1]);
        BlendRoad(Centers[0], Centers[1]);
    }

    return Height;
}

FVector2D ANWProceduralWorldManager::RandomGroundPoint(FRandomStream& Random, float HalfWorld, float MinimumCenterDistance) const
{
    FVector2D Point = FVector2D::ZeroVector;
    for (int32 Attempt = 0; Attempt < 24; ++Attempt)
    {
        Point.X = Random.FRandRange(-HalfWorld, HalfWorld);
        Point.Y = Random.FRandRange(-HalfWorld, HalfWorld);
        if (Point.Size() >= MinimumCenterDistance) { return Point; }
    }
    return FVector2D(MinimumCenterDistance, 0.0f);
}

TArray<FVector2D> ANWProceduralWorldManager::GetSettlementCenters() const
{
    // V9: no drift by epoch. These become authored macro anchors.
    return { FVector2D(-3600.0f, 2700.0f), FVector2D(4300.0f, -3000.0f) };
}

int32 ANWProceduralWorldManager::GetWorldSeed() const
{
    // Dynamic seed is still useful for events/population; it no longer controls geography.
    return GetMacroWorldSeed() + WorldEpoch * 7919;
}
