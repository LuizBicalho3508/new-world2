#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NWCombatDirectorSubsystem.generated.h"

/**
 * Server-side combat pacing inspired by modern action-RPG encounter directors.
 *
 * The subsystem prevents every nearby NPC from landing attacks in the same frame.
 * It intentionally owns only scheduling/pacing; damage, movement and animation stay
 * inside the enemy actor. This keeps encounter orchestration independent from a
 * specific enemy implementation and makes it safe for dedicated-server use.
 */
UCLASS()
class NEWORLD2_API UNWCombatDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Requests a short attack commitment against Target.
     * Returns true when the attacker is allowed to start its attack now.
     */
    bool RequestAttackPermit(AActor* Attacker, AActor* Target, bool bWorldBoss);

    /** Immediately forgets an actor from all target/permit state. */
    void ForgetActor(const AActor* Actor);

private:
    struct FAttackPermit
    {
        TWeakObjectPtr<AActor> Attacker;
        float ExpiresAt = 0.0f;
    };

    struct FTargetState
    {
        TWeakObjectPtr<AActor> Target;
        TArray<FAttackPermit> Permits;
        float NextPermitAt = 0.0f;
        uint32 Sequence = 0;
    };

    FTargetState& FindOrAddTargetState(AActor* Target);
    void Compact(float Now);

    TArray<FTargetState> TargetStates;
    float NextCompactAt = 0.0f;
};
