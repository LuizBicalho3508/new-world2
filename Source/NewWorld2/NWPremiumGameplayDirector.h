#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWPremiumGameplayDirector.generated.h"

class ANWCharacter;
class ANWEnemy;

/**
 * Diretor de gameplay do vertical slice premium.
 *
 * A mira e action-RPG/free aim: Q/E/R sempre podem ser usados sem target lock.
 * O ponto de cast vem da camera/crosshair. Soft aim existe somente para absorver
 * imprecisoes normais de uma camera over-the-shoulder; ele nunca e requisito
 * para disparar a habilidade.
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
        FVector AimPoint = FVector::ZeroVector;
        FVector AimDirection = FVector::ForwardVector;
        TArray<TWeakObjectPtr<ANWEnemy>> ObservedEnemies;
        TArray<float> HealthRatiosAtCast;
    };

    void EnsurePremiumWorldSystems();
    void EnsureLocalHUDs(float DeltaSeconds);
    void EnsureTrainingEncounter(float DeltaSeconds);
    void DetectAbilityCasts();
    void ResolvePendingAssists();
    void QueueAbilityAssist(ANWCharacter* Character, ENWWeaponType Weapon, int32 AbilityIndex);
    void SpawnCastFeedback(ANWCharacter* Character, ENWWeaponType Weapon, const FNWWeaponAbilityDefinition& Ability, int32 AbilityIndex, const FVector& AimPoint);
    bool NativeCastDamagedEnemy(const FPendingAbilityAssist& Pending) const;
    int32 ApplyFallbackAbilityHit(const FPendingAbilityAssist& Pending);
    ANWEnemy* FindBestAimTarget(ANWCharacter* Character, const FVector& AimPoint, const FVector& AimDirection, float MaxRange, float SoftRadius) const;
    FVector ResolveFreeAimPoint(ANWCharacter* Character, float MaxRange, FVector& OutAimDirection) const;
    FVector ClampAimPointToAbilityRange(ANWCharacter* Character, const FVector& AimPoint, float MaxRange, bool bFlattenForMelee) const;
    bool IsRangedWeapon(ENWWeaponType Weapon) const;

    TMap<TWeakObjectPtr<ANWCharacter>, FPlayerCooldownState> PlayerStates;
    TArray<FPendingAbilityAssist> PendingAssists;

    UPROPERTY(EditDefaultsOnly, Category="Premium|Combat", meta=(ClampMin="0.03", ClampMax="0.40"))
    float AbilityVerificationDelay = 0.14f;

    // V3: 220 cm ainda exigia precisao de FPS para melee na camera lateral.
    // 380 cm mantem free aim, mas ajuda quando o reticulo passa perto da silhueta.
    UPROPERTY(EditDefaultsOnly, Category="Premium|Combat", meta=(ClampMin="80.0", ClampMax="500.0"))
    float SoftAimRadius = 380.0f;

    UPROPERTY(EditDefaultsOnly, Category="Premium|Combat", meta=(ClampMin="1000.0", ClampMax="12000.0"))
    float FreeAimTraceRange = 7000.0f;

    UPROPERTY(EditDefaultsOnly, Category="Premium|HUD", meta=(ClampMin="0.10", ClampMax="2.0"))
    float HudEnsureInterval = 0.50f;

    UPROPERTY(EditDefaultsOnly, Category="Premium|Playtest", meta=(ClampMin="3", ClampMax="10"))
    int32 DesiredNearbyTrainingEnemies = 5;

    UPROPERTY(EditDefaultsOnly, Category="Premium|Playtest", meta=(ClampMin="500.0", ClampMax="3000.0"))
    float TrainingEncounterRadius = 1900.0f;

    float HudAccumulator = 0.0f;
    float TrainingAccumulator = 0.0f;
    bool bPremiumWorldSystemsSpawned = false;
    bool bTrainingEncounterReady = false;
};
