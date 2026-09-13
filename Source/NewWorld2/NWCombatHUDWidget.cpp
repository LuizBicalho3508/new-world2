#include "NWCombatHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"
#include "NWPremiumV6AbilityLibrary.h"

namespace
{
    float SumAffix(const ANWCharacter* Character, ENWAffixType Type)
    {
        if (!Character) return 0.0f;
        float Total = 0.0f;
        auto Accumulate = [&Total, Type](const TArray<FNWGeneratedItem>& Items)
        {
            for (const FNWGeneratedItem& Item : Items)
                for (const FNWItemAffix& Affix : Item.Affixes)
                    if (Affix.Type == Type) Total += Affix.Magnitude;
        };
        Accumulate(Character->GetEquippedItems());
        Accumulate(Character->GetEquippedWeaponItems());
        return Total;
    }

    FString ItemTypeLabel(const FNWGeneratedItem& Item)
    {
        if (Item.Kind == ENWItemKind::Weapon) return NWCombat::WeaponTypeToString(Item.WeaponType);
        if (Item.Kind == ENWItemKind::Armor)
            return FString::Printf(TEXT("%s · %s"), *NWCombat::EquipmentSlotToString(Item.Slot), *NWCombat::ArmorWeightToString(Item.ArmorWeight));
        return TEXT("Consumivel");
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
    UE_LOG(LogTemp, Warning, TEXT("[HUD-V6] HUD action-RPG construido: vitals compactos + loadout + QER tematico + bag comparativa."));
}

void UNWCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator >= 0.10f)
    {
        RefreshAccumulator = 0.0f;
        RefreshHUD();
    }
}

UTextBlock* UNWCombatHUDWidget::MakeText(const FString& InitialText, int32 FontSize)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Text->SetText(FText::FromString(InitialText));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.95f, 0.97f, 1.0f)));
    Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
    Text->SetAutoWrapText(true);
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = FontSize;
    Text->SetFont(Font);
    return Text;
}

UBorder* UNWCombatHUDWidget::MakePanel(const FLinearColor& Color, const FMargin& Padding)
{
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Panel->SetBrushColor(Color);
    Panel->SetPadding(Padding);
    return Panel;
}

void UNWCombatHUDWidget::BuildHUD()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRootV6"));
    WidgetTree->RootWidget = Root;

    UBorder* VitalsPanel = MakePanel(FLinearColor(0.015f, 0.020f, 0.026f, 0.88f), FMargin(14.0f, 10.0f));
    UCanvasPanelSlot* VitalsSlot = Root->AddChildToCanvas(VitalsPanel);
    VitalsSlot->SetAnchors(FAnchors(0.0f, 0.0f));
    VitalsSlot->SetPosition(FVector2D(18.0f, 18.0f));
    VitalsSlot->SetSize(FVector2D(455.0f, 190.0f));

    UVerticalBox* Vitals = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    VitalsPanel->SetContent(Vitals);

    WeaponText = MakeText(TEXT("LOADOUT"), 17);
    WeaponText->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.76f, 0.35f, 1.0f)));
    Vitals->AddChildToVerticalBox(WeaponText);

    HealthText = MakeText(TEXT("VIDA"), 13);
    Vitals->AddChildToVerticalBox(HealthText);
    HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerHealthBarV6"));
    HealthBar->SetFillColorAndOpacity(FLinearColor(0.83f, 0.075f, 0.055f, 1.0f));
    UVerticalBoxSlot* HealthSlot = Vitals->AddChildToVerticalBox(HealthBar);
    HealthSlot->SetPadding(FMargin(0, 0, 0, 5));

    StaminaText = MakeText(TEXT("STAMINA"), 13);
    Vitals->AddChildToVerticalBox(StaminaText);
    StaminaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerStaminaBarV6"));
    StaminaBar->SetFillColorAndOpacity(FLinearColor(0.90f, 0.67f, 0.08f, 1.0f));
    UVerticalBoxSlot* StaminaSlot = Vitals->AddChildToVerticalBox(StaminaBar);
    StaminaSlot->SetPadding(FMargin(0, 0, 0, 5));

    StatsText = MakeText(TEXT("PODER · ARMADURA · PRECISAO · HASTE"), 11);
    StatsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.66f, 0.79f, 0.92f, 1.0f)));
    Vitals->AddChildToVerticalBox(StatsText);
    StateText = MakeText(TEXT("COMBATE PRONTO"), 11);
    StateText->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.73f, 0.77f, 1.0f)));
    Vitals->AddChildToVerticalBox(StateText);

    UBorder* AbilityPanel = MakePanel(FLinearColor(0.010f, 0.014f, 0.020f, 0.90f), FMargin(8.0f));
    UCanvasPanelSlot* AbilitySlot = Root->AddChildToCanvas(AbilityPanel);
    AbilitySlot->SetAnchors(FAnchors(0.5f, 1.0f));
    AbilitySlot->SetAlignment(FVector2D(0.5f, 1.0f));
    AbilitySlot->SetPosition(FVector2D(0.0f, -18.0f));
    AbilitySlot->SetSize(FVector2D(780.0f, 132.0f));

    UHorizontalBox* AbilityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    AbilityPanel->SetContent(AbilityRow);
    AbilityTexts.Reset(); AbilityDescriptionTexts.Reset(); AbilityCards.Reset();
    const TCHAR* Keys[3] = { TEXT("Q"), TEXT("E"), TEXT("R") };
    for (int32 I = 0; I < 3; ++I)
    {
        UBorder* Card = MakePanel(FLinearColor(0.035f, 0.045f, 0.060f, 0.98f), FMargin(10.0f, 7.0f));
        UHorizontalBoxSlot* CardSlot = AbilityRow->AddChildToHorizontalBox(Card);
        CardSlot->SetPadding(FMargin(4.0f));
        CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        UVerticalBox* CardBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Card->SetContent(CardBody);
        UTextBlock* Title = MakeText(FString::Printf(TEXT("[%s] HABILIDADE"), Keys[I]), 14);
        Title->SetJustification(ETextJustify::Center);
        CardBody->AddChildToVerticalBox(Title);
        UTextBlock* Description = MakeText(TEXT(""), 10);
        Description->SetJustification(ETextJustify::Center);
        Description->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.75f, 0.78f, 1.0f)));
        CardBody->AddChildToVerticalBox(Description);
        AbilityCards.Add(Card); AbilityTexts.Add(Title); AbilityDescriptionTexts.Add(Description);
    }

    LootPromptText = MakeText(TEXT(""), 15);
    LootPromptText->SetJustification(ETextJustify::Center);
    LootPromptText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.30f, 1.0f)));
    UCanvasPanelSlot* LootSlot = Root->AddChildToCanvas(LootPromptText);
    LootSlot->SetAnchors(FAnchors(0.5f, 1.0f));
    LootSlot->SetAlignment(FVector2D(0.5f, 1.0f));
    LootSlot->SetPosition(FVector2D(0.0f, -158.0f));
    LootSlot->SetSize(FVector2D(900.0f, 38.0f));

    CrosshairText = MakeText(TEXT("+"), 20);
    CrosshairText->SetJustification(ETextJustify::Center);
    CrosshairText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.90f, 0.58f, 0.92f)));
    UCanvasPanelSlot* CrosshairSlot = Root->AddChildToCanvas(CrosshairText);
    CrosshairSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CrosshairSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CrosshairSlot->SetPosition(FVector2D(0, -6));
    CrosshairSlot->SetSize(FVector2D(32, 32));

    HelpText = MakeText(TEXT("LMB atacar · RMB bloquear/parry · ALT esquiva · Q/E/R skills · 1/2 armas · F troca · G loot · I inventario"), 10);
    HelpText->SetJustification(ETextJustify::Right);
    HelpText->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.66f, 0.71f, 0.88f)));
    UCanvasPanelSlot* HelpSlot = Root->AddChildToCanvas(HelpText);
    HelpSlot->SetAnchors(FAnchors(1, 1)); HelpSlot->SetAlignment(FVector2D(1, 1));
    HelpSlot->SetPosition(FVector2D(-14, -5)); HelpSlot->SetSize(FVector2D(720, 22));

    InventoryPanel = MakePanel(FLinearColor(0.008f, 0.012f, 0.017f, 0.985f), FMargin(18.0f));
    InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* InvSlot = Root->AddChildToCanvas(InventoryPanel);
    InvSlot->SetAnchors(FAnchors(0.5f, 0.5f)); InvSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    InvSlot->SetSize(FVector2D(1080.0f, 650.0f));

    UVerticalBox* InvRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    InventoryPanel->SetContent(InvRoot);
    InventoryTitleText = MakeText(TEXT("INVENTARIO"), 24);
    InventoryTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.78f, 0.37f, 1.0f)));
    InvRoot->AddChildToVerticalBox(InventoryTitleText);
    InventoryHintText = MakeText(TEXT("↑ ↓ selecionar · ENTER equipar/usar · I fechar"), 11);
    InventoryHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.66f, 0.73f, 1.0f)));
    UVerticalBoxSlot* HintSlot = InvRoot->AddChildToVerticalBox(InventoryHintText);
    HintSlot->SetPadding(FMargin(0, 0, 0, 9));

    UHorizontalBox* InvBody = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBoxSlot* BodySlot = InvRoot->AddChildToVerticalBox(InvBody);
    BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    USizeBox* EquippedSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    EquippedSize->SetWidthOverride(310.0f);
    UHorizontalBoxSlot* LeftSlot = InvBody->AddChildToHorizontalBox(EquippedSize);
    LeftSlot->SetPadding(FMargin(0, 0, 12, 0));
    UBorder* EquippedPanel = MakePanel(FLinearColor(0.020f, 0.028f, 0.038f, 1.0f), FMargin(14.0f));
    EquippedSize->SetContent(EquippedPanel);
    EquippedText = MakeText(TEXT("EQUIPADO"), 13);
    EquippedPanel->SetContent(EquippedText);

    UBorder* BagPanel = MakePanel(FLinearColor(0.014f, 0.020f, 0.028f, 1.0f), FMargin(12.0f));
    UHorizontalBoxSlot* RightSlot = InvBody->AddChildToHorizontalBox(BagPanel);
    RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    InventoryScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("InventoryScrollV6"));
    InventoryScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    InventoryScroll->SetAnimateWheelScrolling(true);
    BagPanel->SetContent(InventoryScroll);
    InventoryItemsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryItemsV6"));
    InventoryScroll->AddChild(InventoryItemsBox);
}

void UNWCombatHUDWidget::RefreshHUD()
{
    ANWCharacter* Character = ObservedCharacter.Get();
    if (!Character || !HealthBar || !StaminaBar || !WeaponText || !StatsText) return;

    const float MaxHealth = FMath::Max(1.0f, Character->GetMaxHealth());
    const float MaxStamina = FMath::Max(1.0f, Character->GetMaxStamina());
    HealthBar->SetPercent(FMath::Clamp(Character->GetHealth() / MaxHealth, 0.0f, 1.0f));
    StaminaBar->SetPercent(FMath::Clamp(Character->GetStamina() / MaxStamina, 0.0f, 1.0f));
    HealthText->SetText(FText::FromString(FString::Printf(TEXT("VIDA  %.0f / %.0f"), Character->GetHealth(), MaxHealth)));
    StaminaText->SetText(FText::FromString(FString::Printf(TEXT("STAMINA  %.0f / %.0f"), Character->GetStamina(), MaxStamina)));

    const FNWWeaponDefinition Active = NWCombat::GetWeaponDefinition(Character->GetActiveWeapon());
    const TArray<FNWGeneratedItem> WeaponItems = Character->GetEquippedWeaponItems();
    const FString Slot1 = WeaponItems.IsValidIndex(0) && !WeaponItems[0].Name.IsEmpty() ? WeaponItems[0].Name : TEXT("SEM ARMA");
    const FString Slot2 = WeaponItems.IsValidIndex(1) && !WeaponItems[1].Name.IsEmpty() ? WeaponItems[1].Name : TEXT("SEM ARMA");
    WeaponText->SetText(FText::FromString(FString::Printf(TEXT("%s\n[1] %s   ·   [2] %s"), *Active.Name, *Slot1, *Slot2)));

    StatsText->SetText(FText::FromString(FString::Printf(TEXT("PODER %.1f · ARM %.1f · PREC %.1f · HASTE %.1f · ROUBO %.1f"),
        SumAffix(Character, ENWAffixType::Power), SumAffix(Character, ENWAffixType::Armor), SumAffix(Character, ENWAffixType::Precision),
        SumAffix(Character, ENWAffixType::Haste), SumAffix(Character, ENWAffixType::LifeSteal))));

    FString State = Character->GetCombatStateLabel();
    if (Character->IsBrutalTransformationActive()) State += FString::Printf(TEXT(" · BRUTAL %.0fs"), Character->GetBrutalTransformationRemaining());
    State += FString::Printf(TEXT("\n[T] %s · [Y] viajar · [I] inventario"), *Character->GetSelectedFastTravelLabel());
    StateText->SetText(FText::FromString(State));

    const TCHAR* Keys[3] = { TEXT("Q"), TEXT("E"), TEXT("R") };
    for (int32 I = 0; I < 3 && I < AbilityTexts.Num(); ++I)
    {
        const float Remaining = Character->GetAbilityCooldownRemaining(I);
        const FString Status = Remaining <= 0.01f ? TEXT("PRONTO") : FString::Printf(TEXT("%.1fs"), Remaining);
        AbilityTexts[I]->SetText(FText::FromString(FString::Printf(TEXT("[%s] %s\n%s"), Keys[I], *NWPremiumV6::AbilityName(Character->GetActiveWeapon(), I), *Status)));
        if (AbilityDescriptionTexts.IsValidIndex(I)) AbilityDescriptionTexts[I]->SetText(FText::FromString(NWPremiumV6::AbilityDescription(Character->GetActiveWeapon(), I)));
        const FLinearColor Theme = NWPremiumV6::ThemeColor(NWPremiumV6::Theme(Character->GetActiveWeapon(), I, Character->GetArrowElement()));
        AbilityTexts[I]->SetColorAndOpacity(FSlateColor(Remaining <= 0.01f ? Theme : FLinearColor(0.37f, 0.40f, 0.44f, 1.0f)));
        if (AbilityCards.IsValidIndex(I)) AbilityCards[I]->SetBrushColor(FLinearColor(Theme.R * 0.10f + 0.018f, Theme.G * 0.10f + 0.020f, Theme.B * 0.10f + 0.026f, 0.98f));
    }

    const FString Loot = Character->GetNearbyLootLabel();
    if (LootPromptText) LootPromptText->SetText(FText::FromString(Loot.IsEmpty() ? TEXT("") : FString::Printf(TEXT("[G] COLETAR  %s"), *Loot)));
    if (bInventoryVisible) RefreshInventory();
}

void UNWCombatHUDWidget::ToggleInventory()
{
    bInventoryVisible = !bInventoryVisible;
    if (InventoryPanel) InventoryPanel->SetVisibility(bInventoryVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bInventoryVisible) RefreshInventory(true);
    UE_LOG(LogTemp, Display, TEXT("[HUD-V6] Inventario %s"), bInventoryVisible ? TEXT("aberto") : TEXT("fechado"));
}

FString UNWCombatHUDWidget::FormatAffixes(const FNWGeneratedItem& Item) const
{
    FString Result;
    for (int32 I = 0; I < Item.Affixes.Num(); ++I)
    {
        if (I > 0) Result += TEXT("   ");
        Result += FString::Printf(TEXT("+%s %.1f"), *NWCombat::AffixToString(Item.Affixes[I].Type), Item.Affixes[I].Magnitude);
    }
    return Result;
}

uint32 UNWCombatHUDWidget::BuildInventorySignature(const ANWCharacter* Character) const
{
    if (!Character) return 0;
    uint32 Sig = HashCombine(GetTypeHash(Character->GetSelectedInventoryIndex()), GetTypeHash(Character->GetInventoryItems().Num()));
    for (const FNWGeneratedItem& Item : Character->GetInventoryItems()) Sig = HashCombine(Sig, GetTypeHash(Item.ItemSeed));
    for (const FNWGeneratedItem& Item : Character->GetEquippedItems()) Sig = HashCombine(Sig, GetTypeHash(Item.ItemSeed));
    for (const FNWGeneratedItem& Item : Character->GetEquippedWeaponItems()) Sig = HashCombine(Sig, GetTypeHash(Item.ItemSeed));
    return Sig;
}

void UNWCombatHUDWidget::RefreshInventory(bool bForce)
{
    ANWCharacter* Character = ObservedCharacter.Get();
    if (!Character || !InventoryItemsBox || !EquippedText) return;
    const uint32 Signature = BuildInventorySignature(Character);
    if (!bForce && Signature == LastInventorySignature) return;
    LastInventorySignature = Signature;

    FString Equipped = TEXT("EQUIPADO\n\nARMAS\n");
    const TArray<FNWGeneratedItem> Weapons = Character->GetEquippedWeaponItems();
    if (Weapons.IsEmpty()) Equipped += TEXT("  Nenhuma arma equipada\n");
    for (int32 I = 0; I < Weapons.Num(); ++I)
    {
        if (!Weapons[I].Name.IsEmpty()) Equipped += FString::Printf(TEXT("  [%d] %s\n     score %.1f\n"), I + 1, *Weapons[I].Name, NWCombat::GetItemScore(Weapons[I]));
    }
    Equipped += TEXT("\nARMADURA\n");
    const TArray<FNWGeneratedItem> Armor = Character->GetEquippedItems();
    if (Armor.IsEmpty()) Equipped += TEXT("  Corpo base · sem pecas equipadas\n");
    for (const FNWGeneratedItem& Item : Armor)
    {
        Equipped += FString::Printf(TEXT("  %s\n  %s\n  score %.1f\n\n"), *NWCombat::EquipmentSlotToString(Item.Slot), *Item.Name, NWCombat::GetItemScore(Item));
    }
    EquippedText->SetText(FText::FromString(Equipped));

    const TArray<FNWGeneratedItem> Inventory = Character->GetInventoryItems();
    if (InventoryTitleText) InventoryTitleText->SetText(FText::FromString(FString::Printf(TEXT("INVENTARIO  ·  %d ITENS"), Inventory.Num())));
    InventoryItemsBox->ClearChildren();

    if (Inventory.IsEmpty())
    {
        UTextBlock* Empty = MakeText(TEXT("Bag vazia. Derrote inimigos e colete loot com [G]."), 15);
        Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.62f, 0.68f, 1.0f)));
        InventoryItemsBox->AddChildToVerticalBox(Empty);
        return;
    }

    const int32 Selected = FMath::Clamp(Character->GetSelectedInventoryIndex(), 0, Inventory.Num() - 1);
    int32 SelectedVisualIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Inventory.Num(); ++Index)
    {
        const FNWGeneratedItem& Item = Inventory[Index];
        const bool bSelected = Index == Selected;
        const FLinearColor Rarity = NWCombat::RarityColor(Item.Rarity);
        UBorder* Card = MakePanel(
            bSelected ? FLinearColor(0.16f, 0.13f, 0.07f, 1.0f) : FLinearColor(0.026f, 0.033f, 0.043f, 1.0f),
            FMargin(12.0f, 8.0f));
        UVerticalBoxSlot* CardSlot = InventoryItemsBox->AddChildToVerticalBox(Card);
        CardSlot->SetPadding(FMargin(0, 0, 0, 6));
        if (bSelected) SelectedVisualIndex = InventoryItemsBox->GetChildrenCount() - 1;

        UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Card->SetContent(Body);

        UTextBlock* Name = MakeText(FString::Printf(TEXT("%s%s  ·  %s  ·  SCORE %.1f"), bSelected ? TEXT("▶ ") : TEXT(""), *Item.Name, *ItemTypeLabel(Item), NWCombat::GetItemScore(Item)), 14);
        Name->SetColorAndOpacity(FSlateColor(Rarity));
        Body->AddChildToVerticalBox(Name);

        const FString Comparison = Character->GetInventoryComparisonLabel(Index);
        UTextBlock* Compare = MakeText(Comparison, 12);
        const float Delta = Character->GetInventoryComparisonDelta(Index);
        Compare->SetColorAndOpacity(FSlateColor(
            Item.Kind == ENWItemKind::Consumable ? FLinearColor(0.90f, 0.70f, 0.25f, 1.0f) :
            Delta > 0.25f ? FLinearColor(0.20f, 0.92f, 0.36f, 1.0f) :
            Delta < -0.25f ? FLinearColor(0.94f, 0.26f, 0.20f, 1.0f) : FLinearColor(0.78f, 0.78f, 0.78f, 1.0f)));
        Body->AddChildToVerticalBox(Compare);

        UTextBlock* Affixes = MakeText(FormatAffixes(Item), 10);
        Affixes->SetColorAndOpacity(FSlateColor(FLinearColor(0.63f, 0.70f, 0.78f, 1.0f)));
        Body->AddChildToVerticalBox(Affixes);
    }

    if (InventoryScroll && SelectedVisualIndex != INDEX_NONE && InventoryItemsBox->GetChildrenCount() > SelectedVisualIndex)
    {
        InventoryScroll->ScrollWidgetIntoView(InventoryItemsBox->GetChildAt(SelectedVisualIndex), true, EDescendantScrollDestination::IntoView, 12.0f);
    }
}
