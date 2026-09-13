#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWGameplaySafetyActor.generated.h"

class ANWProceduralWorldManager;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Estabiliza o primeiro playtest do terreno procedural.
 *
 * O terreno visual continua sendo gerado pelo UProceduralMeshComponent, mas este
 * ator cria um piso de colisao invisivel seguindo a inclinacao local da malha e
 * faz apenas uma recuperacao de emergencia caso o capsule atravesse a superficie.
 *
 * A ideia e deixar locomocao/pulo/combate previsiveis enquanto o terreno final
 * ainda nao migrou para Landscape/PCG com colisao autorada.
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

    TWeakObjectPtr<ANWProceduralWorldManager> CachedWorldManager;
    int32 CachedEpoch = INDEX_NONE;
    float LastRecoveryLogTime = -1000.0f;
};
