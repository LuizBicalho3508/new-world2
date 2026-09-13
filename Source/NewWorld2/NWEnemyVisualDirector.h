#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWEnemyVisualDirector.generated.h"

class ANWCharacter;
class ANWEnemy;
class USkeletalMesh;

/**
 * Garante que inimigos do playtest usem skeletal meshes reais instalados.
 * Procura primeiro monstros/undead/wraiths, depois usa personagens Paragon
 * instalados como ultimo recurso visual. Nunca volta para capsula visivel.
 * Tambem faz um safety pass no player para eliminar arma externa duplicada
 * no rig do Greystone e placeholders /Engine/BasicShapes.
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
    void StabilizePlayerWeaponVisuals();
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
    TSet<TWeakObjectPtr<ANWCharacter>> LoggedWeaponSafetyCharacters;
};
