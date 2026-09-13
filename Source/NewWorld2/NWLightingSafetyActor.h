#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWLightingSafetyActor.generated.h"

class ANWProceduralWorldManager;
class ANWWorldEventDirector;

UCLASS()
class NEWORLD2_API ANWLightingSafetyActor : public AActor
{
    GENERATED_BODY()

public:
    ANWLightingSafetyActor();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void RefreshReferences();
    void ApplyLightingFix();

    TWeakObjectPtr<ANWProceduralWorldManager> WorldManager;
    TWeakObjectPtr<ANWWorldEventDirector> WorldDirector;
};
