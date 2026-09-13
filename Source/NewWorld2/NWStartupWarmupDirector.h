#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWStartupWarmupDirector.generated.h"

class APlayerController;
class UNiagaraSystem;
class UNWStartupLoadingWidget;

/**
 * Stability V10 startup gate.
 *
 * The V9 playtest proved that scanning/loading vendor Niagara graphs during
 * map startup created minute-long shader/texture compilation while the gate
 * itself reported no pending work. V10 only gives the PSO cache a short,
 * bounded head start; combat VFX uses a tiny deterministic whitelist and is
 * no longer force-spawned below the map.
 */
UCLASS()
class NEWORLD2_API ANWStartupWarmupDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWStartupWarmupDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void EnsureLocalLoadingScreens();
    void SetPlayerInputBlocked(bool bBlocked);
    void FinishWarmup(bool bTimedOut);
    void PrimeCombatNiagara();
    int32 CountPendingCombatNiagara();

    TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<UNWStartupLoadingWidget>> LoadingWidgets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UNiagaraSystem>> CombatNiagaraSystems;

    float ElapsedSeconds = 0.0f;
    float LastOutstandingWorkTime = 0.0f;
    int32 PeakOutstandingPSOs = 0;
    int32 PeakOutstandingNiagara = 0;
    bool bFinished = false;
    bool bInputBlocked = false;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="1.0", ClampMax="15.0"))
    float MinimumLoadingSeconds = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="0.20", ClampMax="3.0"))
    float QuietWindowSeconds = 0.55f;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="4.0", ClampMax="30.0"))
    float MaximumLoadingSeconds = 12.0f;
};
