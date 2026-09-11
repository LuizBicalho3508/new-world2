#pragma once

#include "CoreMinimal.h"
#include "NWCombatTypes.generated.h"

UENUM(BlueprintType)
enum class ENWWeaponType : uint8
{
    Staff UMETA(DisplayName="Cajado"),
    Greatsword UMETA(DisplayName="Espada Grande"),
    DualSwords UMETA(DisplayName="Duas Espadas"),
    SwordShield UMETA(DisplayName="Espada e Escudo"),
    Daggers UMETA(DisplayName="Adagas"),
    Bow UMETA(DisplayName="Arco"),
    Firearm UMETA(DisplayName="Arma de Fogo")
};

UENUM(BlueprintType)
enum class ENWAbilityKind : uint8
{
    DirectDamage,
    AreaDamage,
    Heal
};

UENUM(BlueprintType)
enum class ENWEquipmentSlot : uint8
{
    Head,
    Chest,
    Gloves,
    Legs,
    Boots
};

UENUM(BlueprintType)
enum class ENWArmorWeight : uint8
{
    Light UMETA(DisplayName="Leve"),
    Medium UMETA(DisplayName="Media"),
    Heavy UMETA(DisplayName="Pesada")
};

UENUM(BlueprintType)
enum class ENWItemRarity : uint8
{
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary
};

UENUM(BlueprintType)
enum class ENWAffixType : uint8
{
    Power,
    Vitality,
    Precision,
    Haste,
    Healing,
    PoisonCoating,
    LifeSteal,
    Armor,
    Firebrand,
    Frostbite,
    ShockChain,
    AbilityEcho,
    CooldownOnCrit,
    FortifiedGuard,
    ParryHeal,
    DodgeEmpower,
    Bleed,
    Executioner
};

UENUM(BlueprintType)
enum class ENWCombatState : uint8
{
    Normal,
    Blocking,
    Dodging,
    Staggered
};

USTRUCT(BlueprintType)
struct FNWWeaponAbilityDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ENWAbilityKind Kind = ENWAbilityKind::DirectDamage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Power = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Range = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Radius = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Cooldown = 3.0f;
};

USTRUCT(BlueprintType)
struct FNWWeaponDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ENWWeaponType Type = ENWWeaponType::Greatsword;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float BasicDamage = 24.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float BasicRange = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float BasicRadius = 75.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FNWWeaponAbilityDefinition> Abilities;
};

USTRUCT(BlueprintType)
struct FNWItemAffix
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ENWAffixType Type = ENWAffixType::Power;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Magnitude = 1.0f;
};

USTRUCT(BlueprintType)
struct FNWGeneratedItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ItemSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ENWEquipmentSlot Slot = ENWEquipmentSlot::Gloves;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ENWArmorWeight ArmorWeight = ENWArmorWeight::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ENWItemRarity Rarity = ENWItemRarity::Common;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ItemLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 AppearanceSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName StyleId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FNWItemAffix> Affixes;
};
