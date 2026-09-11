#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NWGameMode.generated.h"

class ANWProceduralWorldManager;

UCLASS()
class NEWORLD2_API ANWGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ANWGameMode();

    virtual void StartPlay() override;
    virtual void RestartPlayer(AController* NewPlayer) override;

private:
    ANWProceduralWorldManager* EnsureWorldManager();

    UPROPERTY(Transient)
    TObjectPtr<ANWProceduralWorldManager> WorldManager;
};
