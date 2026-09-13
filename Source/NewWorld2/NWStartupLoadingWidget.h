#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NWStartupLoadingWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * Overlay nativo de carregamento do vertical slice.
 * Mantem a tela de jogo coberta enquanto DDC/PSO e apresentacao inicial aquecem,
 * evitando que o jogador veja pop-in ou interaja durante compilacoes iniciais.
 */
UCLASS()
class NEWORLD2_API UNWStartupLoadingWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void UpdateWarmupStatus(int32 RemainingPSOs, int32 PeakPSOs, float ElapsedSeconds, bool bFinishing);

protected:
    virtual void NativeConstruct() override;

private:
    void BuildWidgetTreeIfNeeded();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HintText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> ProgressBar;
};
