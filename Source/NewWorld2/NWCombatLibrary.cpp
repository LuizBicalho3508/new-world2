#include "NWCombatLibrary.h"

namespace
{
    FNWWeaponAbilityDefinition Ability(const TCHAR* Name, ENWAbilityKind Kind, float Power, float Range, float Radius, float Cooldown = 3.0f)
    {
        FNWWeaponAbilityDefinition Result;
        Result.Name = Name;
        Result.Kind = Kind;
        Result.Power = Power;
        Result.Range = Range;
        Result.Radius = Radius;
        Result.Cooldown = Cooldown;
        return Result;
    }
}

FNWWeaponDefinition NWCombat::GetWeaponDefinition(ENWWeaponType WeaponType)
{
    FNWWeaponDefinition Def;
    Def.Type = WeaponType;

    switch (WeaponType)
    {
        case ENWWeaponType::Staff:
            Def.Name = TEXT("Cajado Arcano"); Def.BasicDamage = 19.0f; Def.BasicRange = 1200.0f; Def.BasicRadius = 55.0f;
            Def.Abilities = { Ability(TEXT("Lanca Arcana"), ENWAbilityKind::DirectDamage, 34.0f, 1450.0f, 70.0f), Ability(TEXT("Explosao Elemental"), ENWAbilityKind::AreaDamage, 28.0f, 650.0f, 360.0f), Ability(TEXT("Fluxo Vital"), ENWAbilityKind::Heal, 28.0f, 0.0f, 0.0f) };
            break;
        case ENWWeaponType::Greatsword:
            Def.Name = TEXT("Espada Grande"); Def.BasicDamage = 32.0f; Def.BasicRange = 300.0f; Def.BasicRadius = 95.0f;
            Def.Abilities = { Ability(TEXT("Corte Demolidor"), ENWAbilityKind::DirectDamage, 44.0f, 360.0f, 110.0f), Ability(TEXT("Ruptura de Terra"), ENWAbilityKind::AreaDamage, 36.0f, 280.0f, 420.0f), Ability(TEXT("Renovacao de Batalha"), ENWAbilityKind::Heal, 25.0f, 0.0f, 0.0f) };
            break;
        case ENWWeaponType::DualSwords:
            Def.Name = TEXT("Duas Espadas"); Def.BasicDamage = 23.0f; Def.BasicRange = 280.0f; Def.BasicRadius = 80.0f;
            Def.Abilities = { Ability(TEXT("Corte Cruzado"), ENWAbilityKind::DirectDamage, 36.0f, 330.0f, 85.0f), Ability(TEXT("Turbilhao de Aco"), ENWAbilityKind::AreaDamage, 30.0f, 240.0f, 390.0f), Ability(TEXT("Segundo Folego"), ENWAbilityKind::Heal, 22.0f, 0.0f, 0.0f) };
            break;
        case ENWWeaponType::SwordShield:
            Def.Name = TEXT("Espada e Escudo"); Def.BasicDamage = 24.0f; Def.BasicRange = 270.0f; Def.BasicRadius = 80.0f;
            Def.Abilities = { Ability(TEXT("Impacto de Escudo"), ENWAbilityKind::DirectDamage, 31.0f, 300.0f, 95.0f), Ability(TEXT("Varredura do Guardiao"), ENWAbilityKind::AreaDamage, 27.0f, 220.0f, 350.0f), Ability(TEXT("Graca do Defensor"), ENWAbilityKind::Heal, 31.0f, 0.0f, 0.0f) };
            break;
        case ENWWeaponType::Daggers:
            Def.Name = TEXT("Adagas"); Def.BasicDamage = 21.0f; Def.BasicRange = 235.0f; Def.BasicRadius = 65.0f;
            Def.Abilities = { Ability(TEXT("Presa Sombria"), ENWAbilityKind::DirectDamage, 39.0f, 285.0f, 70.0f), Ability(TEXT("Leque de Laminas"), ENWAbilityKind::AreaDamage, 29.0f, 230.0f, 330.0f), Ability(TEXT("Sutura de Sangue"), ENWAbilityKind::Heal, 21.0f, 0.0f, 0.0f) };
            break;
        case ENWWeaponType::Bow:
            Def.Name = TEXT("Arco"); Def.BasicDamage = 25.0f; Def.BasicRange = 1550.0f; Def.BasicRadius = 45.0f;
            Def.Abilities = { Ability(TEXT("Flecha Perfurante"), ENWAbilityKind::DirectDamage, 40.0f, 1850.0f, 52.0f), Ability(TEXT("Chuva de Flechas"), ENWAbilityKind::AreaDamage, 31.0f, 1000.0f, 430.0f), Ability(TEXT("Curativo de Campo"), ENWAbilityKind::Heal, 24.0f, 0.0f, 0.0f) };
            break;
        case ENWWeaponType::Firearm:
            Def.Name = TEXT("Arma de Fogo"); Def.BasicDamage = 29.0f; Def.BasicRange = 1750.0f; Def.BasicRadius = 35.0f;
            Def.Abilities = { Ability(TEXT("Tiro de Impacto"), ENWAbilityKind::DirectDamage, 43.0f, 2050.0f, 42.0f), Ability(TEXT("Rajada de Chumbo"), ENWAbilityKind::AreaDamage, 33.0f, 750.0f, 300.0f), Ability(TEXT("Tonico de Adrenalina"), ENWAbilityKind::Heal, 23.0f, 0.0f, 0.0f) };
            break;
        default:
            break;
    }

    return Def;
}

FString NWCombat::WeaponTypeToString(ENWWeaponType WeaponType) { return GetWeaponDefinition(WeaponType).Name; }

bool NWCombat::IsPoisonCompatible(ENWWeaponType WeaponType)
{
    return WeaponType == ENWWeaponType::Greatsword || WeaponType == ENWWeaponType::DualSwords || WeaponType == ENWWeaponType::SwordShield || WeaponType == ENWWeaponType::Daggers || WeaponType == ENWWeaponType::Bow;
}

ENWWeaponType NWCombat::GetNextWeaponType(ENWWeaponType WeaponType)
{
    const uint8 Next = (static_cast<uint8>(WeaponType) + 1u) % 7u;
    return static_cast<ENWWeaponType>(Next);
}

FString NWCombat::RarityToString(ENWItemRarity Rarity)
{
    switch (Rarity) { case ENWItemRarity::Common: return TEXT("Comum"); case ENWItemRarity::Uncommon: return TEXT("Incomum"); case ENWItemRarity::Rare: return TEXT("Raro"); case ENWItemRarity::Epic: return TEXT("Epico"); case ENWItemRarity::Legendary: return TEXT("Lendario"); default: return TEXT("Desconhecido"); }
}

FString NWCombat::EquipmentSlotToString(ENWEquipmentSlot Slot)
{
    switch (Slot) { case ENWEquipmentSlot::Head: return TEXT("Cabeca"); case ENWEquipmentSlot::Chest: return TEXT("Peitoral"); case ENWEquipmentSlot::Gloves: return TEXT("Luvas"); case ENWEquipmentSlot::Legs: return TEXT("Pernas"); case ENWEquipmentSlot::Boots: return TEXT("Botas"); default: return TEXT("Item"); }
}

FString NWCombat::AffixToString(ENWAffixType Affix)
{
    switch (Affix) { case ENWAffixType::Power: return TEXT("Poder"); case ENWAffixType::Vitality: return TEXT("Vitalidade"); case ENWAffixType::Precision: return TEXT("Precisao"); case ENWAffixType::Haste: return TEXT("Aceleracao"); case ENWAffixType::Healing: return TEXT("Cura"); case ENWAffixType::PoisonCoating: return TEXT("Revestimento Venenoso"); case ENWAffixType::LifeSteal: return TEXT("Roubo de Vida"); default: return TEXT("Atributo"); }
}
