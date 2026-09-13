#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWPremiumV6CombatDirector.generated.h"

class ANWCharacter;
class ANWEnemy;

/**
 * Acrescenta identidade de gameplay às 21 habilidades sem substituir o combate
 * autoritativo existente. Detecta casts aceitos pelos cooldowns e aplica efeitos
 * secundarios por arma: fogo/gelo/raio, slam, giro, veneno, volley etc.
 */
UCLASS()
class NEWORLD2_API ANWPremiumV6CombatDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWPremiumV6CombatDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    struct FCastState
    {
        ENWWeaponType Weapon = ENWWeaponType::Greatsword;
        TArray<float> Cooldowns;
        bool bInitialized = false;
    };

    void DetectCasts();
    void ApplyWeaponSignature(ANWCharacter* Character, ENWWeaponType Weapon, int32 Slot);
    FVector ResolveAimPoint(ANWCharacter* Character, float MaxDistance) const;
    ANWEnemy* FindBestEnemyNear(const FVector& Point, float Radius) const;
    TArray<ANWEnemy*> FindEnemiesNear(const FVector& Point, float Radius, int32 MaxCount = 8) const;
    void ApplyDamage(ANWCharacter* Character, ANWEnemy* Enemy, float Damage) const;
    void ApplyDot(ANWCharacter* Character, ANWEnemy* Enemy, float DamagePerTick, int32 Ticks, float Interval);
    void ApplySlow(ANWEnemy* Enemy, float SpeedMultiplier, float Duration);
    void PulseDamage(ANWCharacter* Character, const FVector& Point, float Radius, float Damage, int32 Pulses, float Interval);

    TMap<TWeakObjectPtr<ANWCharacter>, FCastState> States;
};