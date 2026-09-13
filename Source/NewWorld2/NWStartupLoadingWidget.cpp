#include "NWStartupLoadingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UNWStartupLoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildWidgetTreeIfNeeded();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UNWStartupLoadingWidget::BuildWidgetTreeIfNeeded()
{
    if (!WidgetTree || WidgetTree->RootWidget) { return; }

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WarmupRoot"));
    WidgetTree->RootWidget = Root;

    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WarmupBackdrop"));
    Backdrop->SetBrushColor(FLinearColor(0.004f, 0.007f, 0.012f, 0.96f));
    UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
    BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    BackdropSlot->SetOffsets(FMargin(0.0f));

    UVerticalBox* Center = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WarmupCenter"));
    UCanvasPanelSlot* CenterSlot = Root->AddChildToCanvas(Center);
    CenterSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CenterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CenterSlot->SetPosition(FVector2D(0.0f, 0.0f));
    CenterSlot->SetSize(FVector2D(720.0f, 220.0f));

    auto MakeText = [this](const TCHAR* Name, const TCHAR* Text, int32 Size, const FLinearColor& Color)
    {
        UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
        Block->SetText(FText::FromString(Text));
        Block->SetJustification(ETextJustify::Center);
        Block->SetColorAndOpacity(FSlateColor(Color));
        FSlateFontInfo Font = Block->GetFont();
        Font.Size = Size;
        Block->SetFont(Font);
        Block->SetShadowOffset(FVector2D(1.0f, 1.0f));
        return Block;
    };

    TitleText = MakeText(TEXT("WarmupTitle"), TEXT("NEW WORLD 2"), 30, FLinearColor(0.92f, 0.78f, 0.42f, 1.0f));
    Center->AddChildToVerticalBox(TitleText);

    StatusText = MakeText(TEXT("WarmupStatus"), TEXT("Preparando shaders, materiais e mundo..."), 17, FLinearColor(0.93f, 0.95f, 1.0f, 1.0f));
    UVerticalBoxSlot* StatusSlot = Center->AddChildToVerticalBox(StatusText);
    StatusSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 8.0f));

    ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("WarmupProgress"));
    ProgressBar->SetPercent(0.05f);
    ProgressBar->SetFillColorAndOpacity(FLinearColor(0.95f, 0.62f, 0.16f, 1.0f));
    UVerticalBoxSlot* ProgressSlot = Center->AddChildToVerticalBox(ProgressBar);
    ProgressSlot->SetPadding(FMargin(70.0f, 8.0f));

    HintText = MakeText(TEXT("WarmupHint"), TEXT("O jogo libera o controle quando o lote inicial de PSOs estiver pronto."), 12, FLinearColor(0.68f, 0.73f, 0.82f, 1.0f));
    UVerticalBoxSlot* HintSlot = Center->AddChildToVerticalBox(HintText);
    HintSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
}

void UNWStartupLoadingWidget::UpdateWarmupStatus(int32 RemainingPSOs, int32 PeakPSOs, float ElapsedSeconds, bool bFinishing)
{
    BuildWidgetTreeIfNeeded();
    if (!StatusText || !ProgressBar) { return; }

    const int32 SafePeak = FMath::Max(PeakPSOs, RemainingPSOs);
    float Progress = SafePeak > 0 ? 1.0f - static_cast<float>(RemainingPSOs) / static_cast<float>(SafePeak) : FMath::Clamp(ElapsedSeconds / 6.0f, 0.05f, 0.95f);
    if (bFinishing) { Progress = 1.0f; }
    ProgressBar->SetPercent(FMath::Clamp(Progress, 0.03f, 1.0f));

    if (bFinishing)
    {
        StatusText->SetText(FText::FromString(TEXT("Mundo pronto. Entrando...")));
    }
    else if (RemainingPSOs > 0)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Preparando renderizacao  |  PSOs restantes: %d"), RemainingPSOs)));
    }
    else
    {
        StatusText->SetText(FText::FromString(TEXT("Finalizando streaming e cache inicial...")));
    }
}
