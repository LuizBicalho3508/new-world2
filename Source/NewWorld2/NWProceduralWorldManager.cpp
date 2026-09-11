#include "NWProceduralWorldManager.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NWCivilian.h"
#include "NWEnemy.h"
#include "NWSettlementCore.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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

    Structures = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Structures"));
    Structures->SetupAttachment(SceneRoot);
    Structures->SetCollisionProfileName(TEXT("BlockAll"));

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
    HeightFog->SetFogDensity(0.008f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (CylinderMesh.Succeeded()) { TreeTrunks->SetStaticMesh(CylinderMesh.Object); }
    if (ConeMesh.Succeeded()) { TreeCrowns->SetStaticMesh(ConeMesh.Object); }
    if (SphereMesh.Succeeded()) { Bushes->SetStaticMesh(SphereMesh.Object); Crystals->SetStaticMesh(SphereMesh.Object); }
    if (CubeMesh.Succeeded()) { Rocks->SetStaticMesh(CubeMesh.Object); Structures->SetStaticMesh(CubeMesh.Object); }
}

void ANWProceduralWorldManager::BeginPlay()
{
    Super::BeginPlay();
    BuildWorld();

    if (HasAuthority())
    {
        if (EvolutionIntervalSeconds > 0.0f)
        {
            GetWorldTimerManager().SetTimer(EvolutionTimer, this, &ANWProceduralWorldManager::AdvanceEpoch, EvolutionIntervalSeconds, true);
        }

        if (InvasionIntervalSeconds > 0.0f)
        {
            GetWorldTimerManager().SetTimer(InvasionTimer, this, &ANWProceduralWorldManager::SpawnInvasionWave, InvasionIntervalSeconds, true, 20.0f);
        }
    }
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
    BuildWorld();
    RelocatePlayersAfterEpoch();
    UE_LOG(LogTemp, Display, TEXT("[WORLD] Novo epoch %d. Terreno, vegetacao, recursos, cidades e populacao foram regenerados."), WorldEpoch);
}

float ANWProceduralWorldManager::GetTerrainHeightAt(float X, float Y) const
{
    return SampleHeight(X - GetActorLocation().X, Y - GetActorLocation().Y) + GetActorLocation().Z;
}

void ANWProceduralWorldManager::OnRep_WorldEpoch()
{
    BuildTerrain();
    BuildDecorations();
}

void ANWProceduralWorldManager::BuildWorld()
{
    BuildTerrain();
    BuildDecorations();

    if (HasAuthority())
    {
        SpawnWorldActors();
    }
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
            UV0.Add(FVector2D(static_cast<float>(X) / TerrainResolution, static_cast<float>(Y) / TerrainResolution));

            const float HeightLeft = SampleHeight(LocalX - TerrainCellSize, LocalY);
            const float HeightRight = SampleHeight(LocalX + TerrainCellSize, LocalY);
            const float HeightDown = SampleHeight(LocalX, LocalY - TerrainCellSize);
            const float HeightUp = SampleHeight(LocalX, LocalY + TerrainCellSize);
            Normals.Add(FVector(HeightLeft - HeightRight, HeightDown - HeightUp, 2.0f * TerrainCellSize).GetSafeNormal());
            Tangents.Add(FProcMeshTangent(FVector(1.0f, 0.0f, 0.0f), false));

            const float NormalizedHeight = FMath::Clamp((Height / TerrainAmplitude + 1.0f) * 0.5f, 0.0f, 1.0f);
            Colors.Add(FLinearColor(0.08f + NormalizedHeight * 0.18f, 0.22f + NormalizedHeight * 0.34f, 0.07f + NormalizedHeight * 0.08f, 1.0f));
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

            Triangles.Add(I0); Triangles.Add(I1); Triangles.Add(I2);
            Triangles.Add(I1); Triangles.Add(I3); Triangles.Add(I2);
        }
    }

    TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, Colors, Tangents, true);
}

void ANWProceduralWorldManager::BuildDecorations()
{
    TreeTrunks->ClearInstances();
    TreeCrowns->ClearInstances();
    Bushes->ClearInstances();
    Rocks->ClearInstances();
    Crystals->ClearInstances();
    Structures->ClearInstances();

    const float HalfWorld = TerrainResolution * TerrainCellSize * 0.5f - 600.0f;
    FRandomStream Random(GetWorldSeed());

    for (int32 Index = 0; Index < TreeCount; ++Index)
    {
        const FVector2D Point = RandomGroundPoint(Random, HalfWorld, 1200.0f);
        const float GroundZ = SampleHeight(Point.X, Point.Y);
        const float UniformScale = Random.FRandRange(0.65f, 1.45f);
        const float Yaw = Random.FRandRange(0.0f, 360.0f);

        TreeTrunks->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Point.X, Point.Y, GroundZ + 150.0f * UniformScale), FVector(0.30f * UniformScale, 0.30f * UniformScale, 3.0f * UniformScale)));
        TreeCrowns->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Point.X, Point.Y, GroundZ + 430.0f * UniformScale), FVector(1.55f * UniformScale, 1.55f * UniformScale, 2.25f * UniformScale)));
    }

    for (int32 Index = 0; Index < BushCount; ++Index)
    {
        const FVector2D Point = RandomGroundPoint(Random, HalfWorld, 650.0f);
        const float GroundZ = SampleHeight(Point.X, Point.Y);
        const FVector Scale(Random.FRandRange(0.35f, 0.85f), Random.FRandRange(0.35f, 0.85f), Random.FRandRange(0.22f, 0.62f));
        Bushes->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(Point.X, Point.Y, GroundZ + 45.0f), Scale));
    }

    for (int32 Index = 0; Index < RockCount; ++Index)
    {
        const FVector2D Point = RandomGroundPoint(Random, HalfWorld, 700.0f);
        const float GroundZ = SampleHeight(Point.X, Point.Y);
        const FVector Scale(Random.FRandRange(0.35f, 1.25f), Random.FRandRange(0.35f, 1.15f), Random.FRandRange(0.25f, 0.95f));
        Rocks->AddInstance(FTransform(FRotator(Random.FRandRange(-12.0f, 12.0f), Random.FRandRange(0.0f, 360.0f), Random.FRandRange(-12.0f, 12.0f)), FVector(Point.X, Point.Y, GroundZ + 45.0f * Scale.Z), Scale));
    }

    for (int32 Index = 0; Index < CrystalCount; ++Index)
    {
        const FVector2D Point = RandomGroundPoint(Random, HalfWorld, 900.0f);
        const float GroundZ = SampleHeight(Point.X, Point.Y);
        const float Scale = Random.FRandRange(0.18f, 0.42f);
        Crystals->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(Point.X, Point.Y, GroundZ + 55.0f), FVector(Scale, Scale, Scale * Random.FRandRange(1.4f, 2.2f))));
    }

    BuildSettlements();
}

void ANWProceduralWorldManager::BuildSettlements()
{
    const TArray<FVector2D> Centers = GetSettlementCenters();
    FRandomStream Random(GetWorldSeed() + 981);

    for (const FVector2D& Center : Centers)
    {
        for (int32 House = 0; House < 12; ++House)
        {
            const float Angle = (2.0f * PI * House / 12.0f) + Random.FRandRange(-0.12f, 0.12f);
            const float Radius = Random.FRandRange(650.0f, 1350.0f);
            const float X = Center.X + FMath::Cos(Angle) * Radius;
            const float Y = Center.Y + FMath::Sin(Angle) * Radius;
            const float GroundZ = SampleHeight(X, Y);
            const FVector Scale(Random.FRandRange(2.4f, 4.2f), Random.FRandRange(2.2f, 3.8f), Random.FRandRange(2.2f, 4.8f));
            Structures->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f), FVector(X, Y, GroundZ + 50.0f * Scale.Z), Scale));
        }

        for (int32 Wall = 0; Wall < 18; ++Wall)
        {
            const float Angle = 2.0f * PI * Wall / 18.0f;
            const float Radius = 1700.0f;
            const float X = Center.X + FMath::Cos(Angle) * Radius;
            const float Y = Center.Y + FMath::Sin(Angle) * Radius;
            const float GroundZ = SampleHeight(X, Y);
            Structures->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f), FVector(X, Y, GroundZ + 150.0f), FVector(3.1f, 0.45f, 3.0f)));
        }

        const float TowerZ = SampleHeight(Center.X, Center.Y);
        Structures->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Center.X, Center.Y, TowerZ + 260.0f), FVector(4.0f, 4.0f, 5.2f)));
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
    if (!HasAuthority() || !GetWorld()) { return; }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn) { continue; }

        FVector Location = Pawn->GetActorLocation();
        Location.Z = GetTerrainHeightAt(Location.X, Location.Y) + 250.0f;
        Pawn->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

float ANWProceduralWorldManager::SampleHeight(float X, float Y) const
{
    const int32 Seed = GetWorldSeed();
    const FVector2D Offset(Seed * 0.0173f, Seed * 0.0119f);
    const FVector2D P1 = FVector2D(X, Y) * 0.00042f + Offset;
    const FVector2D P2 = FVector2D(X, Y) * 0.00095f + Offset * 1.73f;
    const FVector2D P3 = FVector2D(X, Y) * 0.00210f + Offset * 2.41f;

    const float Large = FMath::PerlinNoise2D(P1) * TerrainAmplitude;
    const float Medium = FMath::PerlinNoise2D(P2) * TerrainAmplitude * 0.32f;
    const float Detail = FMath::PerlinNoise2D(P3) * TerrainAmplitude * 0.09f;
    return Large + Medium + Detail;
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
    FRandomStream Random(GetWorldSeed() + 5501);
    const FVector2D DriftA(Random.FRandRange(-450.0f, 450.0f), Random.FRandRange(-450.0f, 450.0f));
    const FVector2D DriftB(Random.FRandRange(-450.0f, 450.0f), Random.FRandRange(-450.0f, 450.0f));
    return { FVector2D(-3600.0f, 2700.0f) + DriftA, FVector2D(4300.0f, -3000.0f) + DriftB };
}

int32 ANWProceduralWorldManager::GetWorldSeed() const
{
    return 1337 + WorldEpoch * 7919;
}
