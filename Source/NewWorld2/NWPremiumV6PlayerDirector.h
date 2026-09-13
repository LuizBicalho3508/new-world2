#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWPremiumV6PlayerDirector.generated.h"

class ANWCharacter;

/**
 * Premium V6 player runtime director.
 * Mantem o input action-RPG responsivo mesmo quando montages de terceiros tentam
 * assumir root motion e inicializa o fluxo de equipamento demonstravel na bag.
 */
UCLASS()
class NEWORLD2_API ANWPremiumV6PlayerDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWPremiumV6PlayerDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void UpdatePlayers();

    TSet<TWeakObjectPtr<ANWCharacter>> StarterConfigured;
    float LogAccumulator = 0.0f;
};