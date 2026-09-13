#include "NWActionReticleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

void UNWActionReticleWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildReticle();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    UE_LOG(LogTemp, Display, TEXT("[RETICLE] mira action-RPG ativa em modo FREE AIM."));
}

void UNWActionReticleWidget::BuildReticle()
{
    if (!WidgetTree || WidgetTree->RootWidget) { return; }

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ReticleRoot"));
    WidgetTree->RootWidget = Root;

    AddBar(Root, FVector2D(0.0f, 0.0f), FVector2D(5.0f, 5.0f), 1.0f);
    AddBar(Root, FVector2D(-13.0f, 0.0f), FVector2D(8.0f, 2.0f), 0.92f);
    AddBar(Root, FVector2D(13.0f, 0.0f), FVector2D(8.0f, 2.0f), 0.92f);
    AddBar(Root, FVector2D(0.0f, -13.0f), FVector2D(2.0f, 8.0f), 0.92f);
    AddBar(Root, FVector2D(0.0f, 13.0f), FVector2D(2.0f, 8.0f), 0.92f);
}

UBorder* UNWActionReticleWidget::AddBar(UCanvasPanel* Root, const FVector2D& Position, const FVector2D& Size, float Opacity)
{
    if (!Root || !WidgetTree) { return nullptr; }

    UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Bar->SetBrushColor(FLinearColor(0.92f, 0.96f, 1.0f, FMath::Clamp(Opacity, 0.0f, 1.0f)));

    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Bar);
    Slot->SetAnchors(FAnchors(0.5f, 0.5f));
    Slot->SetAlignment(FVector2D(0.5f, 0.5f));
    Slot->SetPosition(Position);
    Slot->SetSize(Size);
    return Bar;
}
