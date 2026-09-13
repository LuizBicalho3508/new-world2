#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NWCombatHUDWidget.generated.h"

class ANWCharacter;
class UBorder;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class NEWORLD2_API UNWCombatHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetObservedCharacter(ANWCharacter* Character);
    void ToggleInventory();
    bool IsInventoryVisible() const { return bInventoryVisible; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildHUD();
    void RefreshHUD();
    void RefreshInventory(bool bForce = false);
    void ApplyInventoryFocusMode();
    uint32 BuildInventorySignature(const ANWCharacter* Character) const;
    UTextBlock* MakeText(const FString& InitialText, int32 FontSize);
    UBorder* MakePanel(const FLinearColor& Color, const FMargin& Padding);
    FString FormatAffixes(const struct FNWGeneratedItem& Item) const;

    TWeakObjectPtr<ANWCharacter> ObservedCharacter;

    UPROPERTY(Transient) TObjectPtr<UBorder> VitalsPanelRef;
    UPROPERTY(Transient) TObjectPtr<UBorder> AbilityPanelRef;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> StaminaBar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StaminaText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WeaponText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatsText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StateText;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AbilityIconTexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AbilityTexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AbilityDescriptionTexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> AbilityCards;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LootPromptText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CrosshairText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HelpText;

    UPROPERTY(Transient) TObjectPtr<UBorder> InventoryPanel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> InventoryTitleText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EquippedText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> InventoryHintText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> InventoryItemsBox;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> InventoryScroll;

    bool bInventoryVisible = false;
    float RefreshAccumulator = 0.0f;
    uint32 LastInventorySignature = 0;
};
