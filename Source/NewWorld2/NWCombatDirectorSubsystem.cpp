#include "NWCombatDirectorSubsystem.h"

#include "Engine/World.h"

UNWCombatDirectorSubsystem::FTargetState& UNWCombatDirectorSubsystem::FindOrAddTargetState(AActor* Target)
{
    for (FTargetState& State : TargetStates)
    {
        if (State.Target.Get() == Target)
        {
            return State;
        }
    }

    FTargetState& NewState = TargetStates.AddDefaulted_GetRef();
    NewState.Target = Target;
    return NewState;
}

void UNWCombatDirectorSubsystem::Compact(float Now)
{
    if (Now < NextCompactAt)
    {
        return;
    }

    NextCompactAt = Now + 1.0f;

    for (int32 Index = TargetStates.Num() - 1; Index >= 0; --Index)
    {
        FTargetState& State = TargetStates[Index];
        if (!State.Target.IsValid())
        {
            TargetStates.RemoveAtSwap(Index, 1, EAllowShrinking::No);
            continue;
        }

        State.Permits.RemoveAllSwap([Now](const FAttackPermit& Permit)
        {
            return !Permit.Attacker.IsValid() || Permit.ExpiresAt <= Now;
        }, EAllowShrinking::No);
    }
}

bool UNWCombatDirectorSubsystem::RequestAttackPermit(AActor* Attacker, AActor* Target, bool bWorldBoss)
{
    if (!IsValid(Attacker) || !IsValid(Target) || !GetWorld())
    {
        return false;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    Compact(Now);

    FTargetState& State = FindOrAddTargetState(Target);
    State.Permits.RemoveAllSwap([Now](const FAttackPermit& Permit)
    {
        return !Permit.Attacker.IsValid() || Permit.ExpiresAt <= Now;
    }, EAllowShrinking::No);

    for (const FAttackPermit& Permit : State.Permits)
    {
        if (Permit.Attacker.Get() == Attacker)
        {
            return true;
        }
    }

    const int32 MaxConcurrentAttackers = bWorldBoss ? 3 : 2;
    if (State.Permits.Num() >= MaxConcurrentAttackers || Now < State.NextPermitAt)
    {
        return false;
    }

    FAttackPermit& Permit = State.Permits.AddDefaulted_GetRef();
    Permit.Attacker = Attacker;
    Permit.ExpiresAt = Now + (bWorldBoss ? 0.82f : 0.62f);

    // Deterministic jitter prevents multiple enemies that reached melee range on the
    // same server frame from falling into a permanent synchronized rhythm.
    const float Jitter = 0.035f * static_cast<float>((++State.Sequence + Attacker->GetUniqueID()) % 5);
    State.NextPermitAt = Now + (bWorldBoss ? 0.12f : 0.20f) + Jitter;
    return true;
}

void UNWCombatDirectorSubsystem::ForgetActor(const AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    for (int32 Index = TargetStates.Num() - 1; Index >= 0; --Index)
    {
        FTargetState& State = TargetStates[Index];
        if (State.Target.Get() == Actor)
        {
            TargetStates.RemoveAtSwap(Index, 1, EAllowShrinking::No);
            continue;
        }

        State.Permits.RemoveAllSwap([Actor](const FAttackPermit& Permit)
        {
            return Permit.Attacker.Get() == Actor;
        }, EAllowShrinking::No);
    }
}
