#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWStartupWarmupDirector.generated.h"

class APlayerController;
class UNWStartupLoadingWidget;

/**
 * Loading gate para PSO precache/streaming inicial.
 * Usa o modo Fast do ShaderPipelineCache enquanto a tela de carregamento esta
 * visivel e so libera o controle depois de uma janela sem PSOs pendentes.
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
    float MinimumLoadingSeconds = 6.0f;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="0.25", ClampMax="5.0"))
    float QuietWindowSeconds = 1.25f;

    UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="10.0", ClampMax="120.0"))
    float MaximumLoadingSeconds = 45.0f;
};
