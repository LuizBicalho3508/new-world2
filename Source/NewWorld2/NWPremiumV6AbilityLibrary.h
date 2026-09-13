#pragma once

#include "CoreMinimal.h"
#include "NWCombatTypes.h"

namespace NWPremiumV6
{
    enum class ESkillTheme : uint8
    {
        Fire,
        Frost,
        Lightning,
        Earth,
        Blade,
        Shadow,
        Blood,
        Holy,
        Projectile,
        Gunpowder
    };

    inline FString AbilityName(ENWWeaponType Weapon, int32 Slot)
    {
        switch (Weapon)
        {
            case ENWWeaponType::Staff:
                return Slot == 0 ? TEXT("Lanca de Fogo") : Slot == 1 ? TEXT("Prisao Glacial") : TEXT("Tempestade Arcana");
            case ENWWeaponType::Greatsword:
                return Slot == 0 ? TEXT("Golpe do Colosso") : Slot == 1 ? TEXT("Ruptura Sismica") : TEXT("Ciclone de Guerra");
            case ENWWeaponType::DualSwords:
                return Slot == 0 ? TEXT("Cruzamento Relampago") : Slot == 1 ? TEXT("Turbilhao de Aco") : TEXT("Danca Rubra");
            case ENWWeaponType::SwordShield:
                return Slot == 0 ? TEXT("Impacto de Escudo") : Slot == 1 ? TEXT("Varredura do Guardiao") : TEXT("Bastiao Radiante");
            case ENWWeaponType::Daggers:
                return Slot == 0 ? TEXT("Passo Sombrio") : Slot == 1 ? TEXT("Leque Venenoso") : TEXT("Ritual de Sangue");
            case ENWWeaponType::Bow:
                return Slot == 0 ? TEXT("Flecha Perfurante") : Slot == 1 ? TEXT("Chuva Elemental") : TEXT("Rajada do Cacador");
            case ENWWeaponType::Firearm:
                return Slot == 0 ? TEXT("Tiro de Impacto") : Slot == 1 ? TEXT("Rajada de Chumbo") : TEXT("Municao Explosiva");
            default:
                return TEXT("Habilidade");
        }
    }

    inline FString AbilityDescription(ENWWeaponType Weapon, int32 Slot)
    {
        switch (Weapon)
        {
            case ENWWeaponType::Staff:
                return Slot == 0 ? TEXT("Projétil incendiario + queimadura") : Slot == 1 ? TEXT("Explosao de gelo + lentidao") : TEXT("Raios encadeados em area");
            case ENWWeaponType::Greatsword:
                return Slot == 0 ? TEXT("Corte pesado + stagger") : Slot == 1 ? TEXT("Slam no solo em grande area") : TEXT("Giro ofensivo com pulsos");
            case ENWWeaponType::DualSwords:
                return Slot == 0 ? TEXT("Avanco e corte cruzado") : Slot == 1 ? TEXT("Giro multi-hit") : TEXT("Sequencia com roubo de vida");
            case ENWWeaponType::SwordShield:
                return Slot == 0 ? TEXT("Atordoa o alvo frontal") : Slot == 1 ? TEXT("Controle em arco ao redor") : TEXT("Recupera vida/stamina e fortalece");
            case ENWWeaponType::Daggers:
                return Slot == 0 ? TEXT("Dash assassino + veneno") : Slot == 1 ? TEXT("Multi-hit em cone") : TEXT("Nuvem de sangue e veneno");
            case ENWWeaponType::Bow:
                return Slot == 0 ? TEXT("Disparo de alto alcance") : Slot == 1 ? TEXT("Area com flechas elementais") : TEXT("Volley conforme elemento ativo");
            case ENWWeaponType::Firearm:
                return Slot == 0 ? TEXT("Disparo de alto impacto") : Slot == 1 ? TEXT("Cone de pellets") : TEXT("Explosao de polvora em area");
            default:
                return TEXT("");
        }
    }

    inline ESkillTheme Theme(ENWWeaponType Weapon, int32 Slot, ENWArrowElement Arrow = ENWArrowElement::Physical)
    {
        switch (Weapon)
        {
            case ENWWeaponType::Staff:
                return Slot == 0 ? ESkillTheme::Fire : Slot == 1 ? ESkillTheme::Frost : ESkillTheme::Lightning;
            case ENWWeaponType::Greatsword:
                return Slot == 1 ? ESkillTheme::Earth : ESkillTheme::Blade;
            case ENWWeaponType::DualSwords:
                return Slot == 2 ? ESkillTheme::Blood : ESkillTheme::Blade;
            case ENWWeaponType::SwordShield:
                return Slot == 2 ? ESkillTheme::Holy : ESkillTheme::Blade;
            case ENWWeaponType::Daggers:
                return Slot == 1 ? ESkillTheme::Blade : Slot == 2 ? ESkillTheme::Blood : ESkillTheme::Shadow;
            case ENWWeaponType::Bow:
                if (Arrow == ENWArrowElement::Fire) return ESkillTheme::Fire;
                if (Arrow == ENWArrowElement::Frost) return ESkillTheme::Frost;
                if (Arrow == ENWArrowElement::Lightning) return ESkillTheme::Lightning;
                if (Arrow == ENWArrowElement::Poison) return ESkillTheme::Shadow;
                return ESkillTheme::Projectile;
            case ENWWeaponType::Firearm:
                return ESkillTheme::Gunpowder;
            default:
                return ESkillTheme::Blade;
        }
    }

    inline FLinearColor ThemeColor(ESkillTheme Theme)
    {
        switch (Theme)
        {
            case ESkillTheme::Fire: return FLinearColor(1.0f, 0.18f, 0.025f, 1.0f);
            case ESkillTheme::Frost: return FLinearColor(0.15f, 0.72f, 1.0f, 1.0f);
            case ESkillTheme::Lightning: return FLinearColor(0.38f, 0.55f, 1.0f, 1.0f);
            case ESkillTheme::Earth: return FLinearColor(0.72f, 0.34f, 0.08f, 1.0f);
            case ESkillTheme::Shadow: return FLinearColor(0.45f, 0.08f, 0.72f, 1.0f);
            case ESkillTheme::Blood: return FLinearColor(0.75f, 0.015f, 0.035f, 1.0f);
            case ESkillTheme::Holy: return FLinearColor(1.0f, 0.78f, 0.25f, 1.0f);
            case ESkillTheme::Projectile: return FLinearColor(0.42f, 0.95f, 0.48f, 1.0f);
            case ESkillTheme::Gunpowder: return FLinearColor(1.0f, 0.52f, 0.12f, 1.0f);
            default: return FLinearColor(0.88f, 0.92f, 1.0f, 1.0f);
        }
    }
}
