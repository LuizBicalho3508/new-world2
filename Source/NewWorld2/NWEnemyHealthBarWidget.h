#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NWEnemyHealthBarWidget.generated.h"

class ANWEnemy;
class UProgressBar;
class UTextBlock;

UCLASS()
class NEWORLD2_API UNWEnemyHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetObservedEnemy(ANWEnemy* Enemy);
    void RefreshFromEnemy();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildWidgetTreeIfNeeded();

    TWeakObjectPtr<ANWEnemy> ObservedEnemy;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> NameText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HealthText;

    float RefreshAccumulator = 0.0f;
};
