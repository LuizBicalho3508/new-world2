#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWPremiumEnvironmentDirector.generated.h"

class ANWProceduralWorldManager;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

/**
 * V9 environment layer for the fixed/hybrid vertical slice.
 * Macro layout comes from NWProceduralWorldManager; this actor adds only
 * deterministic HISM nature while preserving roads, settlements and spawn space.
 */
UCLASS()
class NEWORLD2_API ANWPremiumEnvironmentDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWPremiumEnvironmentDirector();

protected:
    virtual void BeginPlay() override;

private:
    void ScanInstalledAssets();
    void BuildPremiumEnvironment(ANWProceduralWorldManager* WorldManager);
    void HideLegacyPrototypeDecor(ANWProceduralWorldManager* WorldManager) const;
    void ApplyGroundMaterial(ANWProceduralWorldManager* WorldManager);
    void PrepareMeshForInstancing(UStaticMesh* Mesh) const;

    UStaticMesh* FindBestNatureMesh(
        const TArray<FString>& PrimaryKeywords,
        const TArray<FString>& PreferredKeywords,
        float MinDimension,
        float MaxDimension,
        const FString& ExcludedPath = FString()) const;

    UMaterialInterface* FindBestGroundMaterial() const;
    int32 ScoreNatureAsset(const FAssetData& Asset, const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const;
    float ComputeScaleForHeight(UStaticMesh* Mesh, float TargetHeight) const;
    float ComputeScaleForMaxDimension(UStaticMesh* Mesh, float TargetDimension) const;
    float ComputeGroundOffset(UStaticMesh* Mesh, float UniformScale) const;
    FVector EstimateTerrainNormal(ANWProceduralWorldManager* WorldManager, float X, float Y) const;
    bool IsTerrainUsable(ANWProceduralWorldManager* WorldManager, float X, float Y, float MinimumNormalZ = 0.72f) const;
    bool IsPlacementClear(ANWProceduralWorldManager* WorldManager, float X, float Y, float MaxPathInfluence, float SettlementExtra) const;
    FVector2D RandomPoint(FRandomStream& Random, float HalfExtent, float ClearRadius) const;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreesPrimary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreesSecondary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundCover;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Bushes;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RocksPrimary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RocksSecondary;

    TArray<FAssetData> StaticMeshAssets;
    TArray<FAssetData> MaterialAssets;
};
