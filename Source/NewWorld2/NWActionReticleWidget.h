#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NWActionReticleWidget.generated.h"

class UBorder;
class UCanvasPanel;

/** Reticulo central de action RPG: mira livre, sem exigir target lock. */
UCLASS()
class NEWORLD2_API UNWActionReticleWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

private:
    void BuildReticle();
    UBorder* AddBar(UCanvasPanel* Root, const FVector2D& Position, const FVector2D& Size, float Opacity);
};
