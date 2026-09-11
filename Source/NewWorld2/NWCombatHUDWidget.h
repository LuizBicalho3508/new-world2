#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NWCombatHUDWidget.generated.h"

class ANWCharacter;
class UBorder;
class UProgressBar;
class UTextBlock;

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
    void RefreshInventory();
    UTextBlock* MakeText(const FString& InitialText, int32 FontSize = 16);

    TWeakObjectPtr<ANWCharacter> ObservedCharacter;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> StaminaBar;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HealthText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StaminaText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> WeaponText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StateText;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> AbilityTexts;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LootPromptText;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> InventoryPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> InventoryText;

    bool bInventoryVisible = false;
    float RefreshAccumulator = 0.0f;
};
