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

namespace
{
    float SumAffix(const ANWCharacter* Character, ENWAffixType Type)
    {
        if (!Character) { return 0.0f; }
        float Total = 0.0f;
        auto Accumulate = [&Total, Type](const TArray<FNWGeneratedItem>& Items)
        {
            for (const FNWGeneratedItem& Item : Items)
            {
                for (const FNWItemAffix& Affix : Item.Affixes)
                {
                    if (Affix.Type == Type) { Total += Affix.Magnitude; }
                }
            }
        };
        Accumulate(Character->GetEquippedItems());
        Accumulate(Character->GetEquippedWeaponItems());
        return Total;
    }
}

void UNWCombatHUDWidget::SetObservedCharacter(ANWCharacter* Character)
{
    ObservedCharacter = Character;
    BuildHUD();
    RefreshHUD();
}

void UNWCombatHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildHUD();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetRenderOpacity(1.0f);
    RefreshHUD();
    UE_LOG(LogTemp, Warning, TEXT("[HUD-V4] HUD completo construido: vida/stamina/poder/loadout/QER."));
}

void UNWCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator >= 0.08f)
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

    // Painel principal sempre dentro da safe area superior esquerda. O V3 usava
    // AddToViewport e em alguns layouts Linux o painel nao aparecia no player layer.
    UBorder* VitalsBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VitalsBorder"));
    VitalsBorder->SetPadding(FMargin(15.0f, 12.0f));
    VitalsBorder->SetBrushColor(FLinearColor(0.008f, 0.012f, 0.020f, 0.91f));
    UCanvasPanelSlot* VitalsSlot = Root->AddChildToCanvas(VitalsBorder);
    VitalsSlot->SetAnchors(FAnchors(0.0f, 0.0f));
    VitalsSlot->SetPosition(FVector2D(22.0f, 22.0f));
    VitalsSlot->SetSize(FVector2D(610.0f, 312.0f));

    UVerticalBox* VitalsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    VitalsBorder->SetContent(VitalsBox);

    WeaponText = MakeText(TEXT("ARMA ATIVA"), 18);
    WeaponText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.78f, 0.38f, 1.0f)));
    VitalsBox->AddChildToVerticalBox(WeaponText);

    HealthText = MakeText(TEXT("VIDA"), 15);
    VitalsBox->AddChildToVerticalBox(HealthText);
    HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerHealthBar"));
    HealthBar->SetPercent(1.0f);
    HealthBar->SetFillColorAndOpacity(FLinearColor(0.82f, 0.055f, 0.04f, 1.0f));
    UVerticalBoxSlot* HealthSlot = VitalsBox->AddChildToVerticalBox(HealthBar);
    HealthSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 6.0f));

    StaminaText = MakeText(TEXT("STAMINA"), 15);
    VitalsBox->AddChildToVerticalBox(StaminaText);
    StaminaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerStaminaBar"));
    StaminaBar->SetPercent(1.0f);
    StaminaBar->SetFillColorAndOpacity(FLinearColor(0.92f, 0.67f, 0.08f, 1.0f));
    UVerticalBoxSlot* StaminaSlot = VitalsBox->AddChildToVerticalBox(StaminaBar);
    StaminaSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 7.0f));

    StatsText = MakeText(TEXT("PODER | ARMADURA | PRECISAO | HASTE"), 13);
    StatsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.86f, 1.0f, 1.0f)));
    VitalsBox->AddChildToVerticalBox(StatsText);

    StateText = MakeText(TEXT("COMBATE PRONTO"), 12);
    StateText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.86f, 0.90f, 1.0f)));
    VitalsBox->AddChildToVerticalBox(StateText);

    // Barra de habilidades action-RPG no centro inferior.
    UBorder* AbilityBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AbilityBorder"));
    AbilityBorder->SetPadding(FMargin(10.0f));
    AbilityBorder->SetBrushColor(FLinearColor(0.006f, 0.010f, 0.017f, 0.92f));
    UCanvasPanelSlot* AbilityCanvasSlot = Root->AddChildToCanvas(AbilityBorder);
    AbilityCanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
    AbilityCanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
    AbilityCanvasSlot->SetPosition(FVector2D(0.0f, -24.0f));
    AbilityCanvasSlot->SetSize(FVector2D(840.0f, 116.0f));

    UHorizontalBox* AbilityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    AbilityBorder->SetContent(AbilityRow);
    AbilityTexts.Reset();

    const TCHAR* Keys[3] = { TEXT("Q"), TEXT("E"), TEXT("R") };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Card->SetPadding(FMargin(12.0f, 8.0f));
        Card->SetBrushColor(FLinearColor(0.045f, 0.060f, 0.085f, 0.98f));
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
    LootSlot->SetPosition(FVector2D(0.0f, -145.0f));
    LootSlot->SetSize(FVector2D(980.0f, 54.0f));

    CrosshairText = MakeText(TEXT("+"), 24);
    CrosshairText->SetJustification(ETextJustify::Center);
    CrosshairText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.96f, 0.94f, 0.82f)));
    UCanvasPanelSlot* CrosshairSlot = Root->AddChildToCanvas(CrosshairText);
    CrosshairSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CrosshairSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CrosshairSlot->SetPosition(FVector2D(0.0f, -8.0f));
    CrosshairSlot->SetSize(FVector2D(42.0f, 42.0f));

    HelpText = MakeText(TEXT("LMB atacar | RMB defesa/parry | ALT esquiva | Q/E/R skills | 1/2 armas | F troca | G loot | I bag"), 11);
    HelpText->SetJustification(ETextJustify::Right);
    HelpText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.77f, 0.84f, 0.92f)));
    UCanvasPanelSlot* HelpSlot = Root->AddChildToCanvas(HelpText);
    HelpSlot->SetAnchors(FAnchors(1.0f, 1.0f));
    HelpSlot->SetAlignment(FVector2D(1.0f, 1.0f));
    HelpSlot->SetPosition(FVector2D(-20.0f, -16.0f));
    HelpSlot->SetSize(FVector2D(760.0f, 28.0f));

    InventoryPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryPanel"));
    InventoryPanel->SetPadding(FMargin(22.0f));
    InventoryPanel->SetBrushColor(FLinearColor(0.005f, 0.009f, 0.015f, 0.97f));
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
    if (!Character || !HealthBar || !StaminaBar || !WeaponText || !StatsText) { return; }

    const float MaxHealth = FMath::Max(1.0f, Character->GetMaxHealth());
    const float MaxStamina = FMath::Max(1.0f, Character->GetMaxStamina());
    HealthBar->SetPercent(FMath::Clamp(Character->GetHealth() / MaxHealth, 0.0f, 1.0f));
    StaminaBar->SetPercent(FMath::Clamp(Character->GetStamina() / MaxStamina, 0.0f, 1.0f));
    HealthText->SetText(FText::FromString(FString::Printf(TEXT("VIDA     %.0f / %.0f"), Character->GetHealth(), MaxHealth)));
    StaminaText->SetText(FText::FromString(FString::Printf(TEXT("STAMINA  %.0f / %.0f"), Character->GetStamina(), MaxStamina)));

    const FNWWeaponDefinition Primary = NWCombat::GetWeaponDefinition(Character->GetPrimaryWeapon());
    const FNWWeaponDefinition Secondary = NWCombat::GetWeaponDefinition(Character->GetSecondaryWeapon());
    const FNWWeaponDefinition Active = NWCombat::GetWeaponDefinition(Character->GetActiveWeapon());

    FString WeaponInfo = FString::Printf(TEXT("ATIVA: %s\n[1] %s   |   [2] %s"), *Active.Name, *Primary.Name, *Secondary.Name);
    if (Character->GetActiveWeapon() == ENWWeaponType::Bow)
    {
        WeaponInfo += FString::Printf(TEXT("   |   [V] %s"), *Character->GetArrowElementLabel());
    }
    WeaponInfo += TEXT("\nPASSIVAS: ");
    for (int32 Index = 0; Index < Active.Passives.Num(); ++Index)
    {
        if (Index > 0) { WeaponInfo += TEXT(" | "); }
        WeaponInfo += Active.Passives[Index].Name;
    }
    WeaponText->SetText(FText::FromString(WeaponInfo));

    const float Power = SumAffix(Character, ENWAffixType::Power);
    const float Armor = SumAffix(Character, ENWAffixType::Armor);
    const float Precision = SumAffix(Character, ENWAffixType::Precision);
    const float Haste = SumAffix(Character, ENWAffixType::Haste);
    const float LifeSteal = SumAffix(Character, ENWAffixType::LifeSteal);
    StatsText->SetText(FText::FromString(FString::Printf(
        TEXT("PODER %.1f   |   ARMADURA %.1f   |   PRECISAO %.1f   |   HASTE %.1f   |   ROUBO VIDA %.1f"),
        Power, Armor, Precision, Haste, LifeSteal)));

    FString State = Character->GetCombatStateLabel();
    if (Character->IsBrutalTransformationActive())
    {
        State += FString::Printf(TEXT(" | BRUTAL %.0fs"), Character->GetBrutalTransformationRemaining());
    }
    State += FString::Printf(TEXT("\n[T] %s  [Y] VIAJAR  [I] BAG"), *Character->GetSelectedFastTravelLabel());
    StateText->SetText(FText::FromString(State));

    const TCHAR* Keys[3] = { TEXT("Q"), TEXT("E"), TEXT("R") };
    for (int32 Index = 0; Index < AbilityTexts.Num() && Index < Active.Abilities.Num(); ++Index)
    {
        const float Remaining = Character->GetAbilityCooldownRemaining(Index);
        const FString Status = Remaining <= 0.01f ? TEXT("PRONTO") : FString::Printf(TEXT("%.1fs"), Remaining);
        AbilityTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("[%s] %s\n%s"), Keys[Index], *Active.Abilities[Index].Name, *Status)));
        AbilityTexts[Index]->SetColorAndOpacity(FSlateColor(
            Remaining <= 0.01f ? FLinearColor(0.96f, 0.96f, 0.96f, 1.0f) : FLinearColor(0.43f, 0.48f, 0.56f, 1.0f)));
    }

    const FString NearbyLoot = Character->GetNearbyLootLabel();
    if (LootPromptText)
    {
        LootPromptText->SetText(FText::FromString(NearbyLoot.IsEmpty() ? TEXT("") : FString::Printf(TEXT("[G] COLETAR  %s"), *NearbyLoot)));
    }

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
    UE_LOG(LogTemp, Display, TEXT("[HUD] Bag %s"), bInventoryVisible ? TEXT("aberta") : TEXT("fechada"));
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
