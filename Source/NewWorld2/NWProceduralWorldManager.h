#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "NWProceduralWorldManager.generated.h"

class ANWCivilian;
class ANWEnemy;
class ANWSettlementCore;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPCGComponent;
class UPCGGraphInterface;
class UProceduralMeshComponent;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;

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
    float GetTerrainHalfExtent() const { return TerrainResolution * TerrainCellSize * 0.5f; }

    UFUNCTION(BlueprintPure, Category="World")
    int32 GetWorldEpoch() const { return WorldEpoch; }

    void DisableAutomaticEvolution()
    {
        EvolutionIntervalSeconds = 0.0f;
        GetWorldTimerManager().ClearTimer(EvolutionTimer);
    }

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
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Bushes;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Rocks;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Crystals;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Buildings;

    UPROPERTY(VisibleAnywhere, Category="World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Structures;

    UPROPERTY(VisibleAnywhere, Category="PCG")
    TObjectPtr<UPCGComponent> RuntimePCG;

    UPROPERTY(EditAnywhere, Category="PCG")
    TSoftObjectPtr<UPCGGraphInterface> RuntimePCGGraph;

    UPROPERTY(EditAnywhere, Category="PCG")
    bool bEnableRuntimePartitionedPCG = true;

    UPROPERTY(EditAnywhere, Category="Visual")
    bool bAutoDiscoverInstalledFreeAssets = false;

    UPROPERTY(VisibleAnywhere, Category="Lighting")
    TObjectPtr<UDirectionalLightComponent> SunLight;

    UPROPERTY(VisibleAnywhere, Category="Lighting")
    TObjectPtr<USkyLightComponent> SkyLight;

    UPROPERTY(VisibleAnywhere, Category="Lighting")
    TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

    UPROPERTY(VisibleAnywhere, Category="Lighting")
    TObjectPtr<UExponentialHeightFogComponent> HeightFog;

    UPROPERTY(EditDefaultsOnly, Category="Generation", meta=(ClampMin="16", ClampMax="160"))
    int32 TerrainResolution = 64;

    UPROPERTY(EditDefaultsOnly, Category="Generation", meta=(ClampMin="100.0", ClampMax="1000.0"))
    float TerrainCellSize = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    float TerrainAmplitude = 900.0f;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 TreeCount = 160;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 BushCount = 110;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 RockCount = 65;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 CrystalCount = 24;

    UPROPERTY(EditDefaultsOnly, Category="Generation")
    int32 AmbientEnemyCount = 8;

    UPROPERTY(EditDefaultsOnly, Category="Settlements")
    int32 CiviliansPerSettlement = 3;

    UPROPERTY(EditDefaultsOnly, Category="Settlements")
    int32 InvasionWaveSizePerSettlement = 2;

    UPROPERTY(EditDefaultsOnly, Category="Settlements", meta=(ClampMin="60.0"))
    float InvasionIntervalSeconds = 150.0f;

    UPROPERTY(EditDefaultsOnly, Category="Evolution", meta=(ClampMin="0.0"))
    float EvolutionIntervalSeconds = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_WorldEpoch, VisibleAnywhere, Category="Evolution")
    int32 WorldEpoch = 1;

    UFUNCTION()
    void OnRep_WorldEpoch();

private:
    void BuildWorld();
    void BuildTerrain();
    void BuildDecorations();
    void BuildSettlements();
    void SpawnWorldActors();
    void SpawnAmbientEnemies();
    void SpawnSettlementsAndCivilians();
    void SpawnInvasionWave();
    void ClearSpawnedActors();
    void RelocatePlayersAfterEpoch();
    void ConfigureRuntimePCG();
    void TryApplyInstalledFreeWorldAssets();
    void ApplySafeFallbackMaterials();
    UMaterialInstanceDynamic* CreateFallbackMaterial(const FLinearColor& Color, const FName Name);
    UStaticMesh* FindInstalledStaticMesh(const TArray<FName>& Roots, const TArray<FString>& Keywords) const;

    float SampleHeight(float X, float Y) const;
    FVector2D RandomGroundPoint(FRandomStream& Random, float HalfWorld, float MinimumCenterDistance) const;
    TArray<FVector2D> GetSettlementCenters() const;
    int32 GetWorldSeed() const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ANWEnemy>> SpawnedEnemies;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ANWCivilian>> SpawnedCivilians;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ANWSettlementCore>> SpawnedSettlements;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TerrainFallbackMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TrunkFallbackMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> FoliageFallbackMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RockFallbackMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CrystalFallbackMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> BuildingFallbackMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> StructureFallbackMaterial;

    bool bUsingRealisticTreeMesh = false;
    bool bUsingRealisticBuildingMesh = false;

    FTimerHandle EvolutionTimer;
    FTimerHandle InvasionTimer;
};
