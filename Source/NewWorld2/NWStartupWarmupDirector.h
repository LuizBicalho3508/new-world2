#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWStartupWarmupDirector.generated.h"

class APlayerController;
class UNiagaraSystem;
class UNWStartupLoadingWidget;

/**
 * Premium V7 startup gate.
 * Waits for both PSO precache and the small set of Niagara systems most likely
 * to be used by combat before returning input to the player. Heavy compilation
 * therefore happens behind the loading layer instead of on the first Q/E/R cast.
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

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="2.0", ClampMax="30.0"))
    float MinimumLoadingSeconds = 6.0f;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="0.25", ClampMax="5.0"))
    float QuietWindowSeconds = 0.85f;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="8.0", ClampMax="60.0"))
    float MaximumLoadingSeconds = 45.0f;
};
