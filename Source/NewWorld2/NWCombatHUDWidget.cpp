#include "NWCombatHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"

void UNWCombatHUDWidget::SetObservedCharacter(ANWCharacter* Character)
{
    ObservedCharacter = Character;
    RefreshHUD();
}

void UNWCombatHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildHUD();
    RefreshHUD();
}

void UNWCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator >= 0.05f)
    {
        RefreshAccumulator = 0.0f;
        RefreshHUD();
    }
}

UTextBlock* UNWCombatHUDWidget::MakeText(const FString& InitialText, int32 FontSize)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Text->SetText(FText::FromString(InitialText));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = FontSize;
    Text->SetFont(Font);
    return Text;
}

void UNWCombatHUDWidget::BuildHUD()
{
    if (!WidgetTree || WidgetTree->RootWidget) { return; }

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
    WidgetTree->RootWidget = Root;

    UBorder* VitalsBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VitalsBorder"));
    VitalsBorder->SetPadding(FMargin(14.0f));
    VitalsBorder->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.025f, 0.84f));
    UCanvasPanelSlot* VitalsSlot = Root->AddChildToCanvas(VitalsBorder);
    VitalsSlot->SetAnchors(FAnchors(0.0f, 0.0f));
    VitalsSlot->SetPosition(FVector2D(28.0f, 28.0f));
    VitalsSlot->SetSize(FVector2D(580.0f, 305.0f));

    UVerticalBox* VitalsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    VitalsBorder->SetContent(VitalsBox);

    WeaponText = MakeText(TEXT("ARMA"), 18);
    VitalsBox->AddChildToVerticalBox(WeaponText);

    HealthText = MakeText(TEXT("Vida"), 15);
    VitalsBox->AddChildToVerticalBox(HealthText);
    HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    HealthBar->SetPercent(1.0f);
    HealthBar->SetFillColorAndOpacity(FLinearColor(0.80f, 0.08f, 0.06f, 1.0f));
    VitalsBox->AddChildToVerticalBox(HealthBar);

    StaminaText = MakeText(TEXT("Stamina"), 15);
    VitalsBox->AddChildToVerticalBox(StaminaText);
    StaminaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    StaminaBar->SetPercent(1.0f);
    StaminaBar->SetFillColorAndOpacity(FLinearColor(0.93f, 0.72f, 0.12f, 1.0f));
    VitalsBox->AddChildToVerticalBox(StaminaBar);

    StateText = MakeText(TEXT("NORMAL"), 14);
    VitalsBox->AddChildToVerticalBox(StateText);

    UBorder* AbilityBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AbilityBorder"));
    AbilityBorder->SetPadding(FMargin(12.0f));
    AbilityBorder->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.02f, 0.88f));
    UCanvasPanelSlot* AbilityCanvasSlot = Root->AddChildToCanvas(AbilityBorder);
    AbilityCanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
    AbilityCanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
    AbilityCanvasSlot->SetPosition(FVector2D(0.0f, -32.0f));
    AbilityCanvasSlot->SetSize(FVector2D(760.0f, 108.0f));

    UHorizontalBox* AbilityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    AbilityBorder->SetContent(AbilityRow);
    AbilityTexts.Reset();

    const TCHAR* Keys[3] = { TEXT("Q"), TEXT("E"), TEXT("C") };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Card->SetPadding(FMargin(12.0f, 8.0f));
        Card->SetBrushColor(FLinearColor(0.06f, 0.075f, 0.09f, 0.96f));
        UHorizontalBoxSlot* CardSlot = AbilityRow->AddChildToHorizontalBox(Card);
        CardSlot->SetPadding(FMargin(5.0f));
        CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        UTextBlock* Text = MakeText(FString::Printf(TEXT("[%s] Habilidade"), Keys[Index]), 15);
        Text->SetJustification(ETextJustify::Center);
        Card->SetContent(Text);
        AbilityTexts.Add(Text);
    }

    LootPromptText = MakeText(TEXT(""), 16);
    LootPromptText->SetJustification(ETextJustify::Center);
    UCanvasPanelSlot* LootSlot = Root->AddChildToCanvas(LootPromptText);
    LootSlot->SetAnchors(FAnchors(0.5f, 1.0f));
    LootSlot->SetAlignment(FVector2D(0.5f, 1.0f));
    LootSlot->SetPosition(FVector2D(0.0f, -152.0f));
    LootSlot->SetSize(FVector2D(980.0f, 54.0f));

    InventoryPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryPanel"));
    InventoryPanel->SetPadding(FMargin(22.0f));
    InventoryPanel->SetBrushColor(FLinearColor(0.008f, 0.012f, 0.018f, 0.96f));
    InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* InventorySlot = Root->AddChildToCanvas(InventoryPanel);
    InventorySlot->SetAnchors(FAnchors(0.5f, 0.5f));
    InventorySlot->SetAlignment(FVector2D(0.5f, 0.5f));
    InventorySlot->SetSize(FVector2D(900.0f, 700.0f));

    UScrollBox* InventoryScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("InventoryScroll"));
    InventoryScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    InventoryScroll->SetAnimateWheelScrolling(true);
    InventoryPanel->SetContent(InventoryScroll);

    InventoryText = MakeText(TEXT("BAG"), 14);
    InventoryScroll->AddChild(InventoryText);
}

void UNWCombatHUDWidget::RefreshHUD()
{
    ANWCharacter* Character = ObservedCharacter.Get();
    if (!Character || !HealthBar || !WeaponText) { return; }

    const float MaxHealth = FMath::Max(1.0f, Character->GetMaxHealth());
    const float MaxStamina = FMath::Max(1.0f, Character->GetMaxStamina());
    HealthBar->SetPercent(Character->GetHealth() / MaxHealth);
    StaminaBar->SetPercent(Character->GetStamina() / MaxStamina);
    HealthText->SetText(FText::FromString(FString::Printf(TEXT("VIDA  %.0f / %.0f"), Character->GetHealth(), MaxHealth)));
    StaminaText->SetText(FText::FromString(FString::Printf(TEXT("STAMINA  %.0f / %.0f"), Character->GetStamina(), MaxStamina)));

    const FNWWeaponDefinition Primary = NWCombat::GetWeaponDefinition(Character->GetPrimaryWeapon());
    const FNWWeaponDefinition Secondary = NWCombat::GetWeaponDefinition(Character->GetSecondaryWeapon());
    FString WeaponInfo = FString::Printf(TEXT("[1] %s   |   [2] %s\nATIVA: %s"), *Primary.Name, *Secondary.Name, *Character->GetActiveWeaponName());
    if (Character->GetActiveWeapon() == ENWWeaponType::Bow)
    {
        WeaponInfo += FString::Printf(TEXT("   |   [V] FLECHA: %s"), *Character->GetArrowElementLabel());
    }

    WeaponInfo += TEXT("\nPASSIVAS SLOT 1: ");
    for (int32 Index = 0; Index < Primary.Passives.Num(); ++Index)
    {
        if (Index > 0) { WeaponInfo += TEXT(" | "); }
        WeaponInfo += Primary.Passives[Index].Name;
    }
    WeaponInfo += TEXT("\nPASSIVAS SLOT 2: ");
    for (int32 Index = 0; Index < Secondary.Passives.Num(); ++Index)
    {
        if (Index > 0) { WeaponInfo += TEXT(" | "); }
        WeaponInfo += Secondary.Passives[Index].Name;
    }
    WeaponText->SetText(FText::FromString(WeaponInfo));

    FString State = Character->GetCombatStateLabel();
    if (Character->IsBrutalTransformationActive())
    {
        State += TEXT(" | METAMORFOSE ATIVA");
    }
    State += FString::Printf(TEXT("\n[T] Destino: %s   [Y] TELEPORTAR"), *Character->GetSelectedFastTravelLabel());
    StateText->SetText(FText::FromString(State));

    const FNWWeaponDefinition Weapon = NWCombat::GetWeaponDefinition(Character->GetActiveWeapon());
    const TCHAR* Keys[3] = { TEXT("Q"), TEXT("E"), TEXT("C") };
    for (int32 Index = 0; Index < AbilityTexts.Num() && Index < Weapon.Abilities.Num(); ++Index)
    {
        const float Remaining = Character->GetAbilityCooldownRemaining(Index);
        const FString Status = Remaining <= 0.01f ? TEXT("PRONTO") : FString::Printf(TEXT("%.1fs"), Remaining);
        AbilityTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("[%s] %s\n%s"), Keys[Index], *Weapon.Abilities[Index].Name, *Status)));
        AbilityTexts[Index]->SetColorAndOpacity(FSlateColor(Remaining <= 0.01f ? FLinearColor::White : FLinearColor(0.48f, 0.52f, 0.58f, 1.0f)));
    }

    const FString NearbyLoot = Character->GetNearbyLootLabel();
    LootPromptText->SetText(FText::FromString(NearbyLoot.IsEmpty() ? TEXT("") : FString::Printf(TEXT("[G] Coletar  %s"), *NearbyLoot)));

    if (bInventoryVisible) { RefreshInventory(); }
}

void UNWCombatHUDWidget::ToggleInventory()
{
    bInventoryVisible = !bInventoryVisible;
    if (InventoryPanel)
    {
        InventoryPanel->SetVisibility(bInventoryVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (bInventoryVisible) { RefreshInventory(); }
}

void UNWCombatHUDWidget::RefreshInventory()
{
    ANWCharacter* Character = ObservedCharacter.Get();
    if (!Character || !InventoryText) { return; }

    FString Text = TEXT("BAG SEM LIMITE  [I fechar]   [Setas selecionar]   [Enter equipar/usar]   [Mouse wheel rolar]\n");
    Text += FString::Printf(TEXT("TOTAL: %d itens\n\nARMADURAS EQUIPADAS\n"), Character->GetInventoryItems().Num());
    for (const FNWGeneratedItem& Item : Character->GetEquippedItems())
    {
        Text += FString::Printf(TEXT("  %s | %s | score %.1f\n"), *NWCombat::EquipmentSlotToString(Item.Slot), *Item.Name, NWCombat::GetItemScore(Item));
    }

    Text += TEXT("\nARMAS EQUIPADAS\n");
    for (const FNWGeneratedItem& Item : Character->GetEquippedWeaponItems())
    {
        if (!Item.Name.IsEmpty())
        {
            Text += FString::Printf(TEXT("  %s | %s | score %.1f\n"), *NWCombat::WeaponTypeToString(Item.WeaponType), *Item.Name, NWCombat::GetItemScore(Item));
        }
    }

    Text += TEXT("\nCONTEUDO DA BAG - ORGANIZADO AUTOMATICAMENTE\n");
    const TArray<FNWGeneratedItem> Inventory = Character->GetInventoryItems();
    const int32 Selected = Character->GetSelectedInventoryIndex();
    if (Inventory.IsEmpty())
    {
        Text += TEXT("  (vazia - derrote inimigos, bosses e colete drops)\n");
    }
    else
    {
        FString LastCategory;
        for (int32 Index = 0; Index < Inventory.Num(); ++Index)
        {
            const FNWGeneratedItem& Item = Inventory[Index];
            FString Category;
            if (Item.Kind == ENWItemKind::Weapon)
            {
                Category = FString::Printf(TEXT("ARMAS > %s"), *NWCombat::WeaponTypeToString(Item.WeaponType));
            }
            else if (Item.Kind == ENWItemKind::Armor)
            {
                Category = FString::Printf(TEXT("ARMADURAS > %s > %s"), *NWCombat::ArmorWeightToString(Item.ArmorWeight), *NWCombat::EquipmentSlotToString(Item.Slot));
            }
            else
            {
                Category = TEXT("CONSUMIVEIS LENDARIOS");
            }

            if (Category != LastCategory)
            {
                Text += FString::Printf(TEXT("\n=== %s ===\n"), *Category);
                LastCategory = Category;
            }

            Text += FString::Printf(TEXT("%s %03d. %s | score %.1f\n"), Index == Selected ? TEXT(">") : TEXT(" "), Index + 1, *Item.Name, NWCombat::GetItemScore(Item));
            for (const FNWItemAffix& Affix : Item.Affixes)
            {
                Text += FString::Printf(TEXT("       + %s %.1f\n"), *NWCombat::AffixToString(Affix.Type), Affix.Magnitude);
            }
        }
    }

    InventoryText->SetText(FText::FromString(Text));
}
