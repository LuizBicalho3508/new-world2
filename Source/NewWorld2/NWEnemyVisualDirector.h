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
 * Premium V7 enemy presentation.
 *
 * One system owns enemy meshes. It deliberately does not require an AnimBlueprint:
 * NWEnemyAnimationDirector drives compatible AnimSequences by skeleton. This keeps
 * old Paragon character blueprints out of the AI path and lets creature/minion packs
 * participate even when they ship only skeletal meshes + sequences.
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
    USkeletalMesh* FindBestMeshForEnemy(const ANWEnemy* Enemy) const;
    USkeletalMesh* FindBestByKeywords(const TArray<FString>& Primary, const TArray<FString>& Preferred) const;
    USkeletalMesh* FindBestParagonFallback(const ANWEnemy* Enemy) const;
    int32 ScoreAsset(const FAssetData& Asset, const TArray<FString>& Primary, const TArray<FString>& Preferred, bool bRequirePrimary) const;
    float ComputeScale(USkeletalMesh* Mesh, float TargetHeight) const;
    void HideDebugMeshes(ANWEnemy* Enemy) const;

    TArray<FAssetData> SkeletalMeshAssets;
    TMap<TWeakObjectPtr<ANWEnemy>, int32> AppliedSignatures;
    TSet<TWeakObjectPtr<ANWCharacter>> LoggedWeaponSafetyCharacters;
};
