#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWGameplaySafetyActor.generated.h"

class ANWProceduralWorldManager;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Guard rails para o primeiro playtest do mundo procedural.
 *
 * O terreno visual continua sendo o UProceduralMeshComponent do world manager,
 * mas este ator cria uma malha de colisao simples/invisivel por baixo dele e
 * recupera o player caso Chaos perca o contato com a superficie procedural.
 * Pode ser removido/substituido quando o terreno definitivo usar Landscape/PCG.
 */
UCLASS()
class NEWORLD2_API ANWGameplaySafetyActor : public AActor
{
    GENERATED_BODY()

public:
    ANWGameplaySafetyActor();

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    ANWProceduralWorldManager* ResolveWorldManager();
    void RebuildCollisionProxy();
    void StabilizePlayers();

    UPROPERTY(VisibleAnywhere, Category="Safety")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Safety")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundCollisionProxy;

    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="16", ClampMax="64"))
    int32 CollisionGridResolution = 40;

    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="500.0"))
    float CollisionProxyThickness = 2600.0f;

    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="0.0"))
    float VisualGroundTolerance = 12.0f;

    TWeakObjectPtr<ANWProceduralWorldManager> CachedWorldManager;
    int32 CachedEpoch = INDEX_NONE;
    float LastRecoveryLogTime = -1000.0f;
};
