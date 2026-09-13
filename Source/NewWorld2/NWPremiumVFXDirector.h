#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWPremiumVFXDirector.generated.h"

class ANWCharacter;
class UNiagaraSystem;

/**
 * Curadoria VFX/animacao do Premium V3.
 * Faz scan uma vez no boot, rejeita packs demo quebrados e pre-carrega os
 * Niagara escolhidos antes do primeiro combate. Detecta casts pelos cooldowns,
 * substitui a animacao instavel por montages conhecidos do personagem e dispara
 * camadas de particulas profissionais sem fazer AssetRegistry search durante a luta.
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
    void SpawnLayeredVFX(const FVector& Location, ENWAbilityKind Kind, ENWWeaponType Weapon, int32 AbilityIndex);
    void WarmupCachedSystems();

    bool IsSafeVFXAsset(const FAssetData& Asset) const;
    int32 ScoreVFXAsset(const FAssetData& Asset, const TArray<FString>& Keywords) const;
    void SelectSystems(const TArray<FAssetData>& Assets, const TArray<FString>& Keywords, TArray<TObjectPtr<UNiagaraSystem>>& OutSystems, int32 MaxSystems);

    TMap<TWeakObjectPtr<ANWCharacter>, FPlayerVFXState> PlayerStates;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UNiagaraSystem>> DirectDamageSystems;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UNiagaraSystem>> AreaDamageSystems;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UNiagaraSystem>> HealSystems;

    UPROPERTY(EditDefaultsOnly, Category="Premium V3|VFX", meta=(ClampMin="1", ClampMax="5"))
    int32 LayersPerCast = 3;

    UPROPERTY(EditDefaultsOnly, Category="Premium V3|VFX", meta=(ClampMin="0.05", ClampMax="1.0"))
    float TickInterval = 0.08f;

    bool bWarmupDone = false;
};
