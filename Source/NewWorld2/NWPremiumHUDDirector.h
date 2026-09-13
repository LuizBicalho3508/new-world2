#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWPremiumHUDDirector.generated.h"

class APlayerController;

/**
 * Garante o HUD no player layer do viewport Linux. Consolida widgets duplicados
 * e reaplica o personagem observado depois de respawn/restart.
 */
UCLASS()
class NEWORLD2_API ANWPremiumHUDDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWPremiumHUDDirector();
    virtual void Tick(float DeltaSeconds) override;

private:
    void EnsureHUDForLocalPlayers();
    TSet<TWeakObjectPtr<APlayerController>> ReparentedControllers;
};
