#include "NWEnemyHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "NWEnemy.h"

void UNWEnemyHealthBarWidget::SetObservedEnemy(ANWEnemy* Enemy)
{
    ObservedEnemy = Enemy;
    BuildWidgetTreeIfNeeded();
    RefreshFromEnemy();
}

void UNWEnemyHealthBarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildWidgetTreeIfNeeded();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    RefreshFromEnemy();
}

void UNWEnemyHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator >= 0.08f)
    {
        RefreshAccumulator = 0.0f;
        RefreshFromEnemy();
    }
}

void UNWEnemyHealthBarWidget::BuildWidgetTreeIfNeeded()
{
    if (!WidgetTree || WidgetTree->RootWidget) { return; }

    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EnemyHealthBackground"));
    Background->SetPadding(FMargin(7.0f, 4.0f));
    Background->SetBrushColor(FLinearColor(0.008f, 0.010f, 0.014f, 0.82f));
    WidgetTree->RootWidget = Background;

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EnemyHealthStack"));
    Background->SetContent(Stack);

    NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnemyName"));
    NameText->SetJustification(ETextJustify::Center);
    NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.98f, 1.0f)));
    FSlateFontInfo NameFont = NameText->GetFont();
    NameFont.Size = 13;
    NameText->SetFont(NameFont);
    Stack->AddChildToVerticalBox(NameText);

    HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("EnemyHealthProgress"));
    HealthBar->SetPercent(1.0f);
    HealthBar->SetFillColorAndOpacity(FLinearColor(0.84f, 0.055f, 0.045f, 1.0f));
    UVerticalBoxSlot* BarSlot = Stack->AddChildToVerticalBox(HealthBar);
    BarSlot->SetPadding(FMargin(0.0f, 2.0f));

    HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnemyHealthText"));
    HealthText->SetJustification(ETextJustify::Center);
    HealthText->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.90f, 0.94f, 0.95f)));
    FSlateFontInfo HealthFont = HealthText->GetFont();
    HealthFont.Size = 10;
    HealthText->SetFont(HealthFont);
    Stack->AddChildToVerticalBox(HealthText);
}

void UNWEnemyHealthBarWidget::RefreshFromEnemy()
{
    ANWEnemy* Enemy = ObservedEnemy.Get();
    if (!Enemy || !HealthBar || !NameText || !HealthText)
    {
        if (!Enemy) { SetVisibility(ESlateVisibility::Collapsed); }
        return;
    }

    const float MaxHealth = FMath::Max(1.0f, Enemy->GetMaxHealth());
    const float Health = FMath::Clamp(Enemy->GetHealth(), 0.0f, MaxHealth);
    const float Ratio = Health / MaxHealth;

    HealthBar->SetPercent(Ratio);
    NameText->SetText(FText::FromString(Enemy->GetDisplayName()));
    HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Health, MaxHealth)));

    if (Enemy->IsWorldBoss())
    {
        HealthBar->SetFillColorAndOpacity(FLinearColor(0.72f, 0.12f, 0.92f, 1.0f));
        NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.78f, 1.0f, 1.0f)));
    }
    else
    {
        HealthBar->SetFillColorAndOpacity(FLinearColor(0.84f, 0.055f, 0.045f, 1.0f));
    }
}
