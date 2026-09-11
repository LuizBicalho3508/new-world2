#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWWorldTypes.h"
#include "NWWorldEventDirector.generated.h"

class ANWCharacter;
class ANWDungeonSite;
class UNiagaraSystem;
class USoundBase;

UCLASS()
class NEWORLD2_API ANWWorldEventDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWWorldEventDirector();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="World|Time")
    float GetWorldTimeHours() const { return WorldTimeHours; }

    UFUNCTION(BlueprintPure, Category="World|Weather")
    ENWWeatherType GetCurrentWeather() const { return CurrentWeather; }

    UFUNCTION(BlueprintPure, Category="World|Biomes")
    ENWBiomeType GetBiomeAtLocation(const FVector& Location) const;

protected:
    virtual void BeginPlay() override;

private:
    void DiscoverPresentationAssets();
    void UpdateDayNight(float DeltaSeconds);
    void ApplyEnvironmentPresentation();
    void ChangeWeather();
    void UpdateLocalPlayerPresentation(float DeltaSeconds);
    void DetectAbilityUse(ANWCharacter* Character);
    void PlayAbilityPresentation(ANWCharacter* Character, int32 AbilityIndex);
    void PlayWeatherPresentation(ANWCharacter* Character, ENWWeatherType EffectiveWeather);
    void SpawnWorldDungeons();

    UNiagaraSystem* FindBestNiagara(const TArray<FString>& Keywords) const;
    USoundBase* FindBestSound(const TArray<FString>& Keywords) const;
    ENWWeatherType GetEffectiveWeatherForBiome(ENWBiomeType Biome) const;
    TArray<FString> GetAbilityKeywords(ENWWeaponType Weapon, int32 AbilityIndex) const;
    FString BiomeToString(ENWBiomeType Biome) const;
    FString WeatherToString(ENWWeatherType Weather) const;

    UPROPERTY(Replicated, VisibleAnywhere, Category="World|Time")
    float WorldTimeHours = 9.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Weather, VisibleAnywhere, Category="World|Weather")
    ENWWeatherType CurrentWeather = ENWWeatherType::Clear;

    UFUNCTION()
    void OnRep_Weather();

    UPROPERTY(EditDefaultsOnly, Category="World|Time", meta=(ClampMin="120.0"))
    float FullDayDurationSeconds = 720.0f;

    UPROPERTY(EditDefaultsOnly, Category="World|Weather", meta=(ClampMin="30.0"))
    float WeatherChangeIntervalSeconds = 95.0f;

    UPROPERTY(EditDefaultsOnly, Category="World|Biomes", meta=(ClampMin="1000.0"))
    float BiomeCellSize = 4800.0f;

    TArray<FAssetData> NiagaraAssets;
    TArray<FAssetData> SoundAssets;
    TMap<TWeakObjectPtr<ANWCharacter>, TArray<float>> PreviousAbilityCooldowns;
    TMap<TWeakObjectPtr<ANWCharacter>, ENWBiomeType> PreviousBiomes;
    TMap<TWeakObjectPtr<ANWCharacter>, ENWWeatherType> PreviousEffectiveWeather;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ANWDungeonSite>> DungeonSites;

    FTimerHandle WeatherTimer;
    float EnvironmentAccumulator = 0.0f;
    float WeatherFxAccumulator = 0.0f;
};
