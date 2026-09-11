#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWProceduralWorldManager.generated.h"

class ANWEnemy;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

UCLASS()
class NEWORLD2_API ANWProceduralWorldManager : public AActor
{
    GENERATED_BODY()

public:
    ANWProceduralWorldManager();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category="World")
    void AdvanceEpoch();

    UFUNCTION(BlueprintPure, Category="World")
    float GetTerrainHeightAt(float X, float Y) const;

    UFUNCTION(BlueprintPure, Category="World")
    int32 GetWorldEpoch() const { return WorldEpoch; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UProceduralMeshComponent> TerrainMesh;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeTrunks;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeCrowns;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Rocks;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Crystals;

    UPROPERTY(VisibleAnywhere, Category="Lighting")
    TObjectPtr<UDirectionalLightComponent> SunLight;

    UPROPERTY(VisibleAnywhere, Category="Lighting")
    TObjectPtr<USkyLightComponent> SkyLight;

    UPROPERTY(VisibleAnywhere, Category="Lighting")
    TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

    UPROPERTY(VisibleAnywhere, Category="Lighting")
    TObjectPtr<UExponentialHeightFogComponent> HeightFog;

    UPROPERTY(EditDefaultsOnly, Category="Generation", meta=(ClampMin="16", ClampMax="128"))
    int32 TerrainResolution = 64;

    UPROPERTY(EditDefaultsOnly, Category="Generation", meta=(ClampMin="100.0", ClampMax="1000.0"))
    float TerrainCellSize = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    float TerrainAmplitude = 900.0f;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 TreeCount = 95;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 RockCount = 70;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 CrystalCount = 28;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 EnemyCount = 18;

    UPROPERTY(EditDefaultsOnly, Category="Evolution", meta=(ClampMin="30.0"))
    float EvolutionIntervalSeconds = 180.0f;

    UPROPERTY(ReplicatedUsing=OnRep_WorldEpoch, VisibleAnywhere, Category="Evolution")
    int32 WorldEpoch = 1;

    UFUNCTION()
    void OnRep_WorldEpoch();

private:
    void BuildWorld();
    void BuildTerrain();
    void BuildDecorations();
    void SpawnEnemies();
    void ClearSpawnedEnemies();
    void RelocatePlayersAfterEpoch();

    float SampleHeight(float X, float Y) const;
    FVector2D RandomGroundPoint(FRandomStream& Random, float HalfWorld, float MinimumCenterDistance) const;
    int32 GetWorldSeed() const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ANWEnemy>> SpawnedEnemies;

    FTimerHandle EvolutionTimer;
};
