#include "NWCharacter.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "NWCombatLibrary.h"

namespace
{
    FNWGeneratedItem MakeStarterArmor(
        int32 Seed,
        ENWEquipmentSlot Slot,
        ENWArmorWeight Weight,
        const TCHAR* Name,
        const TCHAR* Style,
        std::initializer_list<FNWItemAffix> Affixes)
    {
        FNWGeneratedItem Item;
        Item.ItemSeed = Seed;
        Item.AppearanceSeed = Seed * 31;
        Item.ItemLevel = 1;
        Item.Name = Name;
        Item.Kind = ENWItemKind::Armor;
        Item.Slot = Slot;
        Item.ArmorWeight = Weight;
        Item.StyleId = FName(Style);
        Item.Rarity = ENWItemRarity::Common;
        for (const FNWItemAffix& Affix : Affixes) { Item.Affixes.Add(Affix); }
        return Item;
    }

    FNWGeneratedItem MakeStarterWeapon(
        int32 Seed,
        ENWWeaponType Type,
        const TCHAR* Name,
        const TCHAR* Style,
        std::initializer_list<FNWItemAffix> Affixes)
    {
        FNWGeneratedItem Item;
        Item.ItemSeed = Seed;
        Item.AppearanceSeed = Seed * 37;
        Item.ItemLevel = 1;
        Item.Name = Name;
        Item.Kind = ENWItemKind::Weapon;
        Item.WeaponType = Type;
        Item.StyleId = FName(Style);
        Item.Rarity = ENWItemRarity::Common;
        for (const FNWItemAffix& Affix : Affixes) { Item.Affixes.Add(Affix); }
        return Item;
    }

    float ScoreOrZero(const FNWGeneratedItem* Item)
    {
        return Item ? NWCombat::GetItemScore(*Item) : 0.0f;
    }

    bool IsV6Starter(const FNWGeneratedItem& Item)
    {
        return Item.ItemSeed >= 6101 && Item.ItemSeed <= 6113;
    }
}

void ANWCharacter::ConfigurePremiumV6StarterLoadout()
{
    if (!HasAuthority()) { return; }

    const bool bAlreadyConfigured = InventoryItems.ContainsByPredicate(IsV6Starter)
        || EquippedItems.ContainsByPredicate(IsV6Starter)
        || EquippedWeaponItems.ContainsByPredicate(IsV6Starter);
    if (bAlreadyConfigured) { return; }

    // O fluxo legado equipa prototipos no BeginPlay. Na V6 eles viram itens de
    // demonstracao dentro da bag: o corpo base nasce sem overlays e o jogador
    // percebe imediatamente a mudanca visual ao equipar uma peca.
    EquippedItems.RemoveAll([](const FNWGeneratedItem& Item)
    {
        return Item.ItemSeed >= 1001 && Item.ItemSeed <= 1003;
    });
    EquippedWeaponItems.RemoveAll([](const FNWGeneratedItem& Item)
    {
        return Item.ItemSeed >= 2001 && Item.ItemSeed <= 2002;
    });

    InventoryItems.RemoveAll([](const FNWGeneratedItem& Item)
    {
        return Item.ItemSeed >= 6100 && Item.ItemSeed <= 6199;
    });

    InventoryItems.Add(MakeStarterArmor(6101, ENWEquipmentSlot::Head, ENWArmorWeight::Light,
        TEXT("Capuz do Explorador"), TEXT("Ranger"),
        { { ENWAffixType::Haste, 1.5f }, { ENWAffixType::Precision, 1.0f } }));
    InventoryItems.Add(MakeStarterArmor(6102, ENWEquipmentSlot::Chest, ENWArmorWeight::Medium,
        TEXT("Gibao do Mercenario"), TEXT("Mercenary"),
        { { ENWAffixType::Armor, 2.0f }, { ENWAffixType::Power, 1.5f } }));
    InventoryItems.Add(MakeStarterArmor(6103, ENWEquipmentSlot::Gloves, ENWArmorWeight::Medium,
        TEXT("Luvas do Duelista"), TEXT("Duelist"),
        { { ENWAffixType::Precision, 1.6f }, { ENWAffixType::Bleed, 1.1f } }));
    InventoryItems.Add(MakeStarterArmor(6104, ENWEquipmentSlot::Legs, ENWArmorWeight::Light,
        TEXT("Calcas do Batedor"), TEXT("Pathfinder"),
        { { ENWAffixType::Haste, 1.3f }, { ENWAffixType::DodgeEmpower, 1.2f } }));
    InventoryItems.Add(MakeStarterArmor(6105, ENWEquipmentSlot::Boots, ENWArmorWeight::Light,
        TEXT("Botas do Caminhante"), TEXT("Wanderer"),
        { { ENWAffixType::Haste, 1.4f }, { ENWAffixType::DodgeEmpower, 1.0f } }));

    InventoryItems.Add(MakeStarterWeapon(6111, ENWWeaponType::Greatsword,
        TEXT("Espada Grande de Ferro"), TEXT("BlackIron"),
        { { ENWAffixType::Power, 2.0f }, { ENWAffixType::Executioner, 1.2f } }));
    InventoryItems.Add(MakeStarterWeapon(6112, ENWWeaponType::Staff,
        TEXT("Cajado Elemental de Aprendiz"), TEXT("Stormcaller"),
        { { ENWAffixType::Haste, 1.7f }, { ENWAffixType::AbilityEcho, 1.2f } }));
    InventoryItems.Add(MakeStarterWeapon(6113, ENWWeaponType::Daggers,
        TEXT("Adagas Presa Noturna"), TEXT("Nightfang"),
        { { ENWAffixType::Precision, 1.8f }, { ENWAffixType::PoisonCoating, 1.4f } }));

    SortInventory();
    RecalculateEquipmentStats();
    Health = MaxHealth;
    Stamina = MaxStamina;
    SelectedInventoryIndex = 0;
    ForceNetUpdate();

    UE_LOG(LogTemp, Warning, TEXT("[STARTER-V6] personagem base sem overlays | 5 armaduras + 3 armas iniciais na bag para equipar."));
}

bool ANWCharacter::HasEquippedWeaponItem(ENWWeaponType WeaponType) const
{
    return EquippedWeaponItems.ContainsByPredicate([WeaponType](const FNWGeneratedItem& Item)
    {
        return Item.Kind == ENWItemKind::Weapon && !Item.Name.IsEmpty() && Item.WeaponType == WeaponType;
    });
}

float ANWCharacter::GetInventoryComparisonDelta(int32 InventoryIndex) const
{
    if (!InventoryItems.IsValidIndex(InventoryIndex)) { return 0.0f; }
    const FNWGeneratedItem& Candidate = InventoryItems[InventoryIndex];
    if (Candidate.Kind == ENWItemKind::Consumable) { return 0.0f; }

    const FNWGeneratedItem* Equipped = nullptr;
    if (Candidate.Kind == ENWItemKind::Armor)
    {
        Equipped = EquippedItems.FindByPredicate([&Candidate](const FNWGeneratedItem& Item)
        {
            return Item.Kind == ENWItemKind::Armor && Item.Slot == Candidate.Slot;
        });
    }
    else if (Candidate.Kind == ENWItemKind::Weapon)
    {
        Equipped = EquippedWeaponItems.FindByPredicate([&Candidate](const FNWGeneratedItem& Item)
        {
            return Item.Kind == ENWItemKind::Weapon && Item.WeaponType == Candidate.WeaponType;
        });

        if (!Equipped && !EquippedWeaponItems.IsEmpty())
        {
            const int32 ActiveSlot = ActiveWeapon == SecondaryWeapon ? 1 : 0;
            if (EquippedWeaponItems.IsValidIndex(ActiveSlot) && !EquippedWeaponItems[ActiveSlot].Name.IsEmpty())
            {
                Equipped = &EquippedWeaponItems[ActiveSlot];
            }
        }
    }

    return NWCombat::GetItemScore(Candidate) - ScoreOrZero(Equipped);
}

FString ANWCharacter::GetInventoryComparisonLabel(int32 InventoryIndex) const
{
    if (!InventoryItems.IsValidIndex(InventoryIndex)) { return TEXT(""); }
    const FNWGeneratedItem& Candidate = InventoryItems[InventoryIndex];
    if (Candidate.Kind == ENWItemKind::Consumable) { return TEXT("USAR"); }

    bool bHasComparableEquipped = false;
    if (Candidate.Kind == ENWItemKind::Armor)
    {
        bHasComparableEquipped = EquippedItems.ContainsByPredicate([&Candidate](const FNWGeneratedItem& Item)
        {
            return Item.Kind == ENWItemKind::Armor && Item.Slot == Candidate.Slot;
        });
    }
    else
    {
        bHasComparableEquipped = !EquippedWeaponItems.IsEmpty();
    }

    const float Delta = GetInventoryComparisonDelta(InventoryIndex);
    if (!bHasComparableEquipped) { return FString::Printf(TEXT("MELHOR  +%.1f  | SLOT VAZIO"), FMath::Max(0.0f, Delta)); }
    if (Delta > 0.25f) { return FString::Printf(TEXT("MELHOR  +%.1f"), Delta); }
    if (Delta < -0.25f) { return FString::Printf(TEXT("PIOR  %.1f"), Delta); }
    return TEXT("IGUAL");
}

void ANWCharacter::SetSelectedInventoryIndexSafe(int32 NewIndex)
{
    SelectedInventoryIndex = InventoryItems.IsEmpty() ? 0 : FMath::Clamp(NewIndex, 0, InventoryItems.Num() - 1);
}

void ANWCharacter::EquipInventoryItemAtIndex(int32 InventoryIndex)
{
    if (!InventoryItems.IsValidIndex(InventoryIndex)) { return; }
    SetSelectedInventoryIndexSafe(InventoryIndex);
    const int32 Seed = InventoryItems[InventoryIndex].ItemSeed;
    if (HasAuthority()) { EquipInventoryItemBySeed(Seed); }
    else { ServerEquipInventoryItem(Seed); }
}

void ANWCharacter::PremiumV6StabilizeLocomotion()
{
    if (!IsLocallyControlled() || CombatState == ENWCombatState::Staggered) { return; }

    APlayerController* PC = Cast<APlayerController>(GetController());
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!PC || !Movement || PC->IsMoveInputIgnored()) { return; }

    if (USkeletalMeshComponent* Mesh = GetMesh())
    {
        if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
        {
            AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
        }
    }

    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    Movement->MaxAcceleration = 2450.0f;
    Movement->BrakingDecelerationWalking = 1850.0f;
    Movement->GroundFriction = 7.2f;

    const float ForwardAxis = (PC->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f);
    const float RightAxis = (PC->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);
    if (FMath::IsNearlyZero(ForwardAxis) && FMath::IsNearlyZero(RightAxis)) { return; }

    const FRotator YawRotation(0.0f, PC->GetControlRotation().Yaw, 0.0f);
    FVector Expected = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X) * ForwardAxis
        + FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y) * RightAxis;
    Expected = Expected.GetSafeNormal2D();

    const FVector Pending = GetPendingMovementInputVector().GetSafeNormal2D();
    const bool bAxisPipelineMissed = Pending.IsNearlyZero() || FVector::DotProduct(Pending, Expected) < 0.72f;
    if (bAxisPipelineMissed)
    {
        AddMovementInput(Expected, 1.0f, true);
    }
}
