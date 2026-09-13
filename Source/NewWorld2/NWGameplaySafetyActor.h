#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWGameplaySafetyActor.generated.h"

class ANWProceduralWorldManager;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Safety net do vertical slice: acompanha a superficie procedural com um proxy de
 * colisao, recupera o player apenas quando realmente atravessa o piso e aplica um
 * teto de emergencia para a populacao de inimigos.
 *
 * Apresentacao visual NAO e alterada aqui. Armas, armaduras e criaturas possuem um
 * unico dono (NWContentPresentationManager), evitando corrida de escala/visibilidade.
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
    void EnforceEnemyPopulationBudget(float DeltaSeconds);

    UPROPERTY(VisibleAnywhere, Category="Safety")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Safety")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundCollisionProxy;

    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="32", ClampMax="96"))
    int32 CollisionGridResolution = 64;

    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="200.0"))
    float CollisionProxyThickness = 800.0f;

    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="0.0"))
    float LandingSnapTolerance = 32.0f;

    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="40.0"))
    float EmergencyRecoveryDepth = 140.0f;

    UPROPERTY(EditDefaultsOnly, Category="Safety|Performance", meta=(ClampMin="16", ClampMax="120"))
    int32 MaxConcurrentEnemies = 42;

    UPROPERTY(EditDefaultsOnly, Category="Safety|Performance", meta=(ClampMin="0.5", ClampMax="10.0"))
    float PopulationCheckInterval = 2.0f;

    TWeakObjectPtr<ANWProceduralWorldManager> CachedWorldManager;
    int32 CachedEpoch = INDEX_NONE;
    float LastRecoveryLogTime = -1000.0f;
    float PopulationCheckAccumulator = 0.0f;
};
