#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWStartupWarmupDirector.generated.h"

class APlayerController;
class UNWStartupLoadingWidget;

/**
 * Loading gate curto para PSO precache/streaming inicial.
 * Usa o modo Fast durante a tela de carregamento e depois devolve a compilacao
 * residual ao modo Background. A V5 evita segurar o usuario por minutos: o gate
 * tem teto baixo e prioriza abrir o jogo rapidamente.
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

    TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<UNWStartupLoadingWidget>> LoadingWidgets;

    float ElapsedSeconds = 0.0f;
    float LastOutstandingPSOTime = 0.0f;
    int32 PeakOutstandingPSOs = 0;
    bool bFinished = false;
    bool bInputBlocked = false;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="2.0", ClampMax="30.0"))
    float MinimumLoadingSeconds = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="0.25", ClampMax="5.0"))
    float QuietWindowSeconds = 0.75f;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="8.0", ClampMax="60.0"))
    float MaximumLoadingSeconds = 25.0f;
};
