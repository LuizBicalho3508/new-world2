#include "NWGameplaySafetyActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
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

namespace
{
    float GetWeaponTargetDimension(const ENWWeaponType WeaponType, const bool bLeftHand)
    {
        switch (WeaponType)
        {
            case ENWWeaponType::Staff: return 185.0f;
            case ENWWeaponType::Greatsword: return 170.0f;
            case ENWWeaponType::DualSwords: return 112.0f;
            case ENWWeaponType::SwordShield: return bLeftHand ? 78.0f : 112.0f;
            case ENWWeaponType::Daggers: return 58.0f;
            case ENWWeaponType::Bow: return 145.0f;
            case ENWWeaponType::Firearm: return 128.0f;
            default: return 120.0f;
        }
    }
}

ANWGameplaySafetyActor::ANWGameplaySafetyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    // 20 Hz e suficiente para corrigir pousos/queda sem gastar um tick por frame.
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
    StabilizeRuntimeVisuals(DeltaSeconds);
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

void ANWGameplaySafetyActor::StabilizeRuntimeVisuals(float DeltaSeconds)
{
    if (!GetWorld())
    {
        return;
    }

    VisualSafetyAccumulator += DeltaSeconds;
    if (VisualSafetyAccumulator < VisualSafetyInterval)
    {
        return;
    }
    VisualSafetyAccumulator = 0.0f;

    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            NormalizeWeaponVisuals(*It);
        }
    }

    NormalizeEnemyVisuals();
}

void ANWGameplaySafetyActor::NormalizeWeaponVisuals(ANWCharacter* Character)
{
    if (!Character)
    {
        return;
    }

    TArray<UStaticMeshComponent*> Components;
    Character->GetComponents<UStaticMeshComponent>(Components);

    for (UStaticMeshComponent* Component : Components)
    {
        if (!Component || !Component->GetName().Contains(TEXT("NW_WeaponVisual"), ESearchCase::IgnoreCase))
        {
            continue;
        }

        const bool bLeftHand = Component->GetName().Contains(TEXT("_L"), ESearchCase::IgnoreCase);
        const float TargetDimension = GetWeaponTargetDimension(Character->GetActiveWeapon(), bLeftHand);
        NormalizeStaticMeshComponent(Component, TargetDimension, TEXT("arma"));
    }
}

void ANWGameplaySafetyActor::NormalizeEnemyVisuals()
{
    if (!GetWorld())
    {
        return;
    }

    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy) || !Enemy->GetMesh())
        {
            continue;
        }

        USkeletalMesh* Mesh = Enemy->GetMesh()->GetSkeletalMeshAsset();
        if (!Mesh)
        {
            continue;
        }

        const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
        const float Height = static_cast<float>(Bounds.BoxExtent.Z * 2.0);
        if (!FMath::IsFinite(Height) || Height <= 1.0f)
        {
            continue;
        }

        const bool bBoss = Enemy->IsWorldBoss() || Cast<ANWDungeonGuardian>(Enemy) != nullptr;
        const float MinReasonableHeight = bBoss ? 210.0f : 135.0f;
        const float MaxReasonableHeight = bBoss ? 430.0f : 280.0f;
        if (Height >= MinReasonableHeight && Height <= MaxReasonableHeight)
        {
            continue;
        }

        const float TargetHeight = bBoss ? 320.0f : 190.0f;
        const float DesiredScale = FMath::Clamp(TargetHeight / Height, 0.08f, 3.0f);
        const FVector Desired(DesiredScale);
        if (!Enemy->GetMesh()->GetRelativeScale3D().Equals(Desired, 0.025f))
        {
            Enemy->GetMesh()->SetRelativeScale3D(Desired);
            UE_LOG(LogTemp, Warning, TEXT("[VISUAL-SAFETY] criatura %s normalizada: altura mesh %.1f -> alvo %.1f | escala %.3f"),
                *Enemy->GetName(), Height, TargetHeight, DesiredScale);
        }
    }
}

void ANWGameplaySafetyActor::NormalizeStaticMeshComponent(UStaticMeshComponent* Component, float TargetMaxDimension, const TCHAR* Context)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

    const FBoxSphereBounds Bounds = Component->GetStaticMesh()->GetBounds();
    const float SizeX = static_cast<float>(Bounds.BoxExtent.X * 2.0);
    const float SizeY = static_cast<float>(Bounds.BoxExtent.Y * 2.0);
    const float SizeZ = static_cast<float>(Bounds.BoxExtent.Z * 2.0);
    const float MaxDimension = FMath::Max(SizeX, FMath::Max(SizeY, SizeZ));
    if (!FMath::IsFinite(MaxDimension) || MaxDimension <= 1.0f)
    {
        return;
    }

    const float DesiredScale = FMath::Clamp(TargetMaxDimension / MaxDimension, 0.015f, 5.0f);
    const FVector Desired(DesiredScale);
    if (Component->GetRelativeScale3D().Equals(Desired, 0.025f))
    {
        return;
    }

    Component->SetRelativeScale3D(Desired);
    UE_LOG(LogTemp, Display, TEXT("[VISUAL-SAFETY] %s %s | mesh=%s | dimensao=%.1f | alvo=%.1f | escala=%.3f"),
        Context,
        *Component->GetName(),
        *Component->GetStaticMesh()->GetName(),
        MaxDimension,
        TargetMaxDimension,
        DesiredScale);
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

        // Bosses e guardioes sao encontros de progressao e nunca entram no culling.
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
