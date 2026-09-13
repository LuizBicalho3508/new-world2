#include "NWEnemyAnimationDirector.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "NWEnemy.h"

namespace
{
    bool IsUnsafeMobAnimationPath(const FString& InPath)
    {
        const FString Path = InPath.ToLower();
        return Path.Contains(TEXT("animstarterpack")) ||
            Path.Contains(TEXT("free_magic/demo")) ||
            Path.Contains(TEXT("deformablesnowsystem/demo")) ||
            Path.Contains(TEXT("/demo/")) ||
            Path.Contains(TEXT("/preview")) ||
            Path.Contains(TEXT("/tutorial"));
    }
}

ANWEnemyAnimationDirector::ANWEnemyAnimationDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.08f;
    bReplicates = false;
}

void ANWEnemyAnimationDirector::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { return; }
    ScanAnimations();
    UE_LOG(LogTemp, Warning, TEXT("[MOB-ANIM-V3] sequence director ativo | assets=%d | AnimBP de heroi nao e usado para AI."), AnimSequenceAssets.Num());
}

void ANWEnemyAnimationDirector::ScanAnimations()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Registry.GetAssets(Filter, AnimSequenceAssets);
}

void ANWEnemyAnimationDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld() || GetNetMode() == NM_DedicatedServer) { return; }

    const float Now = GetWorld()->GetTimeSeconds();
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (IsValid(Enemy)) { UpdateEnemy(Enemy, Now); }
    }

    for (auto It = EnemyStates.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
}

void ANWEnemyAnimationDirector::EnsureSequences(ANWEnemy* Enemy, FEnemyAnimState& State)
{
    if (!Enemy || !Enemy->GetMesh() || !Enemy->GetMesh()->GetSkeletalMeshAsset()) { return; }
    USkeleton* Skeleton = Enemy->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
    if (!Skeleton) { return; }
    if (State.Skeleton == Skeleton && State.Idle.IsValid() && State.Run.IsValid()) { return; }

    State = FEnemyAnimState();
    State.Skeleton = Skeleton;

    const TArray<FString> CommonPreferred = {
        TEXT("Paragon"), TEXT("Creature"), TEXT("Monster"), TEXT("Enemy"), TEXT("Combat")
    };
    State.Idle = FindBestSequence(Skeleton,
        { TEXT("Idle"), TEXT("Stand") },
        CommonPreferred);
    State.Run = FindBestSequence(Skeleton,
        { TEXT("Run"), TEXT("Jog"), TEXT("Walk") },
        CommonPreferred);
    State.Attack = FindBestSequence(Skeleton,
        { TEXT("Attack"), TEXT("Melee"), TEXT("Primary"), TEXT("Strike"), TEXT("Slash") },
        CommonPreferred);

    UE_LOG(LogTemp, Display, TEXT("[MOB-ANIM-V3] %s | idle=%s | run=%s | attack=%s"),
        *Enemy->GetName(),
        State.Idle.IsValid() ? *State.Idle->GetName() : TEXT("-"),
        State.Run.IsValid() ? *State.Run->GetName() : TEXT("-"),
        State.Attack.IsValid() ? *State.Attack->GetName() : TEXT("-"));
}

void ANWEnemyAnimationDirector::UpdateEnemy(ANWEnemy* Enemy, float Now)
{
    if (!Enemy || !Enemy->GetMesh() || !Enemy->GetMesh()->GetSkeletalMeshAsset()) { return; }

    FEnemyAnimState& State = EnemyStates.FindOrAdd(Enemy);
    EnsureSequences(Enemy, State);
    if (!State.Skeleton) { return; }

    const float Speed = Enemy->GetVelocity().Size2D();
    const float PlayerDistance = GetNearestPlayerDistance(Enemy);

    if (State.State == EVisualAnimState::Attack && Now < State.AttackPresentationEndsAt)
    {
        return;
    }

    if (State.Attack.IsValid() && PlayerDistance <= 255.0f && Now >= State.NextAttackPresentationTime)
    {
        PlayState(Enemy, State, EVisualAnimState::Attack, State.Attack.Get(), false);
        State.AttackPresentationEndsAt = Now + 0.65f;
        State.NextAttackPresentationTime = Now + (Enemy->IsWorldBoss() ? 1.05f : 1.20f);
        return;
    }

    if (Speed > 35.0f && State.Run.IsValid())
    {
        PlayState(Enemy, State, EVisualAnimState::Run, State.Run.Get(), true);
    }
    else if (State.Idle.IsValid())
    {
        PlayState(Enemy, State, EVisualAnimState::Idle, State.Idle.Get(), true);
    }
}

void ANWEnemyAnimationDirector::PlayState(
    ANWEnemy* Enemy,
    FEnemyAnimState& State,
    EVisualAnimState NewState,
    UAnimSequence* Sequence,
    bool bLoop)
{
    if (!Enemy || !Enemy->GetMesh() || !Sequence || State.State == NewState) { return; }

    Enemy->GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Enemy->GetMesh()->SetAnimInstanceClass(nullptr);
    Enemy->GetMesh()->PlayAnimation(Sequence, bLoop);
    State.State = NewState;
}

UAnimSequence* ANWEnemyAnimationDirector::FindBestSequence(
    USkeleton* Skeleton,
    const TArray<FString>& Keywords,
    const TArray<FString>& Preferred) const
{
    if (!Skeleton) { return nullptr; }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    UAnimSequence* Best = nullptr;
    for (const FAssetData& Asset : AnimSequenceAssets)
    {
        const int32 Score = ScoreSequence(Asset, Keywords, Preferred);
        if (Score <= BestScore) { continue; }

        UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset());
        if (!Sequence || Sequence->GetSkeleton() != Skeleton) { continue; }
        BestScore = Score;
        Best = Sequence;
    }
    return Best;
}

int32 ANWEnemyAnimationDirector::ScoreSequence(
    const FAssetData& Asset,
    const TArray<FString>& Keywords,
    const TArray<FString>& Preferred) const
{
    const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
    if (IsUnsafeMobAnimationPath(Searchable)) { return TNumericLimits<int32>::Lowest(); }

    int32 Score = 0;
    bool bPrimary = false;
    for (const FString& Keyword : Keywords)
    {
        if (Searchable.Contains(Keyword.ToLower()))
        {
            bPrimary = true;
            Score += 55;
        }
    }
    if (!bPrimary) { return TNumericLimits<int32>::Lowest(); }

    for (const FString& Keyword : Preferred)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 15; }
    }
    if (Searchable.Contains(TEXT("montage"))) { Score -= 60; }
    if (Searchable.Contains(TEXT("additive")) || Searchable.Contains(TEXT("upperbody"))) { Score -= 35; }
    return Score;
}

float ANWEnemyAnimationDirector::GetNearestPlayerDistance(const ANWEnemy* Enemy) const
{
    if (!Enemy || !GetWorld()) { return TNumericLimits<float>::Max(); }
    float Best = TNumericLimits<float>::Max();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn) { continue; }
        Best = FMath::Min(Best, FVector::Dist2D(Enemy->GetActorLocation(), Pawn->GetActorLocation()));
    }
    return Best;
}
