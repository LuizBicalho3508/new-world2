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
#include "NWCombatLibrary.h"
#include "NWProceduralWorldManager.h"
#include "TimerManager.h"
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
        EnsureStarterEquipment();
        RecalculateEquipmentStats();
        Health = MaxHealth;
    }

    LogLoadout();
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
    PlayerInputComponent->BindAction(TEXT("Ability2"), IE_Pressed, this, &ANWCharacter::Ability2);
    PlayerInputComponent->BindAction(TEXT("Ability3"), IE_Pressed, this, &ANWCharacter::Ability3);
    PlayerInputComponent->BindAction(TEXT("PrimaryWeapon"), IE_Pressed, this, &ANWCharacter::SelectPrimaryWeapon);
    PlayerInputComponent->BindAction(TEXT("SecondaryWeapon"), IE_Pressed, this, &ANWCharacter::SelectSecondaryWeapon);
    PlayerInputComponent->BindAction(TEXT("QuickSwap"), IE_Pressed, this, &ANWCharacter::QuickSwapWeapon);
    PlayerInputComponent->BindAction(TEXT("CyclePrimaryWeapon"), IE_Pressed, this, &ANWCharacter::CyclePrimaryWeapon);
    PlayerInputComponent->BindAction(TEXT("CycleSecondaryWeapon"), IE_Pressed, this, &ANWCharacter::CycleSecondaryWeapon);
    PlayerInputComponent->BindAction(TEXT("RegenerateWorld"), IE_Pressed, this, &ANWCharacter::RegenerateWorld);
}

void ANWCharacter::MoveForward(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
    }
}

void ANWCharacter::MoveRight(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
    }
}

void ANWCharacter::Turn(float Value) { AddControllerYawInput(Value); }
void ANWCharacter::LookUp(float Value) { AddControllerPitchInput(Value); }

void ANWCharacter::StartSprint()
{
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
    if (HasAuthority()) { ExecuteAttack(); }
    else { ServerAttack(); }
}

void ANWCharacter::Ability1() { if (HasAuthority()) { ExecuteAbility(0); } else { ServerUseAbility(0); } }
void ANWCharacter::Ability2() { if (HasAuthority()) { ExecuteAbility(1); } else { ServerUseAbility(1); } }
void ANWCharacter::Ability3() { if (HasAuthority()) { ExecuteAbility(2); } else { ServerUseAbility(2); } }

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

void ANWCharacter::ServerAttack_Implementation() { ExecuteAttack(); }
void ANWCharacter::ServerUseAbility_Implementation(uint8 AbilityIndex) { ExecuteAbility(AbilityIndex); }
void ANWCharacter::ServerSetSprinting_Implementation(bool bSprinting) { ApplySprintState(bSprinting); }

void ANWCharacter::ServerSetActiveWeapon_Implementation(ENWWeaponType RequestedWeapon)
{
    if (RequestedWeapon == PrimaryWeapon || RequestedWeapon == SecondaryWeapon)
    {
        SetActiveWeaponInternal(RequestedWeapon);
    }
}

void ANWCharacter::ServerCycleLoadout_Implementation(bool bPrimarySlot)
{
    ENWWeaponType& Slot = bPrimarySlot ? PrimaryWeapon : SecondaryWeapon;
    const ENWWeaponType Other = bPrimarySlot ? SecondaryWeapon : PrimaryWeapon;
    const ENWWeaponType Previous = Slot;

    Slot = NWCombat::GetNextWeaponType(Slot);
    if (Slot == Other)
    {
        Slot = NWCombat::GetNextWeaponType(Slot);
    }

    if (ActiveWeapon == Previous)
    {
        ActiveWeapon = Slot;
    }

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

void ANWCharacter::ExecuteAttack()
{
    if (!GetWorld()) { return; }

    const FNWWeaponDefinition Weapon = NWCombat::GetWeaponDefinition(ActiveWeapon);
    FVector ViewLocation;
    FRotator ViewRotation;
    GetActorEyesViewPoint(ViewLocation, ViewRotation);

    const FVector Start = ViewLocation;
    const FVector End = Start + ViewRotation.Vector() * Weapon.BasicRange;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NWAttack), false, this);
    TArray<FHitResult> Hits;

    GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Weapon.BasicRadius), QueryParams);

    TSet<AActor*> DamagedActors;
    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (!Target || Target == this || DamagedActors.Contains(Target)) { continue; }
        DamagedActors.Add(Target);
        ApplyWeaponDamage(Target, Weapon.BasicDamage, true);

        if (ActiveWeapon == ENWWeaponType::Staff || ActiveWeapon == ENWWeaponType::Bow || ActiveWeapon == ENWWeaponType::Firearm)
        {
            break;
        }
    }
}

void ANWCharacter::ExecuteAbility(uint8 AbilityIndex)
{
    if (!GetWorld() || AbilityIndex >= 3) { return; }

    const FNWWeaponDefinition Weapon = NWCombat::GetWeaponDefinition(ActiveWeapon);
    if (!Weapon.Abilities.IsValidIndex(AbilityIndex)) { return; }

    const FNWWeaponAbilityDefinition& Ability = Weapon.Abilities[AbilityIndex];
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < AbilityReadyTimes[AbilityIndex]) { return; }

    const float EffectiveCooldown = FMath::Max(0.8f, Ability.Cooldown * CooldownMultiplier);
    AbilityReadyTimes[AbilityIndex] = Now + EffectiveCooldown;

    float ComboMultiplier = 1.0f;
    if ((Now - LastAbilityTime) <= ComboWindowSeconds && LastAbilityWeapon != ActiveWeapon)
    {
        ComboMultiplier = CrossWeaponComboMultiplier;
        UE_LOG(LogTemp, Display, TEXT("[COMBO] %s -> %s | x%.2f"), *NWCombat::WeaponTypeToString(LastAbilityWeapon), *Weapon.Name, ComboMultiplier);
    }

    LastAbilityTime = Now;
    LastAbilityWeapon = ActiveWeapon;

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
            ApplyWeaponDamage(Target, Ability.Power * ComboMultiplier, true);
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
        ApplyWeaponDamage(Target, Ability.Power * ComboMultiplier, true);
    }
}

void ANWCharacter::ApplyWeaponDamage(AActor* Target, float BaseDamage, bool bAllowPoison)
{
    if (!HasAuthority() || !Target) { return; }

    const float Damage = RollDamageWithStats(BaseDamage);
    UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, UDamageType::StaticClass());

    if (LifeStealPercent > 0.0f)
    {
        Health = FMath::Clamp(Health + Damage * LifeStealPercent, 0.0f, MaxHealth);
    }

    if (bAllowPoison && HasPoisonCoating() && NWCombat::IsPoisonCompatible(ActiveWeapon))
    {
        ApplyPoison(Target, PoisonDamagePerTick);
    }
}

void ANWCharacter::ApplyPoison(AActor* Target, float DamagePerTick)
{
    if (!GetWorld() || !Target || DamagePerTick <= 0.0f) { return; }

    TWeakObjectPtr<AActor> WeakTarget(Target);
    TWeakObjectPtr<ANWCharacter> WeakSelf(this);
    TSharedRef<int32> RemainingTicks = MakeShared<int32>(3);
    TSharedRef<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();

    GetWorldTimerManager().SetTimer(*TimerHandle, FTimerDelegate::CreateLambda([WeakTarget, WeakSelf, RemainingTicks, TimerHandle]()
    {
        if (!WeakSelf.IsValid() || !WeakTarget.IsValid() || *RemainingTicks <= 0)
        {
            if (WeakSelf.IsValid()) { WeakSelf->GetWorldTimerManager().ClearTimer(*TimerHandle); }
            return;
        }

        UGameplayStatics::ApplyDamage(WeakTarget.Get(), WeakSelf->PoisonDamagePerTick, WeakSelf->GetController(), WeakSelf.Get(), UDamageType::StaticClass());
        --(*RemainingTicks);
        if (*RemainingTicks <= 0)
        {
            WeakSelf->GetWorldTimerManager().ClearTimer(*TimerHandle);
        }
    }), 1.0f, true, 1.0f);
}

void ANWCharacter::SetActiveWeaponInternal(ENWWeaponType RequestedWeapon)
{
    if (RequestedWeapon != PrimaryWeapon && RequestedWeapon != SecondaryWeapon) { return; }
    ActiveWeapon = RequestedWeapon;
    LogLoadout();
}

void ANWCharacter::ApplySprintState(bool bSprinting)
{
    GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
}

float ANWCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (!HasAuthority() || AppliedDamage <= 0.0f) { return AppliedDamage; }

    Health = FMath::Clamp(Health - AppliedDamage, 0.0f, MaxHealth);
    if (Health <= 0.0f)
    {
        Health = MaxHealth;
        SetActorLocation(FVector(0.0f, 0.0f, 1500.0f), false, nullptr, ETeleportType::TeleportPhysics);
    }
    return AppliedDamage;
}

void ANWCharacter::ReceiveProceduralLoot(int32 LootSeed)
{
    if (!HasAuthority()) { return; }
    EquipIfUpgrade(GenerateProceduralItem(LootSeed));
}

FNWGeneratedItem ANWCharacter::GenerateProceduralItem(int32 LootSeed) const
{
    FRandomStream Random(LootSeed);
    FNWGeneratedItem Item;
    Item.ItemSeed = LootSeed;
    Item.Slot = static_cast<ENWEquipmentSlot>(Random.RandRange(0, 4));

    const int32 RarityRoll = Random.RandRange(0, 99);
    if (RarityRoll < 50) Item.Rarity = ENWItemRarity::Common;
    else if (RarityRoll < 76) Item.Rarity = ENWItemRarity::Uncommon;
    else if (RarityRoll < 91) Item.Rarity = ENWItemRarity::Rare;
    else if (RarityRoll < 98) Item.Rarity = ENWItemRarity::Epic;
    else Item.Rarity = ENWItemRarity::Legendary;

    const int32 RarityLevel = static_cast<int32>(Item.Rarity) + 1;
    const int32 AffixCount = FMath::Clamp(1 + RarityLevel / 2, 1, 3);
    TSet<ENWAffixType> UsedAffixes;

    if (Item.Slot == ENWEquipmentSlot::Gloves && Random.FRand() < 0.32f)
    {
        FNWItemAffix Poison;
        Poison.Type = ENWAffixType::PoisonCoating;
        Poison.Magnitude = 2.0f + RarityLevel * 1.25f;
        Item.Affixes.Add(Poison);
        UsedAffixes.Add(Poison.Type);
    }

    while (Item.Affixes.Num() < AffixCount)
    {
        ENWAffixType Type = static_cast<ENWAffixType>(Random.RandRange(0, 6));
        if (Type == ENWAffixType::PoisonCoating && Item.Slot != ENWEquipmentSlot::Gloves) { Type = ENWAffixType::Power; }
        if (UsedAffixes.Contains(Type)) { continue; }

        FNWItemAffix Affix;
        Affix.Type = Type;
        Affix.Magnitude = Random.FRandRange(1.5f, 3.5f) * RarityLevel;
        Item.Affixes.Add(Affix);
        UsedAffixes.Add(Type);
    }

    Item.Name = FString::Printf(TEXT("%s %s [%d]"), *NWCombat::RarityToString(Item.Rarity), *NWCombat::EquipmentSlotToString(Item.Slot), FMath::Abs(LootSeed % 10000));
    return Item;
}

void ANWCharacter::EquipIfUpgrade(const FNWGeneratedItem& Item)
{
    int32 ExistingIndex = INDEX_NONE;
    for (int32 Index = 0; Index < EquippedItems.Num(); ++Index)
    {
        if (EquippedItems[Index].Slot == Item.Slot) { ExistingIndex = Index; break; }
    }

    const bool bUpgrade = ExistingIndex == INDEX_NONE || GetItemScore(Item) > GetItemScore(EquippedItems[ExistingIndex]);
    UE_LOG(LogTemp, Display, TEXT("[LOOT] Drop %s | score %.1f | %s"), *Item.Name, GetItemScore(Item), bUpgrade ? TEXT("EQUIPADO") : TEXT("mantido no chao/inventario futuro"));

    if (!bUpgrade) { return; }
    if (ExistingIndex == INDEX_NONE) { EquippedItems.Add(Item); }
    else { EquippedItems[ExistingIndex] = Item; }
    RecalculateEquipmentStats();
}

void ANWCharacter::EnsureStarterEquipment()
{
    if (!EquippedItems.IsEmpty()) { return; }

    FNWGeneratedItem Gloves;
    Gloves.ItemSeed = 1001;
    Gloves.Name = TEXT("Luvas do Alquimista - Prototipo");
    Gloves.Slot = ENWEquipmentSlot::Gloves;
    Gloves.Rarity = ENWItemRarity::Rare;
    Gloves.Affixes = { { ENWAffixType::PoisonCoating, 5.0f }, { ENWAffixType::Vitality, 4.0f } };
    EquippedItems.Add(Gloves);

    FNWGeneratedItem Chest;
    Chest.ItemSeed = 1002;
    Chest.Name = TEXT("Peitoral do Vanguardista - Prototipo");
    Chest.Slot = ENWEquipmentSlot::Chest;
    Chest.Rarity = ENWItemRarity::Uncommon;
    Chest.Affixes = { { ENWAffixType::Power, 4.0f }, { ENWAffixType::Haste, 3.0f } };
    EquippedItems.Add(Chest);
}

void ANWCharacter::RecalculateEquipmentStats()
{
    const float Vitality = GetAffixTotal(ENWAffixType::Vitality);
    const float Power = GetAffixTotal(ENWAffixType::Power);
    const float Healing = GetAffixTotal(ENWAffixType::Healing);
    const float Haste = GetAffixTotal(ENWAffixType::Haste);
    const float Precision = GetAffixTotal(ENWAffixType::Precision);
    const float LifeSteal = GetAffixTotal(ENWAffixType::LifeSteal);
    const float Poison = GetAffixTotal(ENWAffixType::PoisonCoating);

    MaxHealth = BaseMaxHealth + Vitality * 2.5f;
    DamageMultiplier = 1.0f + Power * 0.018f;
    HealingMultiplier = 1.0f + Healing * 0.025f;
    CooldownMultiplier = FMath::Clamp(1.0f - Haste * 0.012f, 0.55f, 1.0f);
    PrecisionChance = FMath::Clamp(Precision * 0.012f, 0.0f, 0.35f);
    LifeStealPercent = FMath::Clamp(LifeSteal * 0.006f, 0.0f, 0.20f);
    PoisonDamagePerTick = Poison;
    Health = FMath::Clamp(Health, 0.0f, MaxHealth);
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

float ANWCharacter::GetItemScore(const FNWGeneratedItem& Item) const
{
    float Score = (static_cast<int32>(Item.Rarity) + 1) * 10.0f;
    for (const FNWItemAffix& Affix : Item.Affixes)
    {
        Score += Affix.Magnitude;
        if (Affix.Type == ENWAffixType::PoisonCoating) { Score += 18.0f; }
    }
    return Score;
}

bool ANWCharacter::HasPoisonCoating() const
{
    return PoisonDamagePerTick > 0.0f;
}

float ANWCharacter::RollDamageWithStats(float BaseDamage) const
{
    float Result = BaseDamage * DamageMultiplier;
    if (FMath::FRand() < PrecisionChance) { Result *= 1.5f; }
    return Result;
}

FString ANWCharacter::GetActiveWeaponName() const
{
    return NWCombat::WeaponTypeToString(ActiveWeapon);
}

void ANWCharacter::LogLoadout() const
{
    UE_LOG(LogTemp, Display, TEXT("[LOADOUT] Slot 1=%s | Slot 2=%s | Ativa=%s"), *NWCombat::WeaponTypeToString(PrimaryWeapon), *NWCombat::WeaponTypeToString(SecondaryWeapon), *NWCombat::WeaponTypeToString(ActiveWeapon));
}

void ANWCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWCharacter, Health);
    DOREPLIFETIME(ANWCharacter, PrimaryWeapon);
    DOREPLIFETIME(ANWCharacter, SecondaryWeapon);
    DOREPLIFETIME(ANWCharacter, ActiveWeapon);
    DOREPLIFETIME(ANWCharacter, EquippedItems);
}

void ANWCharacter::OnRep_Health() {}
void ANWCharacter::OnRep_Loadout() { LogLoadout(); }
void ANWCharacter::OnRep_Equipment() { RecalculateEquipmentStats(); }
