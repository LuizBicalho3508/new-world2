#include "NWCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NWProceduralWorldManager.h"
#include "UObject/ConstructorHelpers.h"

ANWCharacter::ANWCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 620.0f, 0.0f);
    GetCharacterMovement()->JumpZVelocity = 620.0f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 430.0f;
    CameraBoom->SocketOffset = FVector(0.0f, 45.0f, 70.0f);
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 12.0f;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    DebugBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugBody"));
    DebugBody->SetupAttachment(GetCapsuleComponent());
    DebugBody->SetRelativeScale3D(FVector(0.58f, 0.58f, 1.65f));
    DebugBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleMesh(TEXT("/Engine/BasicShapes/Capsule.Capsule"));
    if (CapsuleMesh.Succeeded())
    {
        DebugBody->SetStaticMesh(CapsuleMesh.Object);
    }
}

void ANWCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        Health = MaxHealth;
    }
}

void ANWCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    check(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ANWCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ANWCharacter::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ANWCharacter::Turn);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ANWCharacter::LookUp);

    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &ANWCharacter::StartSprint);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &ANWCharacter::StopSprint);
    PlayerInputComponent->BindAction(TEXT("Attack"), IE_Pressed, this, &ANWCharacter::Attack);
    PlayerInputComponent->BindAction(TEXT("Ability1"), IE_Pressed, this, &ANWCharacter::Ability1);
    PlayerInputComponent->BindAction(TEXT("RegenerateWorld"), IE_Pressed, this, &ANWCharacter::RegenerateWorld);
}

void ANWCharacter::MoveForward(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator ControlRotation = Controller->GetControlRotation();
        const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
    }
}

void ANWCharacter::MoveRight(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator ControlRotation = Controller->GetControlRotation();
        const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
    }
}

void ANWCharacter::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void ANWCharacter::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

void ANWCharacter::StartSprint()
{
    ApplySprintState(true);
    if (!HasAuthority())
    {
        ServerSetSprinting(true);
    }
}

void ANWCharacter::StopSprint()
{
    ApplySprintState(false);
    if (!HasAuthority())
    {
        ServerSetSprinting(false);
    }
}

void ANWCharacter::Attack()
{
    if (HasAuthority())
    {
        ExecuteAttack();
    }
    else
    {
        ServerAttack();
    }
}

void ANWCharacter::Ability1()
{
    if (HasAuthority())
    {
        ExecuteAbility1();
    }
    else
    {
        ServerAbility1();
    }
}

void ANWCharacter::RegenerateWorld()
{
    if (HasAuthority())
    {
        ServerRegenerateWorld_Implementation();
    }
    else
    {
        ServerRegenerateWorld();
    }
}

void ANWCharacter::ServerAttack_Implementation()
{
    ExecuteAttack();
}

void ANWCharacter::ServerAbility1_Implementation()
{
    ExecuteAbility1();
}

void ANWCharacter::ServerSetSprinting_Implementation(bool bSprinting)
{
    ApplySprintState(bSprinting);
}

void ANWCharacter::ServerRegenerateWorld_Implementation()
{
    if (!GetWorld())
    {
        return;
    }

    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        It->AdvanceEpoch();
        break;
    }
}

void ANWCharacter::ExecuteAttack()
{
    if (!GetWorld())
    {
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    GetActorEyesViewPoint(ViewLocation, ViewRotation);

    const FVector Start = ViewLocation;
    const FVector End = Start + ViewRotation.Vector() * 260.0f;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NWAttack), false, this);
    TArray<FHitResult> Hits;

    const bool bHit = GetWorld()->SweepMultiByChannel(
        Hits,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(75.0f),
        QueryParams);

    if (!bHit)
    {
        return;
    }

    TSet<AActor*> DamagedActors;
    for (const FHitResult& Hit : Hits)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor || HitActor == this || DamagedActors.Contains(HitActor))
        {
            continue;
        }

        DamagedActors.Add(HitActor);
        UGameplayStatics::ApplyDamage(HitActor, AttackDamage, GetController(), this, UDamageType::StaticClass());
    }
}

void ANWCharacter::ExecuteAbility1()
{
    if (!GetWorld())
    {
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    if ((Now - LastAbilityTime) < AbilityCooldown)
    {
        return;
    }
    LastAbilityTime = Now;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NWAbility1), false, this);
    TArray<FOverlapResult> Overlaps;

    GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        GetActorLocation(),
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(430.0f),
        QueryParams);

    TSet<AActor*> DamagedActors;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* HitActor = Overlap.GetActor();
        if (!HitActor || HitActor == this || DamagedActors.Contains(HitActor))
        {
            continue;
        }

        DamagedActors.Add(HitActor);
        UGameplayStatics::ApplyDamage(HitActor, AbilityDamage, GetController(), this, UDamageType::StaticClass());
    }
}

void ANWCharacter::ApplySprintState(bool bSprinting)
{
    GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
}

float ANWCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
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
        SetActorLocation(FVector(0.0f, 0.0f, 1500.0f), false, nullptr, ETeleportType::TeleportPhysics);
    }

    return AppliedDamage;
}

void ANWCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWCharacter, Health);
}

void ANWCharacter::OnRep_Health()
{
}
