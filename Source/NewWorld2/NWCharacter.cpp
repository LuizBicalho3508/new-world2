#include "NWCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "NWCombatHUDWidget.h"
#include "NWCombatLibrary.h"
#include "NWEnemy.h"
#include "NWLootPickup.h"
#include "NWProceduralWorldManager.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr uint8 AnimAttack = 0;
    constexpr uint8 AnimAbility1 = 1;
    constexpr uint8 AnimAbility2 = 2;
    constexpr uint8 AnimAbility3 = 3;
    constexpr uint8 AnimDodge = 4;
    constexpr uint8 AnimBlock = 5;
    constexpr uint8 AnimParry = 6;
    constexpr uint8 AnimHit = 7;
    constexpr uint8 AnimStagger = 8;
    constexpr int32 WeaponCount = 7;
    constexpr int32 AbilitiesPerWeapon = 3;
    constexpr int32 TotalAbilityCooldowns = WeaponCount * AbilitiesPerWeapon;

    int32 GetCooldownSlot(ENWWeaponType WeaponType, int32 AbilityIndex)
    {
        return FMath::Clamp(static_cast<int32>(WeaponType), 0, WeaponCount - 1) * AbilitiesPerWeapon + FMath::Clamp(AbilityIndex, 0, AbilitiesPerWeapon - 1);
    }
}

ANWCharacter::ANWCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.02f;
    bReplicates = true;
    SetReplicateMovement(true);
    NetUpdateFrequency = 30.0f;

    AbilityReadyTimes.SetNumZeroed(TotalAbilityCooldowns);

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
    if (CapsuleMesh.Succeeded()) { DebugBody->SetStaticMesh(CapsuleMesh.Object); }
}

void ANWCharacter::BeginPlay()
{
    Super::BeginPlay();
    TryApplyLicensedCharacterVisual();

    if (HasAuthority())
    {
        EnsureStarterEquipment();
        RecalculateEquipmentStats();
        Health = MaxHealth;
        Stamina = MaxStamina;
        AbilityReadyTimes.SetNumZeroed(TotalAbilityCooldowns);
    }

    EnsureCombatHUD();
    LogLoadout();
}

void ANWCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();
    EnsureCombatHUD();
}

void ANWCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !GetWorld()) { return; }

    const float Now = GetWorld()->GetTimeSeconds();
    if ((CombatState == ENWCombatState::Dodging || CombatState == ENWCombatState::Staggered) && Now >= CombatStateEndTime)
    {
        CombatState = ENWCombatState::Normal;
    }

    if (CombatState == ENWCombatState::Blocking && Stamina <= KINDA_SMALL_NUMBER)
    {
        ApplyStagger(0.85f, nullptr);
    }

    const bool bCanRegen = CombatState != ENWCombatState::Blocking && CombatState != ENWCombatState::Dodging && CombatState != ENWCombatState::Staggered;
    if (bCanRegen && (Now - LastDamageTime) >= 0.55f && Stamina < MaxStamina)
    {
        Stamina = FMath::Min(MaxStamina, Stamina + 26.0f * DeltaSeconds);
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
    PlayerInputComponent->BindAction(TEXT("Block"), IE_Pressed, this, &ANWCharacter::StartBlock);
    PlayerInputComponent->BindAction(TEXT("Block"), IE_Released, this, &ANWCharacter::StopBlock);
    PlayerInputComponent->BindAction(TEXT("Dodge"), IE_Pressed, this, &ANWCharacter::Dodge);
    PlayerInputComponent->BindAction(TEXT("Ability1"), IE_Pressed, this, &ANWCharacter::Ability1);
    PlayerInputComponent->BindAction(TEXT("Ability2"), IE_Pressed, this, &ANWCharacter::Ability2);
    PlayerInputComponent->BindAction(TEXT("Ability3"), IE_Pressed, this, &ANWCharacter::Ability3);
    PlayerInputComponent->BindAction(TEXT("PrimaryWeapon"), IE_Pressed, this, &ANWCharacter::SelectPrimaryWeapon);
    PlayerInputComponent->BindAction(TEXT("SecondaryWeapon"), IE_Pressed, this, &ANWCharacter::SelectSecondaryWeapon);
    PlayerInputComponent->BindAction(TEXT("QuickSwap"), IE_Pressed, this, &ANWCharacter::QuickSwapWeapon);
    PlayerInputComponent->BindAction(TEXT("CyclePrimaryWeapon"), IE_Pressed, this, &ANWCharacter::CyclePrimaryWeapon);
    PlayerInputComponent->BindAction(TEXT("CycleSecondaryWeapon"), IE_Pressed, this, &ANWCharacter::CycleSecondaryWeapon);
    PlayerInputComponent->BindAction(TEXT("PickupLoot"), IE_Pressed, this, &ANWCharacter::PickupNearestLoot);
    PlayerInputComponent->BindAction(TEXT("Inventory"), IE_Pressed, this, &ANWCharacter::ToggleInventory);
    PlayerInputComponent->BindAction(TEXT("InventoryPrev"), IE_Pressed, this, &ANWCharacter::InventoryPrevious);
    PlayerInputComponent->BindAction(TEXT("InventoryNext"), IE_Pressed, this, &ANWCharacter::InventoryNext);
    PlayerInputComponent->BindAction(TEXT("InventoryEquip"), IE_Pressed, this, &ANWCharacter::EquipSelectedInventoryItem);
    PlayerInputComponent->BindAction(TEXT("RegenerateWorld"), IE_Pressed, this, &ANWCharacter::RegenerateWorld);
}

void ANWCharacter::MoveForward(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value) && CombatState != ENWCombatState::Staggered)
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
    }
}

void ANWCharacter::MoveRight(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value) && CombatState != ENWCombatState::Staggered)
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
    }
}

void ANWCharacter::Turn(float Value) { AddControllerYawInput(Value); }
void ANWCharacter::LookUp(float Value) { AddControllerPitchInput(Value); }

void ANWCharacter::StartSprint()
{
    if (CombatState != ENWCombatState::Normal) { return; }
    ApplySprintState(true);
    if (!HasAuthority()) { ServerSetSprinting(true); }
}

void ANWCharacter::StopSprint()
{
    ApplySprintState(false);
    if (!HasAuthority()) { ServerSetSprinting(false); }
}

void ANWCharacter::Attack()
{
    if (CombatState == ENWCombatState::Dodging || CombatState == ENWCombatState::Staggered) { return; }
    if (HasAuthority()) { ExecuteAttack(); }
    else { ServerAttack(); }
}

void ANWCharacter::Ability1() { if (CombatState == ENWCombatState::Normal) { if (HasAuthority()) ExecuteAbility(0); else ServerUseAbility(0); } }
void ANWCharacter::Ability2() { if (CombatState == ENWCombatState::Normal) { if (HasAuthority()) ExecuteAbility(1); else ServerUseAbility(1); } }
void ANWCharacter::Ability3() { if (CombatState == ENWCombatState::Normal) { if (HasAuthority()) ExecuteAbility(2); else ServerUseAbility(2); } }

void ANWCharacter::SelectPrimaryWeapon()
{
    SetActiveWeaponInternal(PrimaryWeapon);
    if (!HasAuthority()) { ServerSetActiveWeapon(PrimaryWeapon); }
}

void ANWCharacter::SelectSecondaryWeapon()
{
    SetActiveWeaponInternal(SecondaryWeapon);
    if (!HasAuthority()) { ServerSetActiveWeapon(SecondaryWeapon); }
}

void ANWCharacter::QuickSwapWeapon()
{
    const ENWWeaponType Requested = ActiveWeapon == PrimaryWeapon ? SecondaryWeapon : PrimaryWeapon;
    SetActiveWeaponInternal(Requested);
    if (!HasAuthority()) { ServerSetActiveWeapon(Requested); }
}

void ANWCharacter::CyclePrimaryWeapon()
{
    if (HasAuthority()) { ServerCycleLoadout_Implementation(true); }
    else { ServerCycleLoadout(true); }
}

void ANWCharacter::CycleSecondaryWeapon()
{
    if (HasAuthority()) { ServerCycleLoadout_Implementation(false); }
    else { ServerCycleLoadout(false); }
}

void ANWCharacter::RegenerateWorld()
{
    if (HasAuthority()) { ServerRegenerateWorld_Implementation(); }
    else { ServerRegenerateWorld(); }
}

void ANWCharacter::StartBlock()
{
    if (HasAuthority()) { ExecuteSetBlocking(true); }
    else { ServerSetBlocking(true); }
}

void ANWCharacter::StopBlock()
{
    if (HasAuthority()) { ExecuteSetBlocking(false); }
    else { ServerSetBlocking(false); }
}

void ANWCharacter::Dodge()
{
    FVector Direction = GetLastMovementInputVector();
    if (Direction.IsNearlyZero()) { Direction = GetVelocity().GetSafeNormal2D(); }
    if (Direction.IsNearlyZero()) { Direction = GetActorForwardVector(); }
    Direction.Z = 0.0f;
    Direction.Normalize();

    if (HasAuthority()) { ExecuteDodge(Direction); }
    else { ServerDodge(Direction); }
}

void ANWCharacter::PickupNearestLoot()
{
    if (HasAuthority()) { ServerPickupNearestLoot_Implementation(); }
    else { ServerPickupNearestLoot(); }
}

void ANWCharacter::ToggleInventory()
{
    EnsureCombatHUD();
    if (CombatHUD) { CombatHUD->ToggleInventory(); }
}

void ANWCharacter::InventoryPrevious()
{
    if (!CombatHUD || !CombatHUD->IsInventoryVisible() || InventoryItems.IsEmpty()) { return; }
    SelectedInventoryIndex = (SelectedInventoryIndex - 1 + InventoryItems.Num()) % InventoryItems.Num();
}

void ANWCharacter::InventoryNext()
{
    if (!CombatHUD || !CombatHUD->IsInventoryVisible() || InventoryItems.IsEmpty()) { return; }
    SelectedInventoryIndex = (SelectedInventoryIndex + 1) % InventoryItems.Num();
}

void ANWCharacter::EquipSelectedInventoryItem()
{
    if (!CombatHUD || !CombatHUD->IsInventoryVisible() || !InventoryItems.IsValidIndex(SelectedInventoryIndex)) { return; }
    const int32 Seed = InventoryItems[SelectedInventoryIndex].ItemSeed;
    if (HasAuthority()) { EquipInventoryItemBySeed(Seed); }
    else { ServerEquipInventoryItem(Seed); }
}

void ANWCharacter::ServerAttack_Implementation() { ExecuteAttack(); }
void ANWCharacter::ServerUseAbility_Implementation(uint8 AbilityIndex) { ExecuteAbility(AbilityIndex); }
void ANWCharacter::ServerSetSprinting_Implementation(bool bSprinting) { ApplySprintState(bSprinting); }
void ANWCharacter::ServerSetBlocking_Implementation(bool bBlocking) { ExecuteSetBlocking(bBlocking); }
void ANWCharacter::ServerDodge_Implementation(FVector_NetQuantizeNormal Direction) { ExecuteDodge(Direction); }
void ANWCharacter::ServerEquipInventoryItem_Implementation(int32 ItemSeed) { EquipInventoryItemBySeed(ItemSeed); }

void ANWCharacter::ServerSetActiveWeapon_Implementation(ENWWeaponType RequestedWeapon)
{
    if (RequestedWeapon == PrimaryWeapon || RequestedWeapon == SecondaryWeapon) { SetActiveWeaponInternal(RequestedWeapon); }
}

void ANWCharacter::ServerCycleLoadout_Implementation(bool bPrimarySlot)
{
    ENWWeaponType& Slot = bPrimarySlot ? PrimaryWeapon : SecondaryWeapon;
    const ENWWeaponType Other = bPrimarySlot ? SecondaryWeapon : PrimaryWeapon;
    const ENWWeaponType Previous = Slot;

    Slot = NWCombat::GetNextWeaponType(Slot);
    if (Slot == Other) { Slot = NWCombat::GetNextWeaponType(Slot); }
    if (ActiveWeapon == Previous) { ActiveWeapon = Slot; }
    LogLoadout();
}

void ANWCharacter::ServerRegenerateWorld_Implementation()
{
    if (!GetWorld()) { return; }
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        It->AdvanceEpoch();
        break;
    }
}

void ANWCharacter::ServerPickupNearestLoot_Implementation()
{
    if (!GetWorld()) { return; }

    ANWLootPickup* BestPickup = nullptr;
    float BestDistanceSq = FMath::Square(PickupRadius);
    for (TActorIterator<ANWLootPickup> It(GetWorld()); It; ++It)
    {
        const float DistanceSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            BestPickup = *It;
        }
    }
    if (BestPickup) { BestPickup->TryPickup(this); }
}

void ANWCharacter::ExecuteAttack()
{
    if (!GetWorld() || CombatState == ENWCombatState::Dodging || CombatState == ENWCombatState::Staggered) { return; }
    if (CombatState == ENWCombatState::Blocking) { ExecuteSetBlocking(false); }

    MulticastPlayCombatAnimation(AnimAttack);

    const FNWWeaponDefinition Weapon = NWCombat::GetWeaponDefinition(ActiveWeapon);
    FVector ViewLocation;
    FRotator ViewRotation;
    GetActorEyesViewPoint(ViewLocation, ViewRotation);

    const FVector End = ViewLocation + ViewRotation.Vector() * Weapon.BasicRange;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NWAttack), false, this);
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByChannel(Hits, ViewLocation, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Weapon.BasicRadius), QueryParams);

    TSet<AActor*> DamagedActors;
    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (!Target || Target == this || DamagedActors.Contains(Target)) { continue; }
        DamagedActors.Add(Target);
        ApplyWeaponDamage(Target, Weapon.BasicDamage, true, false);

        if (ActiveWeapon == ENWWeaponType::Staff || ActiveWeapon == ENWWeaponType::Bow || ActiveWeapon == ENWWeaponType::Firearm) { break; }
    }
}

void ANWCharacter::ExecuteAbility(uint8 AbilityIndex)
{
    if (!GetWorld() || AbilityIndex >= AbilitiesPerWeapon || CombatState != ENWCombatState::Normal) { return; }
    if (AbilityReadyTimes.Num() < TotalAbilityCooldowns) { AbilityReadyTimes.SetNumZeroed(TotalAbilityCooldowns); }

    const FNWWeaponDefinition Weapon = NWCombat::GetWeaponDefinition(ActiveWeapon);
    if (!Weapon.Abilities.IsValidIndex(AbilityIndex)) { return; }

    const int32 CooldownSlot = GetCooldownSlot(ActiveWeapon, AbilityIndex);
    const FNWWeaponAbilityDefinition& Ability = Weapon.Abilities[AbilityIndex];
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < AbilityReadyTimes[CooldownSlot]) { return; }

    const float EffectiveCooldown = FMath::Max(0.8f, Ability.Cooldown * CooldownMultiplier);
    AbilityReadyTimes[CooldownSlot] = Now + EffectiveCooldown;

    float ComboMultiplier = 1.0f;
    if ((Now - LastAbilityTime) <= ComboWindowSeconds && LastAbilityWeapon != ActiveWeapon)
    {
        ComboMultiplier = CrossWeaponComboMultiplier;
        UE_LOG(LogTemp, Display, TEXT("[COMBO] %s -> %s | x%.2f"), *NWCombat::WeaponTypeToString(LastAbilityWeapon), *Weapon.Name, ComboMultiplier);
    }

    LastAbilityTime = Now;
    LastAbilityWeapon = ActiveWeapon;
    MulticastPlayCombatAnimation(AbilityIndex == 0 ? AnimAbility1 : AbilityIndex == 1 ? AnimAbility2 : AnimAbility3);

    if (Ability.Kind == ENWAbilityKind::Heal)
    {
        const float HealAmount = Ability.Power * HealingMultiplier;
        Health = FMath::Clamp(Health + HealAmount, 0.0f, MaxHealth);
        UE_LOG(LogTemp, Display, TEXT("[ABILITY] %s/%s curou %.1f. Vida %.1f/%.1f"), *Weapon.Name, *Ability.Name, HealAmount, Health, MaxHealth);
        return;
    }

    ExecuteDamageAbility(Ability, ComboMultiplier);
}

void ANWCharacter::ExecuteDamageAbility(const FNWWeaponAbilityDefinition& Ability, float ComboMultiplier)
{
    FVector ViewLocation;
    FRotator ViewRotation;
    GetActorEyesViewPoint(ViewLocation, ViewRotation);

    if (Ability.Kind == ENWAbilityKind::DirectDamage)
    {
        const FVector End = ViewLocation + ViewRotation.Vector() * Ability.Range;
        TArray<FHitResult> Hits;
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NWDirectAbility), false, this);
        GetWorld()->SweepMultiByChannel(Hits, ViewLocation, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Ability.Radius), QueryParams);

        for (const FHitResult& Hit : Hits)
        {
            AActor* Target = Hit.GetActor();
            if (!Target || Target == this) { continue; }
            ApplyWeaponDamage(Target, Ability.Power * ComboMultiplier, true, true);
            break;
        }
        return;
    }

    const bool bRangedArea = ActiveWeapon == ENWWeaponType::Staff || ActiveWeapon == ENWWeaponType::Bow || ActiveWeapon == ENWWeaponType::Firearm;
    const FVector Center = bRangedArea ? GetActorLocation() + ViewRotation.Vector() * Ability.Range : GetActorLocation() + ViewRotation.Vector() * FMath::Min(Ability.Range, 260.0f);

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NWAreaAbility), false, this);
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Ability.Radius), QueryParams);

    TSet<AActor*> DamagedActors;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Target = Overlap.GetActor();
        if (!Target || Target == this || DamagedActors.Contains(Target)) { continue; }
        DamagedActors.Add(Target);
        ApplyWeaponDamage(Target, Ability.Power * ComboMultiplier, true, true);
    }
}

void ANWCharacter::ExecuteSetBlocking(bool bBlocking)
{
    if (!GetWorld()) { return; }

    if (!bBlocking)
    {
        if (CombatState == ENWCombatState::Blocking) { CombatState = ENWCombatState::Normal; }
        ParryWindowEndTime = -1000.0f;
        return;
    }

    if (CombatState != ENWCombatState::Normal || Stamina <= 5.0f) { return; }
    CombatState = ENWCombatState::Blocking;
    ParryWindowEndTime = GetWorld()->GetTimeSeconds() + 0.22f;
    ApplySprintState(false);
    MulticastPlayCombatAnimation(AnimBlock);
}

void ANWCharacter::ExecuteDodge(const FVector& Direction)
{
    if (!GetWorld() || CombatState == ENWCombatState::Dodging || CombatState == ENWCombatState::Staggered || Stamina < 22.0f) { return; }
    if (CombatState == ENWCombatState::Blocking) { ExecuteSetBlocking(false); }

    Stamina = FMath::Max(0.0f, Stamina - 22.0f);
    CombatState = ENWCombatState::Dodging;
    const float Now = GetWorld()->GetTimeSeconds();
    InvulnerableUntilTime = Now + 0.28f;
    CombatStateEndTime = Now + 0.48f;
    if (DodgeEmpowerMagnitude > 0.0f) { DodgeEmpowerEndTime = Now + 3.5f; }

    const FVector SafeDirection = Direction.IsNearlyZero() ? GetActorForwardVector() : Direction.GetSafeNormal2D();
    LaunchCharacter(SafeDirection * 760.0f + FVector(0.0f, 0.0f, 35.0f), true, false);
    MulticastPlayCombatAnimation(AnimDodge);
}

void ANWCharacter::ApplySprintState(bool bSprinting)
{
    if (CombatState != ENWCombatState::Normal && bSprinting) { return; }
    GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
}

float ANWCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || DamageAmount <= 0.0f || !GetWorld()) { return 0.0f; }

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now <= InvulnerableUntilTime) { return 0.0f; }

    float AdjustedDamage = DamageAmount;
    const FVector ToSource = DamageCauser ? (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
    const float FrontalDot = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToSource);

    if (CombatState == ENWCombatState::Blocking && FrontalDot > -0.10f)
    {
        if (Now <= ParryWindowEndTime)
        {
            Stamina = FMath::Min(MaxStamina, Stamina + 7.0f);
            if (ParryHealMagnitude > 0.0f) { Health = FMath::Min(MaxHealth, Health + ParryHealMagnitude); }
            if (ANWEnemy* Enemy = Cast<ANWEnemy>(DamageCauser)) { Enemy->ApplyStagger(1.15f); }
            if (ANWCharacter* OtherCharacter = Cast<ANWCharacter>(DamageCauser)) { OtherCharacter->ApplyStagger(1.15f, this); }
            MulticastPlayCombatAnimation(AnimParry);
            UE_LOG(LogTemp, Display, TEXT("[PARRY] perfeito | stamina %.0f | vida %.0f"), Stamina, Health);
            return 0.0f;
        }

        const float BlockReduction = FMath::Clamp(0.62f + GuardMagnitude * 0.012f, 0.62f, 0.90f);
        const float StaminaFactor = FMath::Clamp(0.48f - GuardMagnitude * 0.006f, 0.24f, 0.48f);
        Stamina = FMath::Max(0.0f, Stamina - FMath::Max(4.0f, DamageAmount * StaminaFactor));
        AdjustedDamage *= (1.0f - BlockReduction);
        if (Stamina <= KINDA_SMALL_NUMBER) { ApplyStagger(0.85f, DamageCauser); }
    }

    AdjustedDamage *= (1.0f - FMath::Clamp(ArmorValue * 0.006f, 0.0f, 0.45f));

    const float AppliedDamage = Super::TakeDamage(AdjustedDamage, DamageEvent, EventInstigator, DamageCauser);
    if (AppliedDamage <= 0.0f) { return AppliedDamage; }

    LastDamageTime = Now;
    Health = FMath::Clamp(Health - AppliedDamage, 0.0f, MaxHealth);
    if (CombatState == ENWCombatState::Normal)
    {
        MulticastPlayCombatAnimation(AnimHit);
        if (AppliedDamage >= MaxHealth * 0.28f) { ApplyStagger(0.34f, DamageCauser); }
    }

    if (Health <= 0.0f)
    {
        Health = MaxHealth;
        Stamina = MaxStamina;
        CombatState = ENWCombatState::Normal;
        SetActorLocation(FVector(0.0f, 0.0f, 1500.0f), false, nullptr, ETeleportType::TeleportPhysics);
    }
    return AppliedDamage;
}

void ANWCharacter::ApplyStagger(float DurationSeconds, AActor* SourceActor)
{
    if (!HasAuthority() || !GetWorld()) { return; }

    CombatState = ENWCombatState::Staggered;
    CombatStateEndTime = FMath::Max(CombatStateEndTime, GetWorld()->GetTimeSeconds() + FMath::Max(0.12f, DurationSeconds));
    ParryWindowEndTime = -1000.0f;
    GetCharacterMovement()->StopMovementImmediately();

    if (SourceActor)
    {
        const FVector Away = (GetActorLocation() - SourceActor->GetActorLocation()).GetSafeNormal2D();
        LaunchCharacter(Away * 145.0f, true, false);
    }
    MulticastPlayCombatAnimation(AnimStagger);
}

void ANWCharacter::ApplyWeaponDamage(AActor* Target, float BaseDamage, bool bAllowStatusEffects, bool bFromAbility)
{
    if (!HasAuthority() || !Target || !GetWorld()) { return; }

    bool bCritical = false;
    float Damage = RollDamageWithStats(BaseDamage, bCritical);

    if (GetWorld()->GetTimeSeconds() <= DodgeEmpowerEndTime && DodgeEmpowerMagnitude > 0.0f)
    {
        Damage *= 1.0f + FMath::Clamp(DodgeEmpowerMagnitude * 0.02f, 0.05f, 0.45f);
        DodgeEmpowerEndTime = -1000.0f;
    }
    if (GetTargetHealthRatio(Target) <= 0.30f && ExecutionerMagnitude > 0.0f)
    {
        Damage *= 1.0f + FMath::Clamp(ExecutionerMagnitude * 0.025f, 0.05f, 0.50f);
    }

    UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, UDamageType::StaticClass());
    if (LifeStealPercent > 0.0f) { Health = FMath::Clamp(Health + Damage * LifeStealPercent, 0.0f, MaxHealth); }

    if (bCritical && CooldownOnCritMagnitude > 0.0f)
    {
        const float Now = GetWorld()->GetTimeSeconds();
        const float Reduction = FMath::Clamp(CooldownOnCritMagnitude * 0.04f, 0.08f, 0.75f);
        for (float& ReadyAt : AbilityReadyTimes) { ReadyAt = FMath::Max(Now, ReadyAt - Reduction); }
    }

    if (!bAllowStatusEffects) { return; }
    if (PoisonDamagePerTick > 0.0f && NWCombat::IsPoisonCompatible(ActiveWeapon)) { ApplyStatusDamage(Target, PoisonDamagePerTick, 3, 1.0f); }
    if (FireDamagePerTick > 0.0f) { ApplyStatusDamage(Target, FireDamagePerTick, 3, 0.75f); }
    if (BleedDamagePerTick > 0.0f && ActiveWeapon != ENWWeaponType::Staff && ActiveWeapon != ENWWeaponType::Firearm) { ApplyStatusDamage(Target, BleedDamagePerTick, 4, 0.65f); }

    if (bFromAbility)
    {
        if (FrostbiteMagnitude > 0.0f) { ApplyFrostbite(Target); }
        if (ShockChainMagnitude > 0.0f) { ApplyShockChain(Target, Damage); }

        const float EchoChance = FMath::Clamp(AbilityEchoMagnitude * 0.012f, 0.0f, 0.42f);
        if (EchoChance > 0.0f && FMath::FRand() < EchoChance)
        {
            TWeakObjectPtr<AActor> WeakTarget(Target);
            TWeakObjectPtr<ANWCharacter> WeakSelf(this);
            FTimerHandle Handle;
            GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakTarget, WeakSelf, Damage]()
            {
                if (WeakTarget.IsValid() && WeakSelf.IsValid())
                {
                    UGameplayStatics::ApplyDamage(WeakTarget.Get(), Damage * 0.38f, WeakSelf->GetController(), WeakSelf.Get(), UDamageType::StaticClass());
                }
            }), 0.22f, false);
        }
    }
}

void ANWCharacter::ApplyStatusDamage(AActor* Target, float DamagePerTick, int32 Ticks, float IntervalSeconds)
{
    if (!GetWorld() || !Target || DamagePerTick <= 0.0f || Ticks <= 0) { return; }

    TWeakObjectPtr<AActor> WeakTarget(Target);
    TWeakObjectPtr<ANWCharacter> WeakSelf(this);
    TSharedRef<int32> RemainingTicks = MakeShared<int32>(Ticks);
    TSharedRef<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();

    GetWorldTimerManager().SetTimer(*TimerHandle, FTimerDelegate::CreateLambda([WeakTarget, WeakSelf, RemainingTicks, TimerHandle, DamagePerTick]()
    {
        if (!WeakSelf.IsValid() || !WeakTarget.IsValid() || *RemainingTicks <= 0)
        {
            if (WeakSelf.IsValid()) { WeakSelf->GetWorldTimerManager().ClearTimer(*TimerHandle); }
            return;
        }
        UGameplayStatics::ApplyDamage(WeakTarget.Get(), DamagePerTick, WeakSelf->GetController(), WeakSelf.Get(), UDamageType::StaticClass());
        --(*RemainingTicks);
        if (*RemainingTicks <= 0) { WeakSelf->GetWorldTimerManager().ClearTimer(*TimerHandle); }
    }), IntervalSeconds, true, IntervalSeconds);
}

void ANWCharacter::ApplyShockChain(AActor* PrimaryTarget, float BaseDamage)
{
    if (!GetWorld() || !PrimaryTarget) { return; }

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NWShockChain), false, this);
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(Overlaps, PrimaryTarget->GetActorLocation(), FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(360.0f), QueryParams);

    const float ChainDamage = BaseDamage * FMath::Clamp(ShockChainMagnitude * 0.018f, 0.08f, 0.35f);
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Other = Overlap.GetActor();
        if (!Other || Other == this || Other == PrimaryTarget) { continue; }
        UGameplayStatics::ApplyDamage(Other, ChainDamage, GetController(), this, UDamageType::StaticClass());
        break;
    }
}

void ANWCharacter::ApplyFrostbite(AActor* Target)
{
    ACharacter* TargetCharacter = Cast<ACharacter>(Target);
    if (!TargetCharacter || !GetWorld()) { return; }

    UCharacterMovementComponent* Movement = TargetCharacter->GetCharacterMovement();
    if (!Movement) { return; }

    const float OriginalSpeed = Movement->MaxWalkSpeed;
    Movement->MaxWalkSpeed = OriginalSpeed * FMath::Clamp(1.0f - FrostbiteMagnitude * 0.018f, 0.52f, 0.88f);

    TWeakObjectPtr<ACharacter> WeakCharacter(TargetCharacter);
    FTimerHandle Handle;
    GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakCharacter, OriginalSpeed]()
    {
        if (WeakCharacter.IsValid() && WeakCharacter->GetCharacterMovement()) { WeakCharacter->GetCharacterMovement()->MaxWalkSpeed = OriginalSpeed; }
    }), 1.55f, false);
}

float ANWCharacter::GetTargetHealthRatio(AActor* Target) const
{
    if (const ANWCharacter* OtherCharacter = Cast<ANWCharacter>(Target))
    {
        return OtherCharacter->GetMaxHealth() > 0.0f ? OtherCharacter->GetHealth() / OtherCharacter->GetMaxHealth() : 1.0f;
    }
    if (const ANWEnemy* Enemy = Cast<ANWEnemy>(Target)) { return Enemy->GetHealthRatio(); }
    return 1.0f;
}

void ANWCharacter::SetActiveWeaponInternal(ENWWeaponType RequestedWeapon)
{
    if (RequestedWeapon != PrimaryWeapon && RequestedWeapon != SecondaryWeapon) { return; }
    ActiveWeapon = RequestedWeapon;
    LogLoadout();
}

bool ANWCharacter::TryAddInventoryItem(const FNWGeneratedItem& Item)
{
    if (!HasAuthority() || InventoryItems.Num() >= InventoryCapacity) { return false; }
    InventoryItems.Add(Item);
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("[INVENTARIO] coletado %s | %d/%d"), *Item.Name, InventoryItems.Num(), InventoryCapacity);
    return true;
}

void ANWCharacter::EquipInventoryItemBySeed(int32 ItemSeed)
{
    if (!HasAuthority()) { return; }

    const int32 InventoryIndex = InventoryItems.IndexOfByPredicate([ItemSeed](const FNWGeneratedItem& Item) { return Item.ItemSeed == ItemSeed; });
    if (InventoryIndex == INDEX_NONE) { return; }

    const FNWGeneratedItem NewItem = InventoryItems[InventoryIndex];
    InventoryItems.RemoveAt(InventoryIndex);

    const int32 EquippedIndex = EquippedItems.IndexOfByPredicate([&NewItem](const FNWGeneratedItem& Item) { return Item.Slot == NewItem.Slot; });
    if (EquippedIndex != INDEX_NONE)
    {
        const FNWGeneratedItem OldItem = EquippedItems[EquippedIndex];
        EquippedItems[EquippedIndex] = NewItem;
        if (InventoryItems.Num() < InventoryCapacity) { InventoryItems.Add(OldItem); }
    }
    else
    {
        EquippedItems.Add(NewItem);
    }

    SelectedInventoryIndex = FMath::Clamp(SelectedInventoryIndex, 0, FMath::Max(0, InventoryItems.Num() - 1));
    RecalculateEquipmentStats();
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("[EQUIP] %s equipado | classe %s | estilo %s"), *NewItem.Name, *NWCombat::ArmorWeightToString(NewItem.ArmorWeight), *NewItem.StyleId.ToString());
}

void ANWCharacter::EnsureStarterEquipment()
{
    if (!EquippedItems.IsEmpty()) { return; }

    FNWGeneratedItem Gloves;
    Gloves.ItemSeed = 1001;
    Gloves.ItemLevel = 1;
    Gloves.AppearanceSeed = 11001;
    Gloves.Name = TEXT("Luvas do Alquimista - Prototipo");
    Gloves.Slot = ENWEquipmentSlot::Gloves;
    Gloves.ArmorWeight = ENWArmorWeight::Medium;
    Gloves.StyleId = TEXT("Battlemage");
    Gloves.Rarity = ENWItemRarity::Rare;
    Gloves.Affixes = { { ENWAffixType::PoisonCoating, 5.0f }, { ENWAffixType::ParryHeal, 3.0f } };
    EquippedItems.Add(Gloves);

    FNWGeneratedItem Chest;
    Chest.ItemSeed = 1002;
    Chest.ItemLevel = 1;
    Chest.AppearanceSeed = 11002;
    Chest.Name = TEXT("Peitoral do Vanguardista - Prototipo");
    Chest.Slot = ENWEquipmentSlot::Chest;
    Chest.ArmorWeight = ENWArmorWeight::Heavy;
    Chest.StyleId = TEXT("IronVanguard");
    Chest.Rarity = ENWItemRarity::Uncommon;
    Chest.Affixes = { { ENWAffixType::Armor, 4.0f }, { ENWAffixType::Vitality, 3.0f }, { ENWAffixType::FortifiedGuard, 3.0f } };
    EquippedItems.Add(Chest);

    FNWGeneratedItem Boots;
    Boots.ItemSeed = 1003;
    Boots.ItemLevel = 1;
    Boots.AppearanceSeed = 11003;
    Boots.Name = TEXT("Botas do Vento - Prototipo");
    Boots.Slot = ENWEquipmentSlot::Boots;
    Boots.ArmorWeight = ENWArmorWeight::Light;
    Boots.StyleId = TEXT("Ranger");
    Boots.Rarity = ENWItemRarity::Uncommon;
    Boots.Affixes = { { ENWAffixType::DodgeEmpower, 3.0f }, { ENWAffixType::Haste, 2.0f } };
    EquippedItems.Add(Boots);
}

void ANWCharacter::RecalculateEquipmentStats()
{
    const float Vitality = GetAffixTotal(ENWAffixType::Vitality);
    const float Power = GetAffixTotal(ENWAffixType::Power);
    const float Healing = GetAffixTotal(ENWAffixType::Healing);
    const float Haste = GetAffixTotal(ENWAffixType::Haste);
    const float Precision = GetAffixTotal(ENWAffixType::Precision);
    const float LifeSteal = GetAffixTotal(ENWAffixType::LifeSteal);

    MaxHealth = BaseMaxHealth + Vitality * 2.5f;
    MaxStamina = BaseMaxStamina + Vitality * 0.45f;
    DamageMultiplier = 1.0f + Power * 0.018f;
    HealingMultiplier = 1.0f + Healing * 0.025f;
    CooldownMultiplier = FMath::Clamp(1.0f - Haste * 0.012f, 0.55f, 1.0f);
    PrecisionChance = FMath::Clamp(Precision * 0.012f, 0.0f, 0.35f);
    LifeStealPercent = FMath::Clamp(LifeSteal * 0.006f, 0.0f, 0.20f);
    ArmorValue = GetAffixTotal(ENWAffixType::Armor);
    PoisonDamagePerTick = GetAffixTotal(ENWAffixType::PoisonCoating);
    FireDamagePerTick = GetAffixTotal(ENWAffixType::Firebrand) * 0.75f;
    FrostbiteMagnitude = GetAffixTotal(ENWAffixType::Frostbite);
    ShockChainMagnitude = GetAffixTotal(ENWAffixType::ShockChain);
    AbilityEchoMagnitude = GetAffixTotal(ENWAffixType::AbilityEcho);
    CooldownOnCritMagnitude = GetAffixTotal(ENWAffixType::CooldownOnCrit);
    GuardMagnitude = GetAffixTotal(ENWAffixType::FortifiedGuard);
    ParryHealMagnitude = GetAffixTotal(ENWAffixType::ParryHeal);
    DodgeEmpowerMagnitude = GetAffixTotal(ENWAffixType::DodgeEmpower);
    BleedDamagePerTick = GetAffixTotal(ENWAffixType::Bleed) * 0.65f;
    ExecutionerMagnitude = GetAffixTotal(ENWAffixType::Executioner);

    Health = FMath::Clamp(Health, 0.0f, MaxHealth);
    Stamina = FMath::Clamp(Stamina, 0.0f, MaxStamina);
}

float ANWCharacter::GetAffixTotal(ENWAffixType AffixType) const
{
    float Total = 0.0f;
    for (const FNWGeneratedItem& Item : EquippedItems)
    {
        for (const FNWItemAffix& Affix : Item.Affixes)
        {
            if (Affix.Type == AffixType) { Total += Affix.Magnitude; }
        }
    }
    return Total;
}

bool ANWCharacter::HasAffix(ENWAffixType AffixType) const
{
    return GetAffixTotal(AffixType) > 0.0f;
}

float ANWCharacter::RollDamageWithStats(float BaseDamage, bool& bOutCritical) const
{
    float Result = BaseDamage * DamageMultiplier;
    bOutCritical = FMath::FRand() < PrecisionChance;
    if (bOutCritical) { Result *= 1.5f; }
    return Result;
}

float ANWCharacter::GetAbilityCooldownRemaining(int32 AbilityIndex) const
{
    if (!GetWorld() || AbilityIndex < 0 || AbilityIndex >= AbilitiesPerWeapon) { return 0.0f; }
    const int32 CooldownSlot = GetCooldownSlot(ActiveWeapon, AbilityIndex);
    if (!AbilityReadyTimes.IsValidIndex(CooldownSlot)) { return 0.0f; }
    return FMath::Max(0.0f, AbilityReadyTimes[CooldownSlot] - GetWorld()->GetTimeSeconds());
}

FString ANWCharacter::GetCombatStateLabel() const
{
    if (CombatState == ENWCombatState::Blocking && GetWorld() && GetWorld()->GetTimeSeconds() <= ParryWindowEndTime) { return TEXT("GUARDA: JANELA DE PARRY"); }
    switch (CombatState)
    {
        case ENWCombatState::Blocking: return TEXT("BLOQUEANDO");
        case ENWCombatState::Dodging: return TEXT("ESQUIVA");
        case ENWCombatState::Staggered: return TEXT("STAGGER");
        default: return TEXT("COMBATE PRONTO");
    }
}

FString ANWCharacter::GetNearbyLootLabel() const
{
    if (!GetWorld()) { return FString(); }

    const ANWLootPickup* BestPickup = nullptr;
    float BestDistanceSq = FMath::Square(PickupRadius);
    for (TActorIterator<ANWLootPickup> It(GetWorld()); It; ++It)
    {
        const float DistanceSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            BestPickup = *It;
        }
    }
    return BestPickup ? BestPickup->GetItem().Name : FString();
}

FString ANWCharacter::GetActiveWeaponName() const
{
    return NWCombat::WeaponTypeToString(ActiveWeapon);
}

void ANWCharacter::LogLoadout() const
{
    UE_LOG(LogTemp, Display, TEXT("[LOADOUT] Slot 1=%s | Slot 2=%s | Ativa=%s"), *NWCombat::WeaponTypeToString(PrimaryWeapon), *NWCombat::WeaponTypeToString(SecondaryWeapon), *NWCombat::WeaponTypeToString(ActiveWeapon));
}

void ANWCharacter::EnsureCombatHUD()
{
    if (!IsLocallyControlled() || CombatHUD) { return; }
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) { return; }

    CombatHUD = CreateWidget<UNWCombatHUDWidget>(PC, UNWCombatHUDWidget::StaticClass());
    if (CombatHUD)
    {
        CombatHUD->SetObservedCharacter(this);
        CombatHUD->AddToViewport(20);
    }
}

void ANWCharacter::TryApplyLicensedCharacterVisual()
{
    USkeletalMesh* LicensedMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone.Greystone"));
    UClass* LicensedAnimClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Greystone_AnimBlueprint.Greystone_AnimBlueprint_C"));

    if (!LicensedMesh || !LicensedAnimClass)
    {
        UE_LOG(LogTemp, Display, TEXT("[VISUAL] Paragon Greystone nao instalado; usando placeholder seguro."));
        return;
    }

    GetMesh()->SetSkeletalMeshAsset(LicensedMesh);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    GetMesh()->SetAnimInstanceClass(LicensedAnimClass);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetVisibility(true, true);
    DebugBody->SetVisibility(false, true);
    UE_LOG(LogTemp, Display, TEXT("[VISUAL] personagem realista Greystone ativado a partir do pack gratuito instalado localmente."));
}

void ANWCharacter::MulticastPlayCombatAnimation_Implementation(uint8 ActionCode)
{
    PlayCombatAnimationLocal(ActionCode);
}

void ANWCharacter::PlayCombatAnimationLocal(uint8 ActionCode)
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance) { return; }

    if (ActionCode == AnimAttack)
    {
        static const TCHAR* AttackMontages[] = {
            TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryA_Montage.Attack_PrimaryA_Montage"),
            TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryB_Montage.Attack_PrimaryB_Montage"),
            TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryC_Montage.Attack_PrimaryC_Montage")
        };
        if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, AttackMontages[AttackAnimationIndex % 3]))
        {
            AnimInstance->Montage_Play(Montage, 1.0f);
            ++AttackAnimationIndex;
            return;
        }
    }

    switch (ActionCode)
    {
        case AnimAbility1:
            if (PlaySequenceByPath(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Ability_Q.Ability_Q"), 1.0f)) return;
            break;
        case AnimAbility2:
            if (PlaySequenceByPath(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Ability_E.Ability_E"), 1.0f)) return;
            break;
        case AnimAbility3:
            if (PlaySequenceByPath(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Ability_R.Ability_R"), 1.0f)) return;
            break;
        case AnimDodge:
            if (TryPlayCompatibleSequence({ TEXT("Dodge"), TEXT("Roll"), TEXT("Evade") }, 1.15f)) return;
            break;
        case AnimBlock:
            if (TryPlayCompatibleSequence({ TEXT("Block"), TEXT("Guard") }, 1.0f)) return;
            break;
        case AnimParry:
            if (TryPlayCompatibleSequence({ TEXT("Parry"), TEXT("Counter") }, 1.12f)) return;
            break;
        case AnimHit:
            if (PlaySequenceByPath(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/HitReact_Front.HitReact_Front"), 1.15f)) return;
            break;
        case AnimStagger:
            if (PlaySequenceByPath(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/HitReact_Back.HitReact_Back"), 0.9f)) return;
            break;
        default:
            break;
    }
}

bool ANWCharacter::PlaySequenceByPath(const TCHAR* ObjectPath, float PlayRate)
{
    UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, ObjectPath);
    if (!Sequence || !GetMesh() || !GetMesh()->GetSkeletalMeshAsset() || Sequence->GetSkeleton() != GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()) { return false; }

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (!AnimInstance) { return false; }

    UAnimMontage* DynamicMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, FName(TEXT("DefaultSlot")), 0.08f, 0.10f, PlayRate, 1);
    if (!DynamicMontage) { return false; }
    AnimInstance->Montage_Play(DynamicMontage, 1.0f);
    return true;
}

bool ANWCharacter::TryPlayCompatibleSequence(const TArray<FString>& Keywords, float PlayRate)
{
    if (!GetMesh() || !GetMesh()->GetSkeletalMeshAsset()) { return false; }

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations")));
    Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);
    for (const FAssetData& Asset : Assets)
    {
        const FString Name = Asset.AssetName.ToString();
        bool bMatches = false;
        for (const FString& Keyword : Keywords)
        {
            if (Name.Contains(Keyword, ESearchCase::IgnoreCase)) { bMatches = true; break; }
        }
        if (!bMatches) { continue; }

        UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset());
        if (!Sequence || Sequence->GetSkeleton() != GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()) { continue; }

        UAnimInstance* LocalAnimInstance = GetMesh()->GetAnimInstance();
        UAnimMontage* DynamicMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, FName(TEXT("DefaultSlot")), 0.07f, 0.10f, PlayRate, 1);
        if (LocalAnimInstance && DynamicMontage)
        {
            LocalAnimInstance->Montage_Play(DynamicMontage, 1.0f);
            return true;
        }
    }
    return false;
}

void ANWCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWCharacter, Health);
    DOREPLIFETIME(ANWCharacter, Stamina);
    DOREPLIFETIME(ANWCharacter, PrimaryWeapon);
    DOREPLIFETIME(ANWCharacter, SecondaryWeapon);
    DOREPLIFETIME(ANWCharacter, ActiveWeapon);
    DOREPLIFETIME(ANWCharacter, CombatState);
    DOREPLIFETIME(ANWCharacter, EquippedItems);
    DOREPLIFETIME(ANWCharacter, InventoryItems);
    DOREPLIFETIME_CONDITION(ANWCharacter, AbilityReadyTimes, COND_OwnerOnly);
}

void ANWCharacter::OnRep_Health() {}
void ANWCharacter::OnRep_Loadout() { LogLoadout(); }
void ANWCharacter::OnRep_Equipment() { RecalculateEquipmentStats(); }
void ANWCharacter::OnRep_Inventory()
{
    SelectedInventoryIndex = FMath::Clamp(SelectedInventoryIndex, 0, FMath::Max(0, InventoryItems.Num() - 1));
}
