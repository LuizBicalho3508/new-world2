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
    NEWORLD2_API FString ArmorWeightToString(ENWArmorWeight ArmorWeight);
    NEWORLD2_API FString ItemKindToString(ENWItemKind Kind);
    NEWORLD2_API FString ArrowElementToString(ENWArrowElement Element);
    NEWORLD2_API FString AffixToString(ENWAffixType Affix);
    NEWORLD2_API FLinearColor RarityColor(ENWItemRarity Rarity);
    NEWORLD2_API FNWGeneratedItem GenerateProceduralItem(int32 LootSeed, int32 WorldEpoch = 1);
    NEWORLD2_API FNWGeneratedItem GenerateLegendaryDungeonItem(int32 LootSeed, int32 WorldEpoch, FName DungeonTheme);
    NEWORLD2_API FNWGeneratedItem GenerateBrutalTransformationPotion(int32 LootSeed, int32 WorldEpoch);
    NEWORLD2_API float GetItemScore(const FNWGeneratedItem& Item);
}
