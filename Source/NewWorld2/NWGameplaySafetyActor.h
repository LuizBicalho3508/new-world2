#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWGameplaySafetyActor.generated.h"

class ANWCharacter;
class ANWProceduralWorldManager;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Estabiliza o primeiro playtest do terreno procedural.
 *
 * O terreno visual continua sendo gerado pelo UProceduralMeshComponent, mas este
 * ator cria um piso de colisao invisivel seguindo a inclinacao local da malha e
 * faz apenas uma recuperacao de emergencia caso o capsule atravesse a superficie.
 *
 * Tambem aplica budget global de inimigos e protecoes visuais. Packs Fab podem
 * usar centimetros, metros, pivots e escalas muito diferentes; componentes de arma
 * e criaturas fora de uma faixa plausivel sao normalizados em runtime.
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
    void StabilizeRuntimeVisuals(float DeltaSeconds);
    void NormalizeWeaponVisuals(ANWCharacter* Character);
    void NormalizeEnemyVisuals();
    void NormalizeStaticMeshComponent(UStaticMeshComponent* Component, float TargetMaxDimension, const TCHAR* Context);
    void EnforceEnemyPopulationBudget(float DeltaSeconds);

    UPROPERTY(VisibleAnywhere, Category="Safety")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Safety")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundCollisionProxy;

    // 64 coincide com a resolucao padrao do terreno (uma placa por celula).
    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="32", ClampMax="96"))
    int32 CollisionGridResolution = 64;

    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="200.0"))
    float CollisionProxyThickness = 800.0f;

    // Ao descer de um pulo e chegar muito perto da superficie, finalizamos o pouso
    // para evitar que um frame de atraso do Chaos deixe o capsule atravessar a malha.
    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="0.0"))
    float LandingSnapTolerance = 32.0f;

    // Recuperacao so acontece quando o personagem realmente ficou abaixo do piso.
    UPROPERTY(EditDefaultsOnly, Category="Safety", meta=(ClampMin="40.0"))
    float EmergencyRecoveryDepth = 140.0f;

    // Inclui mobs normais, inimigos de dungeon e bosses. Bosses mundiais e
    // guardioes de dungeon sao protegidos do culling; os mobs comuns mais distantes
    // dos jogadores sao removidos primeiro quando o teto e ultrapassado.
    UPROPERTY(EditDefaultsOnly, Category="Safety|Performance", meta=(ClampMin="16", ClampMax="120"))
    int32 MaxConcurrentEnemies = 42;

    UPROPERTY(EditDefaultsOnly, Category="Safety|Performance", meta=(ClampMin="0.5", ClampMax="10.0"))
    float PopulationCheckInterval = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category="Safety|Visual", meta=(ClampMin="0.10", ClampMax="5.0"))
    float VisualSafetyInterval = 0.50f;

    TWeakObjectPtr<ANWProceduralWorldManager> CachedWorldManager;
    int32 CachedEpoch = INDEX_NONE;
    float LastRecoveryLogTime = -1000.0f;
    float PopulationCheckAccumulator = 0.0f;
    float VisualSafetyAccumulator = 0.0f;
};
