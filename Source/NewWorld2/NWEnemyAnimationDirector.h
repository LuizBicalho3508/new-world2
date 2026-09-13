#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWEnemyAnimationDirector.generated.h"

class ANWEnemy;
class UAnimSequence;
class USkeleton;

/**
 * Premium V3 mob animation layer.
 * Evita executar AnimBlueprints de herois como AI (o Greystone AnimBP gerava
 * Divide_DoubleDouble no V2). Usa sequencias compatíveis com o skeleton e troca
 * Idle/Run/Attack somente quando o estado muda.
 */
UCLASS()
class NEWORLD2_API ANWEnemyAnimationDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWEnemyAnimationDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    enum class EVisualAnimState : uint8
    {
        None,
        Idle,
        Run,
        Attack
    };

    struct FEnemyAnimState
    {
        EVisualAnimState State = EVisualAnimState::None;
        TWeakObjectPtr<UAnimSequence> Idle;
        TWeakObjectPtr<UAnimSequence> Run;
        TWeakObjectPtr<UAnimSequence> Attack;
        TObjectPtr<USkeleton> Skeleton = nullptr;
        float NextAttackPresentationTime = 0.0f;
        float AttackPresentationEndsAt = 0.0f;
    };

    void ScanAnimations();
    void UpdateEnemy(ANWEnemy* Enemy, float Now);
    void EnsureSequences(ANWEnemy* Enemy, FEnemyAnimState& State);
    UAnimSequence* FindBestSequence(USkeleton* Skeleton, const TArray<FString>& Keywords, const TArray<FString>& Preferred) const;
    int32 ScoreSequence(const FAssetData& Asset, const TArray<FString>& Keywords, const TArray<FString>& Preferred) const;
    float GetNearestPlayerDistance(const ANWEnemy* Enemy) const;
    void PlayState(ANWEnemy* Enemy, FEnemyAnimState& State, EVisualAnimState NewState, UAnimSequence* Sequence, bool bLoop);

    TArray<FAssetData> AnimSequenceAssets;
    TMap<TWeakObjectPtr<ANWEnemy>, FEnemyAnimState> EnemyStates;
};
