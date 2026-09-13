#include "NWGameplaySafetyActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/RotationMatrix.h"
#include "NWCharacter.h"
#include "NWDungeonGuardian.h"
#include "NWEnemy.h"
#include "NWProceduralWorldManager.h"
#include "UObject/ConstructorHelpers.h"

ANWGameplaySafetyActor::ANWGameplaySafetyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;
    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    GroundCollisionProxy = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GroundCollisionProxy"));
    GroundCollisionProxy->SetupAttachment(SceneRoot);
    GroundCollisionProxy->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GroundCollisionProxy->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    GroundCollisionProxy->SetGenerateOverlapEvents(false);
    GroundCollisionProxy->SetVisibility(false, true);
    GroundCollisionProxy->SetHiddenInGame(true);
    GroundCollisionProxy->SetCastShadow(false);
    GroundCollisionProxy->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        GroundCollisionProxy->SetStaticMesh(CubeMesh.Object);
    }
}

void ANWGameplaySafetyActor::BeginPlay()
{
    Super::BeginPlay();
    ResolveWorldManager();
    RebuildCollisionProxy();
}

void ANWGameplaySafetyActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    ANWProceduralWorldManager* Manager = ResolveWorldManager();
    if (!Manager)
    {
        return;
    }

    if (CachedEpoch != Manager->GetWorldEpoch())
    {
        RebuildCollisionProxy();
    }

    StabilizePlayers();
    EnforceEnemyPopulationBudget(DeltaSeconds);
}

ANWProceduralWorldManager* ANWGameplaySafetyActor::ResolveWorldManager()
{
    if (CachedWorldManager.IsValid())
    {
        return CachedWorldManager.Get();
    }

    if (!GetWorld())
    {
        return nullptr;
    }

    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        CachedWorldManager = *It;
        return *It;
    }

    return nullptr;
}

void ANWGameplaySafetyActor::RebuildCollisionProxy()
{
    ANWProceduralWorldManager* Manager = ResolveWorldManager();
    if (!Manager || !GroundCollisionProxy || !GroundCollisionProxy->GetStaticMesh())
    {
        return;
    }

    GroundCollisionProxy->ClearInstances();

    const int32 Grid = FMath::Clamp(CollisionGridResolution, 32, 96);
    const float HalfExtent = Manager->GetTerrainHalfExtent();
    const float TileSize = (HalfExtent * 2.0f) / static_cast<float>(Grid);
    const float HalfTile = TileSize * 0.5f;
    const float SampleOffset = FMath::Max(30.0f, TileSize * 0.45f);
    const float ProxyXYScale = (TileSize * 1.035f) / 100.0f;
    const float ProxyZScale = CollisionProxyThickness / 100.0f;
    const FVector Origin = Manager->GetActorLocation();

    GroundCollisionProxy->PreAllocateInstancesMemory(Grid * Grid);

    for (int32 Y = 0; Y < Grid; ++Y)
    {
        for (int32 X = 0; X < Grid; ++X)
        {
            const float WorldX = Origin.X - HalfExtent + HalfTile + X * TileSize;
            const float WorldY = Origin.Y - HalfExtent + HalfTile + Y * TileSize;
            const float GroundZ = Manager->GetTerrainHeightAt(WorldX, WorldY);

            const float HeightLeft = Manager->GetTerrainHeightAt(WorldX - SampleOffset, WorldY);
            const float HeightRight = Manager->GetTerrainHeightAt(WorldX + SampleOffset, WorldY);
            const float HeightDown = Manager->GetTerrainHeightAt(WorldX, WorldY - SampleOffset);
            const float HeightUp = Manager->GetTerrainHeightAt(WorldX, WorldY + SampleOffset);

            const FVector Normal = FVector(
                HeightLeft - HeightRight,
                HeightDown - HeightUp,
                2.0f * SampleOffset).GetSafeNormal();

            const FRotator SurfaceRotation = FRotationMatrix::MakeFromZ(Normal).Rotator();
            const FVector SurfacePoint(WorldX, WorldY, GroundZ - 2.0f);
            const FVector Location = SurfacePoint - Normal * (CollisionProxyThickness * 0.5f);
            const FVector Scale(ProxyXYScale, ProxyXYScale, ProxyZScale);

            GroundCollisionProxy->AddInstance(FTransform(SurfaceRotation, Location, Scale), true);
        }
    }

    CachedEpoch = Manager->GetWorldEpoch();
    UE_LOG(LogTemp, Display, TEXT("[SAFETY] piso de colisao reconstruido: %dx%d | epoch=%d"), Grid, Grid, CachedEpoch);
}

void ANWGameplaySafetyActor::StabilizePlayers()
{
    ANWProceduralWorldManager* Manager = ResolveWorldManager();
    if (!Manager || !GetWorld())
    {
        return;
    }

    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character) || !Character->GetCapsuleComponent())
        {
            continue;
        }

        UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
        if (!Movement)
        {
            continue;
        }

        const FVector Current = Character->GetActorLocation();
        const float GroundZ = Manager->GetTerrainHeightAt(Current.X, Current.Y);
        const float CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float SafeCenterZ = GroundZ + CapsuleHalfHeight + 3.0f;
        const float Difference = Current.Z - SafeCenterZ;

        const bool bDescending = Movement->Velocity.Z <= 0.0f;
        const bool bLandingSnap = Movement->IsFalling() && bDescending && Difference <= LandingSnapTolerance && Difference >= -EmergencyRecoveryDepth;
        const bool bEmergencyRecovery = Difference < -EmergencyRecoveryDepth;

        if (!bLandingSnap && !bEmergencyRecovery)
        {
            continue;
        }

        FVector SafeLocation = Current;
        SafeLocation.Z = SafeCenterZ;
        Character->SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);

        FVector Velocity = Movement->Velocity;
        Velocity.Z = 0.0f;
        Movement->Velocity = Velocity;
        Movement->SetMovementMode(MOVE_Walking);

        if (bEmergencyRecovery)
        {
            const float Now = GetWorld()->GetTimeSeconds();
            if ((Now - LastRecoveryLogTime) >= 2.0f)
            {
                LastRecoveryLogTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("[SAFETY] queda atraves do terreno corrigida: Z %.1f -> %.1f"), Current.Z, SafeCenterZ);
            }
        }
    }
}

void ANWGameplaySafetyActor::EnforceEnemyPopulationBudget(float DeltaSeconds)
{
    if (!GetWorld() || GetNetMode() == NM_Client)
    {
        return;
    }

    PopulationCheckAccumulator += DeltaSeconds;
    if (PopulationCheckAccumulator < PopulationCheckInterval)
    {
        return;
    }
    PopulationCheckAccumulator = 0.0f;

    TArray<FVector> PlayerLocations;
    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            PlayerLocations.Add(It->GetActorLocation());
        }
    }

    struct FCullCandidate
    {
        ANWEnemy* Enemy = nullptr;
        float NearestPlayerDistanceSq = 0.0f;
    };

    TArray<FCullCandidate> Candidates;
    int32 TotalEnemies = 0;

    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy))
        {
            continue;
        }

        ++TotalEnemies;

        if (Enemy->IsWorldBoss() || Cast<ANWDungeonGuardian>(Enemy))
        {
            continue;
        }

        float NearestDistanceSq = TNumericLimits<float>::Max();
        if (PlayerLocations.IsEmpty())
        {
            NearestDistanceSq = Enemy->GetActorLocation().SizeSquared2D();
        }
        else
        {
            for (const FVector& PlayerLocation : PlayerLocations)
            {
                const FVector Delta = Enemy->GetActorLocation() - PlayerLocation;
                NearestDistanceSq = FMath::Min(NearestDistanceSq, FVector(Delta.X, Delta.Y, 0.0f).SizeSquared());
            }
        }

        FCullCandidate& Candidate = Candidates.AddDefaulted_GetRef();
        Candidate.Enemy = Enemy;
        Candidate.NearestPlayerDistanceSq = NearestDistanceSq;
    }

    const int32 Excess = TotalEnemies - FMath::Max(1, MaxConcurrentEnemies);
    if (Excess <= 0 || Candidates.IsEmpty())
    {
        return;
    }

    Candidates.Sort([](const FCullCandidate& A, const FCullCandidate& B)
    {
        return A.NearestPlayerDistanceSq > B.NearestPlayerDistanceSq;
    });

    int32 Removed = 0;
    for (FCullCandidate& Candidate : Candidates)
    {
        if (Removed >= Excess)
        {
            break;
        }
        if (!IsValid(Candidate.Enemy))
        {
            continue;
        }

        Candidate.Enemy->Destroy();
        ++Removed;
    }

    if (Removed > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BUDGET] inimigos=%d teto=%d removidos=%d (mobs mais distantes; bosses/guardioes preservados)"),
            TotalEnemies, MaxConcurrentEnemies, Removed);
    }
}
