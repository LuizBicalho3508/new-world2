#include "NWCivilian.h"

#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    bool HasAssetPackage(const FName PackageName)
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(PackageName, Assets, true);
        return !Assets.IsEmpty();
    }
}

ANWCivilian::ANWCivilian()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.20f;
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(5.0f);
    SetMinNetUpdateFrequency(2.0f);

    GetCapsuleComponent()->InitCapsuleSize(38.0f, 86.0f);
    GetMesh()->bEnableUpdateRateOptimizations = true;
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(GetCapsuleComponent());
    BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -6.0f));
    BodyMesh->SetRelativeScale3D(FVector(0.38f, 0.38f, 1.55f));
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // UE 5.8 does not ship /Engine/BasicShapes/Capsule. Keep a valid fallback
    // so missing optional NPC packs never create CDO load errors.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (FallbackMesh.Succeeded())
    {
        BodyMesh->SetStaticMesh(FallbackMesh.Object);
    }
}

void ANWCivilian::BeginPlay()
{
    Super::BeginPlay();

    // Sparrow is optional. Verify package presence before hard-loading it so an
    // absent Fab pack stays a normal fallback instead of a runtime warning.
    const FName SparrowMeshPackage(TEXT("/Game/ParagonSparrow/Characters/Heroes/Sparrow/Meshes/Sparrow"));
    const FName SparrowAnimPackage(TEXT("/Game/ParagonSparrow/Characters/Heroes/Sparrow/Sparrow_AnimBlueprint"));
    if (HasAssetPackage(SparrowMeshPackage) && HasAssetPackage(SparrowAnimPackage))
    {
        USkeletalMesh* LicensedMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ParagonSparrow/Characters/Heroes/Sparrow/Meshes/Sparrow.Sparrow"));
        UClass* LicensedAnimClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/ParagonSparrow/Characters/Heroes/Sparrow/Sparrow_AnimBlueprint.Sparrow_AnimBlueprint_C"));
        if (LicensedMesh && LicensedAnimClass)
        {
            GetMesh()->SetSkeletalMeshAsset(LicensedMesh);
            GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            GetMesh()->SetAnimInstanceClass(LicensedAnimClass);
            GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -86.0f));
            GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
            GetMesh()->SetVisibility(true, true);
            BodyMesh->SetVisibility(false, true);
        }
    }

    if (HasAuthority())
    {
        Health = MaxHealth;
        if (HomeLocation.IsNearlyZero())
        {
            HomeLocation = GetActorLocation();
        }
    }
}

void ANWCivilian::SetHomeLocation(const FVector& InHomeLocation)
{
    HomeLocation = InHomeLocation;
}

void ANWCivilian::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    const FVector ToHome = HomeLocation - GetActorLocation();

    if (FVector(ToHome.X, ToHome.Y, 0.0f).Size() > WanderRadius)
    {
        WanderDirection = FVector(ToHome.X, ToHome.Y, 0.0f).GetSafeNormal();
    }
    else if (Now >= NextDirectionChangeTime)
    {
        const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
        WanderDirection = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        NextDirectionChangeTime = Now + FMath::FRandRange(2.5f, 6.0f);
    }

    const FVector NextLocation = GetActorLocation() + WanderDirection * MoveSpeed * DeltaSeconds;
    SetActorLocation(NextLocation, true);

    if (!WanderDirection.IsNearlyZero())
    {
        SetActorRotation(WanderDirection.Rotation());
    }
}

float ANWCivilian::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (!HasAuthority() || AppliedDamage <= 0.0f)
    {
        return AppliedDamage;
    }

    Health = FMath::Clamp(Health - AppliedDamage, 0.0f, MaxHealth);
    if (Health <= 0.0f)
    {
        Health = MaxHealth;
        SetActorLocation(HomeLocation + FVector(0.0f, 0.0f, 120.0f), false, nullptr, ETeleportType::TeleportPhysics);
    }

    return AppliedDamage;
}

void ANWCivilian::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWCivilian, Health);
}

void ANWCivilian::OnRep_Health()
{
}
