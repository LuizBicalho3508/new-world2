#include "NWGameplaySafetyActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NWCharacter.h"
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

    const int32 Grid = FMath::Clamp(CollisionGridResolution, 16, 64);
    const float HalfExtent = Manager->GetTerrainHalfExtent();
    const float TileSize = (HalfExtent * 2.0f) / static_cast<float>(Grid);
    const float HalfTile = TileSize * 0.5f;
    const float ProxyXYScale = (TileSize * 0.98f) / 100.0f;
    const float ProxyZScale = CollisionProxyThickness / 100.0f;
    const FVector Origin = Manager->GetActorLocation();

    GroundCollisionProxy->PreAllocateInstancesMemory(Grid * Grid);

    for (int32 Y = 0; Y < Grid; ++Y)
    {
        for (int32 X = 0; X < Grid; ++X)
        {
            const float WorldX = Origin.X - HalfExtent + HalfTile + X * TileSize;
            const float WorldY = Origin.Y - HalfExtent + HalfTile + Y * TileSize;

            // Mantemos o topo do proxy abaixo da menor amostra local. Assim ele nunca
            // deve aparecer por cima do terreno visual; o clamp de player abaixo cuida
            // da aderencia exata a superficie procedural.
            float TopZ = Manager->GetTerrainHeightAt(WorldX, WorldY);
            TopZ = FMath::Min(TopZ, Manager->GetTerrainHeightAt(WorldX - HalfTile, WorldY - HalfTile));
            TopZ = FMath::Min(TopZ, Manager->GetTerrainHeightAt(WorldX + HalfTile, WorldY - HalfTile));
            TopZ = FMath::Min(TopZ, Manager->GetTerrainHeightAt(WorldX - HalfTile, WorldY + HalfTile));
            TopZ = FMath::Min(TopZ, Manager->GetTerrainHeightAt(WorldX + HalfTile, WorldY + HalfTile));
            TopZ -= 24.0f;

            const FVector Location(WorldX, WorldY, TopZ - CollisionProxyThickness * 0.5f);
            const FVector Scale(ProxyXYScale, ProxyXYScale, ProxyZScale);
            GroundCollisionProxy->AddInstance(FTransform(FRotator::ZeroRotator, Location, Scale), true);
        }
    }

    CachedEpoch = Manager->GetWorldEpoch();
    UE_LOG(LogTemp, Display, TEXT("[SAFETY] proxy de colisao procedural reconstruido: %dx%d | epoch=%d"), Grid, Grid, CachedEpoch);
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
        const float SafeCenterZ = GroundZ + CapsuleHalfHeight + 4.0f;

        // Durante um pulo normal o centro do capsule fica acima do solo. So
        // interferimos quando ele realmente atravessa a superficie na descida.
        if (Current.Z >= SafeCenterZ - VisualGroundTolerance || Movement->Velocity.Z > 0.0f)
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

        const float Now = GetWorld()->GetTimeSeconds();
        if ((Now - LastRecoveryLogTime) >= 2.0f)
        {
            LastRecoveryLogTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("[SAFETY] player recuperado para o terreno em Z=%.1f (estava %.1f)."), SafeCenterZ, Current.Z);
        }
    }
}
