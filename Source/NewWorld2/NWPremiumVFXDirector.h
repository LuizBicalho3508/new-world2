#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWPremiumV6AbilityLibrary.h"
#include "NWPremiumVFXDirector.generated.h"

class ANWCharacter;
class UNiagaraSystem;

/**
 * Premium V6 VFX director.
 * Faz curadoria uma vez no boot, bloqueia sistemas que exigem Mesh Distance
 * Fields no perfil Linux atual e apresenta cada arma com identidade propria.
 */
UCLASS()
class NEWORLD2_API ANWPremiumVFXDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWPremiumVFXDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    struct FPlayerVFXState
    {
        ENWWeaponType Weapon = ENWWeaponType::Greatsword;
        TArray<float> Cooldowns;
        bool bInitialized = false;
    };

    void ScanAndCacheVFX();
    void DetectCasts();
    void PlayCastPresentation(ANWCharacter* Character, int32 AbilityIndex);
    void PlayStableAbilityAnimation(ANWCharacter* Character, int32 AbilityIndex);
    FVector ResolveAimPoint(ANWCharacter* Character, const FNWWeaponAbilityDefinition& Ability) const;
    void SpawnLayeredVFX(const FVector& Location, ENWWeaponType Weapon, int32 AbilityIndex, ENWArrowElement ArrowElement);
    void WarmupCachedSystems();

    bool IsSafeVFXAsset(const FAssetData& Asset) const;
    int32 ScoreVFXAsset(const FAssetData& Asset, const TArray<FString>& Keywords) const;
    void SelectSystems(const TArray<FAssetData>& Assets, const TArray<FString>& Keywords, TArray<TObjectPtr<UNiagaraSystem>>& OutSystems, int32 MaxSystems);
    const TArray<TObjectPtr<UNiagaraSystem>>& GetSystemsForTheme(NWPremiumV6::ESkillTheme Theme) const;

    TMap<TWeakObjectPtr<ANWCharacter>, FPlayerVFXState> PlayerStates;

    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> FireSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> FrostSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> LightningSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> EarthSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> BladeSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> ShadowSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> BloodSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> HolySystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> ProjectileSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> GunpowderSystems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> GenericSystems;

    UPROPERTY(EditDefaultsOnly, Category="Premium V6|VFX", meta=(ClampMin="2", ClampMax="5"))
    int32 LayersPerCast = 4;

    UPROPERTY(EditDefaultsOnly, Category="Premium V6|VFX", meta=(ClampMin="0.05", ClampMax="0.25"))
    float TickInterval = 0.07f;

    bool bWarmupDone = false;
};