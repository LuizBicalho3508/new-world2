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

    FNWWeaponPassiveDefinition Passive(const TCHAR* Name, const TCHAR* Description)
    {
        FNWWeaponPassiveDefinition Result;
        Result.Name = Name;
        Result.Description = Description;
        return Result;
    }

    bool IsArmorSlotCompatible(ENWEquipmentSlot Slot, ENWAffixType Type)
    {
        switch (Type)
        {
            case ENWAffixType::PoisonCoating:
            case ENWAffixType::Firebrand:
            case ENWAffixType::Frostbite:
            case ENWAffixType::ShockChain:
            case ENWAffixType::Bleed:
                return Slot == ENWEquipmentSlot::Gloves;
            case ENWAffixType::FortifiedGuard:
            case ENWAffixType::Armor:
                return Slot == ENWEquipmentSlot::Chest || Slot == ENWEquipmentSlot::Head;
            case ENWAffixType::ParryHeal:
                return Slot == ENWEquipmentSlot::Gloves || Slot == ENWEquipmentSlot::Chest;
            case ENWAffixType::DodgeEmpower:
                return Slot == ENWEquipmentSlot::Boots || Slot == ENWEquipmentSlot::Legs;
            default:
                return true;
        }
    }

    bool IsItemAffixCompatible(const FNWGeneratedItem& Item, ENWAffixType Type)
    {
        if (Item.Kind == ENWItemKind::Consumable) { return false; }
        if (Item.Kind == ENWItemKind::Armor) { return IsArmorSlotCompatible(Item.Slot, Type); }

        switch (Type)
        {
            case ENWAffixType::Armor:
            case ENWAffixType::Vitality:
            case ENWAffixType::Healing:
            case ENWAffixType::FortifiedGuard:
            case ENWAffixType::ParryHeal:
            case ENWAffixType::DodgeEmpower:
                return false;
            default:
                return true;
        }
    }

    void AddAffixIfMissing(FNWGeneratedItem& Item, ENWAffixType Type, float Magnitude)
    {
        if (!IsItemAffixCompatible(Item, Type)) { return; }
        for (const FNWItemAffix& Existing : Item.Affixes)
        {
            if (Existing.Type == Type) { return; }
        }
        FNWItemAffix Affix;
        Affix.Type = Type;
        Affix.Magnitude = Magnitude;
        Item.Affixes.Add(Affix);
    }

    ENWArmorWeight RollArmorWeight(FRandomStream& Random)
    {
        const int32 Roll = Random.RandRange(0, 99);
        if (Roll < 33) { return ENWArmorWeight::Light; }
        if (Roll < 67) { return ENWArmorWeight::Medium; }
        return ENWArmorWeight::Heavy;
    }

    FName PickArmorStyle(ENWArmorWeight Weight, FRandomStream& Random)
    {
        static const FName LightStyles[] = {
            TEXT("Arcanist"), TEXT("Shadowweave"), TEXT("Ranger"), TEXT("Duelist"), TEXT("Moonveil"), TEXT("Wanderer")
        };
        static const FName MediumStyles[] = {
            TEXT("Warden"), TEXT("Mercenary"), TEXT("Hunter"), TEXT("Battlemage"), TEXT("Corsair"), TEXT("Pathfinder")
        };
        static const FName HeavyStyles[] = {
            TEXT("DreadKnight"), TEXT("RoyalGuard"), TEXT("IronVanguard"), TEXT("Dragonplate"), TEXT("Crusader"), TEXT("Obsidian")
        };

        switch (Weight)
        {
            case ENWArmorWeight::Light: return LightStyles[Random.RandRange(0, UE_ARRAY_COUNT(LightStyles) - 1)];
            case ENWArmorWeight::Heavy: return HeavyStyles[Random.RandRange(0, UE_ARRAY_COUNT(HeavyStyles) - 1)];
            default: return MediumStyles[Random.RandRange(0, UE_ARRAY_COUNT(MediumStyles) - 1)];
        }
    }

    FName PickWeaponStyle(ENWWeaponType Weapon, FRandomStream& Random)
    {
        static const FName Staff[] = { TEXT("Astral"), TEXT("Runic"), TEXT("Moonspire"), TEXT("Infernal"), TEXT("Stormcaller"), TEXT("Verdant") };
        static const FName Greatsword[] = { TEXT("Dragonfang"), TEXT("BlackIron"), TEXT("RoyalExecutioner"), TEXT("FrostEdge"), TEXT("Sunforged"), TEXT("Dreadblade") };
        static const FName Dual[] = { TEXT("TwinFang"), TEXT("BloodDance"), TEXT("SilverDuelist"), TEXT("StormTwin"), TEXT("VenomPair"), TEXT("AshenPair") };
        static const FName SwordShield[] = { TEXT("LionGuard"), TEXT("DarkTemplar"), TEXT("RunicAegis"), TEXT("Sunward"), TEXT("IronOath"), TEXT("Gravewarden") };
        static const FName Daggers[] = { TEXT("Nightfang"), TEXT("VenomClaw"), TEXT("Bloodletter"), TEXT("Shadowglass"), TEXT("StormNeedle"), TEXT("Moonknife") };
        static const FName Bow[] = { TEXT("Ethereal"), TEXT("Emberwood"), TEXT("Venomvine"), TEXT("Stormbranch"), TEXT("Frosthorn"), TEXT("Sunhunter") };
        static const FName Firearm[] = { TEXT("BlackPowder"), TEXT("Runelock"), TEXT("AshCannon"), TEXT("Stormshot"), TEXT("SilverHunter"), TEXT("DreadMusket") };

        const FName* Set = Greatsword;
        int32 Count = UE_ARRAY_COUNT(Greatsword);
        switch (Weapon)
        {
            case ENWWeaponType::Staff: Set = Staff; Count = UE_ARRAY_COUNT(Staff); break;
            case ENWWeaponType::DualSwords: Set = Dual; Count = UE_ARRAY_COUNT(Dual); break;
            case ENWWeaponType::SwordShield: Set = SwordShield; Count = UE_ARRAY_COUNT(SwordShield); break;
            case ENWWeaponType::Daggers: Set = Daggers; Count = UE_ARRAY_COUNT(Daggers); break;
            case ENWWeaponType::Bow: Set = Bow; Count = UE_ARRAY_COUNT(Bow); break;
            case ENWWeaponType::Firearm: Set = Firearm; Count = UE_ARRAY_COUNT(Firearm); break;
            default: break;
        }
        return Set[Random.RandRange(0, Count - 1)];
    }

    void AddArmorIdentityAffixes(FNWGeneratedItem& Item, FRandomStream& Random, float Scale)
    {
        switch (Item.ArmorWeight)
        {
            case ENWArmorWeight::Heavy:
                AddAffixIfMissing(Item, ENWAffixType::Armor, Random.FRandRange(2.6f, 4.8f) * Scale);
                AddAffixIfMissing(Item, ENWAffixType::Vitality, Random.FRandRange(1.6f, 3.4f) * Scale);
                if (Item.Slot == ENWEquipmentSlot::Chest || Item.Slot == ENWEquipmentSlot::Head)
                {
                    AddAffixIfMissing(Item, ENWAffixType::FortifiedGuard, Random.FRandRange(1.2f, 2.8f) * Scale);
                }
                break;
            case ENWArmorWeight::Light:
                AddAffixIfMissing(Item, ENWAffixType::Haste, Random.FRandRange(2.0f, 4.0f) * Scale);
                AddAffixIfMissing(Item, ENWAffixType::Healing, Random.FRandRange(1.3f, 3.0f) * Scale);
                if (Item.Slot == ENWEquipmentSlot::Boots || Item.Slot == ENWEquipmentSlot::Legs)
                {
                    AddAffixIfMissing(Item, ENWAffixType::DodgeEmpower, Random.FRandRange(1.2f, 2.6f) * Scale);
                }
                break;
            case ENWArmorWeight::Medium:
            default:
                AddAffixIfMissing(Item, ENWAffixType::Precision, Random.FRandRange(1.8f, 3.6f) * Scale);
                AddAffixIfMissing(Item, ENWAffixType::Power, Random.FRandRange(1.5f, 3.1f) * Scale);
                break;
        }
    }

    void AddWeaponIdentityAffixes(FNWGeneratedItem& Item, FRandomStream& Random, float Scale)
    {
        AddAffixIfMissing(Item, ENWAffixType::Power, Random.FRandRange(1.8f, 3.8f) * Scale);
        switch (Item.WeaponType)
        {
            case ENWWeaponType::Staff:
                AddAffixIfMissing(Item, ENWAffixType::AbilityEcho, Random.FRandRange(1.2f, 2.8f) * Scale);
                break;
            case ENWWeaponType::Greatsword:
                AddAffixIfMissing(Item, ENWAffixType::Executioner, Random.FRandRange(1.4f, 3.0f) * Scale);
                break;
            case ENWWeaponType::DualSwords:
                AddAffixIfMissing(Item, ENWAffixType::Haste, Random.FRandRange(1.4f, 3.0f) * Scale);
                break;
            case ENWWeaponType::SwordShield:
                AddAffixIfMissing(Item, ENWAffixType::CooldownOnCrit, Random.FRandRange(1.1f, 2.4f) * Scale);
                break;
            case ENWWeaponType::Daggers:
                AddAffixIfMissing(Item, ENWAffixType::PoisonCoating, Random.FRandRange(1.5f, 3.2f) * Scale);
                break;
            case ENWWeaponType::Bow:
                AddAffixIfMissing(Item, static_cast<ENWAffixType>(Random.RandRange(static_cast<int32>(ENWAffixType::PoisonCoating), static_cast<int32>(ENWAffixType::ShockChain))), Random.FRandRange(1.4f, 3.2f) * Scale);
                break;
            case ENWWeaponType::Firearm:
                AddAffixIfMissing(Item, ENWAffixType::Precision, Random.FRandRange(1.5f, 3.1f) * Scale);
                break;
            default:
                break;
        }
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
            Def.Passives = { Passive(TEXT("Reservatorio Arcano"), TEXT("+6% de aceleracao de cooldown.")), Passive(TEXT("Tecelagem Vital"), TEXT("+8% de cura recebida das habilidades.")), Passive(TEXT("Mestre Elemental"), TEXT("+12% de dano dos efeitos elementais.")) };
            break;
        case ENWWeaponType::Greatsword:
            Def.Name = TEXT("Espada Grande"); Def.BasicDamage = 32.0f; Def.BasicRange = 300.0f; Def.BasicRadius = 95.0f;
            Def.Abilities = { Ability(TEXT("Corte Demolidor"), ENWAbilityKind::DirectDamage, 44.0f, 360.0f, 110.0f), Ability(TEXT("Ruptura de Terra"), ENWAbilityKind::AreaDamage, 36.0f, 280.0f, 420.0f), Ability(TEXT("Renovacao de Batalha"), ENWAbilityKind::Heal, 25.0f, 0.0f, 0.0f) };
            Def.Passives = { Passive(TEXT("Forca do Colosso"), TEXT("+8% de dano base.")), Passive(TEXT("Folego de Guerra"), TEXT("+10 de stamina maxima.")), Passive(TEXT("Executor Implacavel"), TEXT("+15% de poder de execucao em inimigos feridos.")) };
            break;
        case ENWWeaponType::DualSwords:
            Def.Name = TEXT("Duas Espadas"); Def.BasicDamage = 23.0f; Def.BasicRange = 280.0f; Def.BasicRadius = 80.0f;
            Def.Abilities = { Ability(TEXT("Corte Cruzado"), ENWAbilityKind::DirectDamage, 36.0f, 330.0f, 85.0f), Ability(TEXT("Turbilhao de Aco"), ENWAbilityKind::AreaDamage, 30.0f, 240.0f, 390.0f), Ability(TEXT("Segundo Folego"), ENWAbilityKind::Heal, 22.0f, 0.0f, 0.0f) };
            Def.Passives = { Passive(TEXT("Ritmo Gemeo"), TEXT("+6% de chance critica.")), Passive(TEXT("Danca de Laminas"), TEXT("+10% no bonus apos esquiva.")), Passive(TEXT("Fome de Combate"), TEXT("+3% de roubo de vida.")) };
            break;
        case ENWWeaponType::SwordShield:
            Def.Name = TEXT("Espada e Escudo"); Def.BasicDamage = 24.0f; Def.BasicRange = 270.0f; Def.BasicRadius = 80.0f;
            Def.Abilities = { Ability(TEXT("Impacto de Escudo"), ENWAbilityKind::DirectDamage, 31.0f, 300.0f, 95.0f), Ability(TEXT("Varredura do Guardiao"), ENWAbilityKind::AreaDamage, 27.0f, 220.0f, 350.0f), Ability(TEXT("Graca do Defensor"), ENWAbilityKind::Heal, 31.0f, 0.0f, 0.0f) };
            Def.Passives = { Passive(TEXT("Baluarte"), TEXT("+14 de armadura.")), Passive(TEXT("Guarda Inabalavel"), TEXT("+12% de eficiencia de bloqueio.")), Passive(TEXT("Contra-Ataque"), TEXT("Parry recupera stamina adicional.")) };
            break;
        case ENWWeaponType::Daggers:
            Def.Name = TEXT("Adagas"); Def.BasicDamage = 21.0f; Def.BasicRange = 235.0f; Def.BasicRadius = 65.0f;
            Def.Abilities = { Ability(TEXT("Presa Sombria"), ENWAbilityKind::DirectDamage, 39.0f, 285.0f, 70.0f), Ability(TEXT("Leque de Laminas"), ENWAbilityKind::AreaDamage, 29.0f, 230.0f, 330.0f), Ability(TEXT("Sutura de Sangue"), ENWAbilityKind::Heal, 21.0f, 0.0f, 0.0f) };
            Def.Passives = { Passive(TEXT("Assassino Nato"), TEXT("+8% de chance critica.")), Passive(TEXT("Veneno Concentrado"), TEXT("+18% de veneno e sangramento.")), Passive(TEXT("Golpe Final"), TEXT("+18% de execucao contra alvos abaixo de 30%.")) };
            break;
        case ENWWeaponType::Bow:
            Def.Name = TEXT("Arco"); Def.BasicDamage = 25.0f; Def.BasicRange = 1550.0f; Def.BasicRadius = 45.0f;
            Def.Abilities = { Ability(TEXT("Flecha Perfurante"), ENWAbilityKind::DirectDamage, 40.0f, 1850.0f, 52.0f), Ability(TEXT("Chuva de Flechas"), ENWAbilityKind::AreaDamage, 31.0f, 1000.0f, 430.0f), Ability(TEXT("Curativo de Campo"), ENWAbilityKind::Heal, 24.0f, 0.0f, 0.0f) };
            Def.Passives = { Passive(TEXT("Olho de Aguia"), TEXT("+12% de alcance.")), Passive(TEXT("Aljava Elemental"), TEXT("Flechas podem alternar Fogo, Veneno, Eletrica e Gelo.")), Passive(TEXT("Cacador Preciso"), TEXT("+7% de chance critica.")) };
            break;
        case ENWWeaponType::Firearm:
            Def.Name = TEXT("Arma de Fogo"); Def.BasicDamage = 29.0f; Def.BasicRange = 1750.0f; Def.BasicRadius = 35.0f;
            Def.Abilities = { Ability(TEXT("Tiro de Impacto"), ENWAbilityKind::DirectDamage, 43.0f, 2050.0f, 42.0f), Ability(TEXT("Rajada de Chumbo"), ENWAbilityKind::AreaDamage, 33.0f, 750.0f, 300.0f), Ability(TEXT("Tonico de Adrenalina"), ENWAbilityKind::Heal, 23.0f, 0.0f, 0.0f) };
            Def.Passives = { Passive(TEXT("Polvora Refinada"), TEXT("+10% de dano.")), Passive(TEXT("Olho Morto"), TEXT("+6% de chance critica.")), Passive(TEXT("Recarga Tatica"), TEXT("+5% de aceleracao de cooldown.")) };
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
    return static_cast<ENWWeaponType>((static_cast<uint8>(WeaponType) + 1u) % 7u);
}

FString NWCombat::RarityToString(ENWItemRarity Rarity)
{
    switch (Rarity)
    {
        case ENWItemRarity::Common: return TEXT("Comum");
        case ENWItemRarity::Uncommon: return TEXT("Incomum");
        case ENWItemRarity::Rare: return TEXT("Raro");
        case ENWItemRarity::Epic: return TEXT("Epico");
        case ENWItemRarity::Legendary: return TEXT("Lendario");
        default: return TEXT("Desconhecido");
    }
}

FString NWCombat::EquipmentSlotToString(ENWEquipmentSlot Slot)
{
    switch (Slot)
    {
        case ENWEquipmentSlot::Head: return TEXT("Cabeca");
        case ENWEquipmentSlot::Chest: return TEXT("Peitoral");
        case ENWEquipmentSlot::Gloves: return TEXT("Luvas");
        case ENWEquipmentSlot::Legs: return TEXT("Pernas");
        case ENWEquipmentSlot::Boots: return TEXT("Botas");
        default: return TEXT("Item");
    }
}

FString NWCombat::ArmorWeightToString(ENWArmorWeight ArmorWeight)
{
    switch (ArmorWeight)
    {
        case ENWArmorWeight::Light: return TEXT("Leve");
        case ENWArmorWeight::Heavy: return TEXT("Pesada");
        default: return TEXT("Media");
    }
}

FString NWCombat::ItemKindToString(ENWItemKind Kind)
{
    switch (Kind)
    {
        case ENWItemKind::Weapon: return TEXT("Armas");
        case ENWItemKind::Consumable: return TEXT("Consumiveis");
        default: return TEXT("Armaduras");
    }
}

FString NWCombat::ArrowElementToString(ENWArrowElement Element)
{
    switch (Element)
    {
        case ENWArrowElement::Fire: return TEXT("Fogo");
        case ENWArrowElement::Poison: return TEXT("Veneno");
        case ENWArrowElement::Lightning: return TEXT("Eletrica");
        case ENWArrowElement::Frost: return TEXT("Gelo");
        default: return TEXT("Fisica");
    }
}

FString NWCombat::AffixToString(ENWAffixType Affix)
{
    switch (Affix)
    {
        case ENWAffixType::Power: return TEXT("Poder");
        case ENWAffixType::Vitality: return TEXT("Vitalidade");
        case ENWAffixType::Precision: return TEXT("Precisao");
        case ENWAffixType::Haste: return TEXT("Aceleracao");
        case ENWAffixType::Healing: return TEXT("Cura");
        case ENWAffixType::PoisonCoating: return TEXT("Revestimento Venenoso");
        case ENWAffixType::LifeSteal: return TEXT("Roubo de Vida");
        case ENWAffixType::Armor: return TEXT("Armadura");
        case ENWAffixType::Firebrand: return TEXT("Marca de Fogo");
        case ENWAffixType::Frostbite: return TEXT("Mordida Gelida");
        case ENWAffixType::ShockChain: return TEXT("Corrente Eletrica");
        case ENWAffixType::AbilityEcho: return TEXT("Eco de Habilidade");
        case ENWAffixType::CooldownOnCrit: return TEXT("Ritmo Critico");
        case ENWAffixType::FortifiedGuard: return TEXT("Guarda Fortificada");
        case ENWAffixType::ParryHeal: return TEXT("Parry Restaurador");
        case ENWAffixType::DodgeEmpower: return TEXT("Impulso da Esquiva");
        case ENWAffixType::Bleed: return TEXT("Sangramento");
        case ENWAffixType::Executioner: return TEXT("Executor");
        default: return TEXT("Atributo");
    }
}

FLinearColor NWCombat::RarityColor(ENWItemRarity Rarity)
{
    switch (Rarity)
    {
        case ENWItemRarity::Common: return FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);
        case ENWItemRarity::Uncommon: return FLinearColor(0.20f, 0.90f, 0.30f, 1.0f);
        case ENWItemRarity::Rare: return FLinearColor(0.15f, 0.45f, 1.00f, 1.0f);
        case ENWItemRarity::Epic: return FLinearColor(0.65f, 0.20f, 0.95f, 1.0f);
        case ENWItemRarity::Legendary: return FLinearColor(1.00f, 0.55f, 0.05f, 1.0f);
        default: return FLinearColor::White;
    }
}

FNWGeneratedItem NWCombat::GenerateProceduralItem(int32 LootSeed, int32 WorldEpoch)
{
    FRandomStream Random(LootSeed ^ (WorldEpoch * 7919));
    FNWGeneratedItem Item;
    Item.ItemSeed = LootSeed;
    Item.AppearanceSeed = Random.RandRange(1, MAX_int32);
    Item.ItemLevel = FMath::Max(1, 1 + WorldEpoch / 3 + Random.RandRange(0, 2));
    Item.Kind = Random.RandRange(0, 99) < 55 ? ENWItemKind::Armor : ENWItemKind::Weapon;

    const int32 RarityRoll = Random.RandRange(0, 999);
    if (RarityRoll < 470) Item.Rarity = ENWItemRarity::Common;
    else if (RarityRoll < 735) Item.Rarity = ENWItemRarity::Uncommon;
    else if (RarityRoll < 900) Item.Rarity = ENWItemRarity::Rare;
    else if (RarityRoll < 980) Item.Rarity = ENWItemRarity::Epic;
    else Item.Rarity = ENWItemRarity::Legendary;

    const int32 RarityLevel = static_cast<int32>(Item.Rarity) + 1;
    const float IdentityScale = (0.70f + RarityLevel * 0.32f) * (1.0f + Item.ItemLevel * 0.025f);

    if (Item.Kind == ENWItemKind::Armor)
    {
        Item.Slot = static_cast<ENWEquipmentSlot>(Random.RandRange(0, 4));
        Item.ArmorWeight = RollArmorWeight(Random);
        Item.StyleId = PickArmorStyle(Item.ArmorWeight, Random);
        AddArmorIdentityAffixes(Item, Random, IdentityScale);
    }
    else
    {
        Item.WeaponType = static_cast<ENWWeaponType>(Random.RandRange(0, 6));
        Item.StyleId = PickWeaponStyle(Item.WeaponType, Random);
        AddWeaponIdentityAffixes(Item, Random, IdentityScale);
    }

    const int32 DesiredAffixes = FMath::Clamp(1 + (RarityLevel - 1) / 2 + (Item.Rarity == ENWItemRarity::Legendary ? 1 : 0), 2, 5);
    TSet<ENWAffixType> UsedAffixes;
    for (const FNWItemAffix& Existing : Item.Affixes) { UsedAffixes.Add(Existing.Type); }

    const int32 AffixTypeCount = static_cast<int32>(ENWAffixType::Executioner) + 1;
    int32 Guard = 0;
    while (Item.Affixes.Num() < DesiredAffixes && Guard++ < 150)
    {
        const ENWAffixType Type = static_cast<ENWAffixType>(Random.RandRange(0, AffixTypeCount - 1));
        if (UsedAffixes.Contains(Type) || !IsItemAffixCompatible(Item, Type)) { continue; }

        FNWItemAffix Affix;
        Affix.Type = Type;
        const float Base = Random.FRandRange(1.2f, 3.2f) * (0.8f + RarityLevel * 0.42f) * (1.0f + Item.ItemLevel * 0.035f);
        Affix.Magnitude = FMath::RoundToFloat(Base * 10.0f) / 10.0f;
        Item.Affixes.Add(Affix);
        UsedAffixes.Add(Type);
    }

    if (Item.Kind == ENWItemKind::Armor)
    {
        Item.Name = FString::Printf(TEXT("%s %s %s - %s E%d [%04d]"), *RarityToString(Item.Rarity), *ArmorWeightToString(Item.ArmorWeight), *EquipmentSlotToString(Item.Slot), *Item.StyleId.ToString(), Item.ItemLevel, FMath::Abs(LootSeed % 10000));
    }
    else
    {
        Item.Name = FString::Printf(TEXT("%s %s - %s E%d [%04d]"), *RarityToString(Item.Rarity), *WeaponTypeToString(Item.WeaponType), *Item.StyleId.ToString(), Item.ItemLevel, FMath::Abs(LootSeed % 10000));
    }
    return Item;
}

FNWGeneratedItem NWCombat::GenerateLegendaryDungeonItem(int32 LootSeed, int32 WorldEpoch, FName DungeonTheme)
{
    FNWGeneratedItem Item = GenerateProceduralItem(LootSeed, FMath::Max(1, WorldEpoch + 3));
    Item.Rarity = ENWItemRarity::Legendary;
    Item.ItemLevel += 3;
    Item.SetId = DungeonTheme.IsNone() ? FName(TEXT("AncientLegend")) : DungeonTheme;
    Item.StyleId = Item.SetId;

    FRandomStream Random(LootSeed ^ 0x5A17C3);
    const float LegendaryScale = 2.2f + Item.ItemLevel * 0.08f;

    if (DungeonTheme == FName(TEXT("DarkCastle")))
    {
        AddAffixIfMissing(Item, ENWAffixType::LifeSteal, Random.FRandRange(3.0f, 6.0f) * LegendaryScale);
        AddAffixIfMissing(Item, ENWAffixType::Executioner, Random.FRandRange(2.5f, 5.5f) * LegendaryScale);
        AddAffixIfMissing(Item, ENWAffixType::AbilityEcho, Random.FRandRange(2.0f, 4.5f) * LegendaryScale);
    }
    else
    {
        AddAffixIfMissing(Item, ENWAffixType::Frostbite, Random.FRandRange(2.0f, 4.0f) * LegendaryScale);
        AddAffixIfMissing(Item, ENWAffixType::ShockChain, Random.FRandRange(2.0f, 4.0f) * LegendaryScale);
        AddAffixIfMissing(Item, ENWAffixType::Vitality, Random.FRandRange(3.0f, 5.0f) * LegendaryScale);
    }

    const FString Detail = Item.Kind == ENWItemKind::Weapon
        ? WeaponTypeToString(Item.WeaponType)
        : FString::Printf(TEXT("%s %s"), *ArmorWeightToString(Item.ArmorWeight), *EquipmentSlotToString(Item.Slot));
    Item.Name = FString::Printf(TEXT("LENDARIO %s - %s [%04d]"), *Item.SetId.ToString(), *Detail, FMath::Abs(LootSeed % 10000));
    return Item;
}

FNWGeneratedItem NWCombat::GenerateBrutalTransformationPotion(int32 LootSeed, int32 WorldEpoch)
{
    FNWGeneratedItem Item;
    Item.ItemSeed = LootSeed;
    Item.AppearanceSeed = LootSeed ^ 0x71B4A1;
    Item.ItemLevel = FMath::Max(1, 4 + WorldEpoch / 2);
    Item.Kind = ENWItemKind::Consumable;
    Item.ConsumableType = ENWConsumableType::BrutalLegendaryTransformation;
    Item.Rarity = ENWItemRarity::Legendary;
    Item.StyleId = TEXT("BrutalLegendaryTransformation");
    Item.SetId = TEXT("BrutalLegendary");
    Item.Name = TEXT("POCAO LENDARIA - Metamorfose da Armadura Brutal");
    return Item;
}

float NWCombat::GetItemScore(const FNWGeneratedItem& Item)
{
    if (Item.Kind == ENWItemKind::Consumable) { return Item.Rarity == ENWItemRarity::Legendary ? 500.0f : 100.0f; }

    float Score = Item.ItemLevel * 2.5f + (static_cast<int32>(Item.Rarity) + 1) * 12.0f;
    if (Item.Kind == ENWItemKind::Weapon) { Score += 10.0f; }
    for (const FNWItemAffix& Affix : Item.Affixes)
    {
        float Weight = 1.0f;
        switch (Affix.Type)
        {
            case ENWAffixType::PoisonCoating:
            case ENWAffixType::Firebrand:
            case ENWAffixType::ShockChain:
            case ENWAffixType::AbilityEcho:
            case ENWAffixType::ParryHeal:
            case ENWAffixType::DodgeEmpower:
                Weight = 1.45f;
                break;
            default:
                break;
        }
        Score += Affix.Magnitude * Weight;
    }
    if (!Item.SetId.IsNone()) { Score += 24.0f; }
    return Score;
}
