#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWPremiumV6EnvironmentBooster.generated.h"

class ANWProceduralWorldManager;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;

/** Premium V6: segunda camada de natureza de baixo custo em HISM. */
UCLASS()
class NEWORLD2_API ANWPremiumV6EnvironmentBooster : public AActor
{
    GENERATED_BODY()

public:
    ANWPremiumV6EnvironmentBooster();

protected:
    virtual void BeginPlay() override;

private:
    void ScanAssets();
    UStaticMesh* FindBestMesh(const TArray<FString>& Keywords, float MinDim, float MaxDim, const FString& Exclude = FString()) const;
    void PrepareMeshForInstancing(UStaticMesh* Mesh) const;
    void PrepareExistingNatureMaterials() const;
    void Populate(ANWProceduralWorldManager* Manager);
    FVector EstimateNormal(ANWProceduralWorldManager* Manager, float X, float Y) const;
    float ScaleForDimension(UStaticMesh* Mesh, float Target) const;
    float GroundOffset(UStaticMesh* Mesh, float Scale) const;

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DenseGrass;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WildPlants;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundStones;

    TArray<FAssetData> StaticAssets;
};