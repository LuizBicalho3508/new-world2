#pragma once

#include "CoreMinimal.h"
#include "NWCombatTypes.h"

namespace NWCombat
{
    NEWORLD2_API FNWWeaponDefinition GetWeaponDefinition(ENWWeaponType WeaponType);
    NEWORLD2_API FString WeaponTypeToString(ENWWeaponType WeaponType);
    NEWORLD2_API bool IsPoisonCompatible(ENWWeaponType WeaponType);
    NEWORLD2_API ENWWeaponType GetNextWeaponType(ENWWeaponType WeaponType);
    NEWORLD2_API FString RarityToString(ENWItemRarity Rarity);
    NEWORLD2_API FString EquipmentSlotToString(ENWEquipmentSlot Slot);
    NEWORLD2_API FString AffixToString(ENWAffixType Affix);
}
