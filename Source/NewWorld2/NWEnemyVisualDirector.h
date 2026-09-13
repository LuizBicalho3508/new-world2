#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWEnemyVisualDirector.generated.h"

class ANWEnemy;
class USkeletalMesh;

/**
 * Garante que inimigos do playtest usem skeletal meshes reais instalados.
 * Procura primeiro monstros/undead/wraiths, depois usa personagens Paragon
 * instalados como ultimo recurso visual. Nunca volta para capsula visivel.
 */
UCLASS()
class NEWORLD2_API ANWEnemyVisualDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWEnemyVisualDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void ScanAssets();
    void RefreshEnemyVisuals();
    bool ApplyVisual(ANWEnemy* Enemy);
    USkeletalMesh* FindBestMeshForEnemy(const ANWEnemy* Enemy, UClass*& OutAnimClass) const;
    USkeletalMesh* FindBestByKeywords(const TArray<FString>& Primary, const TArray<FString>& Preferred, UClass*& OutAnimClass) const;
    USkeletalMesh* FindBestParagonFallback(const ANWEnemy* Enemy, UClass*& OutAnimClass) const;
    UClass* FindAnimClass(USkeletalMesh* Mesh, const TArray<FString>& Preferred) const;
    int32 ScoreAsset(const FAssetData& Asset, const TArray<FString>& Primary, const TArray<FString>& Preferred, bool bRequirePrimary) const;
    float ComputeScale(USkeletalMesh* Mesh, float TargetHeight) const;
    void HideDebugMeshes(ANWEnemy* Enemy) const;

    TArray<FAssetData> SkeletalMeshAssets;
    TArray<FAssetData> AnimBlueprintAssets;
    TMap<TWeakObjectPtr<ANWEnemy>, int32> AppliedSignatures;
};
