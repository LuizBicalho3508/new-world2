#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWPremiumGameplayDirector.generated.h"

class ANWCharacter;
class ANWEnemy;

/**
 * Watchdog do vertical slice premium.
 *
 * Responsabilidades:
 *  - garantir que o HUD nativo exista depois da possessao local;
 *  - observar casts pelas transicoes de cooldown;
 *  - gerar feedback visual deterministico sem Niagara dinamico;
 *  - caso uma habilidade de dano tenha sido aceita mas nao tenha acertado nenhum
 *    inimigo por causa do sweep/camera de terceira pessoa, aplicar aim-assist como
 *    fallback APENAS depois de confirmar que nenhum alvo perdeu vida no cast nativo;
 *  - produzir logs objetivos para o proximo teste.
 */
UCLASS()
class NEWORLD2_API ANWPremiumGameplayDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWPremiumGameplayDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    struct FPlayerCooldownState
    {
        ENWWeaponType Weapon = ENWWeaponType::Greatsword;
        TArray<float> Cooldowns;
        bool bInitialized = false;
    };

    struct FPendingAbilityAssist
    {
        TWeakObjectPtr<ANWCharacter> Character;
        ENWWeaponType Weapon = ENWWeaponType::Greatsword;
        int32 AbilityIndex = 0;
        float ResolveAt = 0.0f;
        TArray<TWeakObjectPtr<ANWEnemy>> ObservedEnemies;
        TArray<float> HealthRatiosAtCast;
    };

    void EnsureLocalHUDs(float DeltaSeconds);
    void DetectAbilityCasts();
    void ResolvePendingAssists();
    void QueueAbilityAssist(ANWCharacter* Character, ENWWeaponType Weapon, int32 AbilityIndex);
    void SpawnCastFeedback(ANWCharacter* Character, const FNWWeaponAbilityDefinition& Ability, int32 AbilityIndex);
    bool NativeCastDamagedEnemy(const FPendingAbilityAssist& Pending) const;
    int32 ApplyFallbackAbilityHit(const FPendingAbilityAssist& Pending);
    ANWEnemy* FindBestAimTarget(ANWCharacter* Character, float MaxRange) const;

    TMap<TWeakObjectPtr<ANWCharacter>, FPlayerCooldownState> PlayerStates;
    TArray<FPendingAbilityAssist> PendingAssists;

    UPROPERTY(EditDefaultsOnly, Category="Premium|Combat", meta=(ClampMin="0.03", ClampMax="0.40"))
    float AbilityVerificationDelay = 0.12f;

    UPROPERTY(EditDefaultsOnly, Category="Premium|Combat", meta=(ClampMin="1.0", ClampMax="2.5"))
    float AimAssistRangeMultiplier = 1.45f;

    UPROPERTY(EditDefaultsOnly, Category="Premium|Combat", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float MinimumAimDot = 0.12f;

    UPROPERTY(EditDefaultsOnly, Category="Premium|HUD", meta=(ClampMin="0.10", ClampMax="2.0"))
    float HudEnsureInterval = 0.50f;

    float HudAccumulator = 0.0f;
};
