#pragma once

#include "CoreMinimal.h"
#include "NWWorldTypes.generated.h"

UENUM(BlueprintType)
enum class ENWBiomeType : uint8
{
    TemperateForest UMETA(DisplayName="Floresta Temperada"),
    Desert UMETA(DisplayName="Deserto"),
    Snow UMETA(DisplayName="Neve"),
    Swamp UMETA(DisplayName="Pantano"),
    Volcanic UMETA(DisplayName="Vulcanico"),
    Haunted UMETA(DisplayName="Amaldicoado")
};

UENUM(BlueprintType)
enum class ENWWeatherType : uint8
{
    Clear UMETA(DisplayName="Ceu Limpo"),
    Rain UMETA(DisplayName="Chuva"),
    Storm UMETA(DisplayName="Tempestade"),
    Snow UMETA(DisplayName="Nevasca"),
    Sandstorm UMETA(DisplayName="Tempestade de Areia"),
    HeavyFog UMETA(DisplayName="Neblina Densa")
};

UENUM(BlueprintType)
enum class ENWDungeonType : uint8
{
    DarkCastle UMETA(DisplayName="Castelo Sombrio"),
    AncientCave UMETA(DisplayName="Caverna Ancestral")
};

USTRUCT(BlueprintType)
struct FNWBiomeCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FIntPoint Cell = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ENWBiomeType Biome = ENWBiomeType::TemperateForest;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Seed = 0;
};
