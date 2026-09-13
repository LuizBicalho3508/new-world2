#include "NWStartupWarmupDirector.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "NiagaraSystem.h"
#include "NWPremiumHUDDirector.h"
#include "NWStartupLoadingWidget.h"
#include "ShaderPipelineCache.h"

ANWStartupWarmupDirector::ANWStartupWarmupDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.10f;
    bReplicates = false;
}

void ANWStartupWarmupDirector::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer)
    {
        bFinished = true;
        SetActorTickEnabled(false);
        return;
    }

    bool bHasHUDDirector = false;
    for (TActorIterator<ANWPremiumHUDDirector> It(GetWorld()); It; ++It)
    {
        bHasHUDDirector = true;
        break;
    }
    if (!bHasHUDDirector)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        GetWorld()->SpawnActor<ANWPremiumHUDDirector>(ANWPremiumHUDDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
    PrimeCombatNiagara();
    LastOutstandingWorkTime = 0.0f;
    EnsureLocalLoadingScreens();
    SetPlayerInputBlocked(true);

    UE_LOG(LogTemp, Warning,
        TEXT("[STARTUP-V7] loading gate ativo | PSO=Fast | Niagara=%d | minimo=%.1fs | timeout=%.1fs"),
        CombatNiagaraSystems.Num(), MinimumLoadingSeconds, MaximumLoadingSeconds);
}

void ANWStartupWarmupDirector::PrimeCombatNiagara()
{
    CombatNiagaraSystems.Reset();

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);

    struct FScoredNiagara
    {
        int32 Score = 0;
        FAssetData Asset;
    };

    TArray<FScoredNiagara> Scored;
    static const TCHAR* Desired[] = {
        TEXT("fire"), TEXT("flame"), TEXT("ice"), TEXT("frost"), TEXT("lightning"), TEXT("electric"),
        TEXT("earth"), TEXT("rock"), TEXT("slash"), TEXT("blade"), TEXT("shadow"), TEXT("poison"),
        TEXT("blood"), TEXT("heal"), TEXT("holy"), TEXT("arrow"), TEXT("trail"), TEXT("projectile"),
        TEXT("muzzle"), TEXT("explosion"), TEXT("impact"), TEXT("magic")
    };
    static const TCHAR* Blocked[] = {
        TEXT("/demo/"), TEXT("/tutorial"), TEXT("preview"), TEXT("benchmark"), TEXT("debug"),
        TEXT("_splinevfx"), TEXT("distancefield"), TEXT("distance_field"), TEXT("grid3d"), TEXT("fluid"),
        TEXT("skeletalmeshbones"), TEXT("skeletalmeshtris"), TEXT("bubble_burst")
    };

    for (const FAssetData& Asset : Assets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        bool bBlocked = false;
        for (const TCHAR* Token : Blocked)
        {
            if (Search.Contains(Token)) { bBlocked = true; break; }
        }
        if (bBlocked) { continue; }

        int32 Score = 0;
        bool bMatch = false;
        for (const TCHAR* Token : Desired)
        {
            if (Search.Contains(Token)) { bMatch = true; Score += 14; }
        }
        if (!bMatch) { continue; }
        if (Search.Contains(TEXT("free_magic"))) Score += 80;
        if (Search.Contains(TEXT("arrowtrail"))) Score += 70;
        if (Search.Contains(TEXT("/fab/"))) Score += 60;
        if (Search.Contains(TEXT("niagaraexamples"))) Score += 18;
        if (Search.Contains(TEXT("loop")) || Search.Contains(TEXT("ambient"))) Score -= 20;
        Scored.Add({ Score, Asset });
    }

    Scored.Sort([](const FScoredNiagara& A, const FScoredNiagara& B)
    {
        if (A.Score != B.Score) { return A.Score > B.Score; }
        return A.Asset.PackageName.LexicalLess(B.Asset.PackageName);
    });

    constexpr int32 MaxWarmupSystems = 16;
    for (const FScoredNiagara& Entry : Scored)
    {
        if (CombatNiagaraSystems.Num() >= MaxWarmupSystems) { break; }
        UNiagaraSystem* System = Cast<UNiagaraSystem>(Entry.Asset.GetAsset());
        if (!System || CombatNiagaraSystems.Contains(System)) { continue; }
        CombatNiagaraSystems.Add(System);
        // bFlushRequestCompile=true converts LazyOnDemand work into real startup work.
        System->PollForCompilationComplete(true);
    }
}

int32 ANWStartupWarmupDirector::CountPendingCombatNiagara()
{
    int32 Pending = 0;
    for (UNiagaraSystem* System : CombatNiagaraSystems)
    {
        if (!System) { continue; }
        if (!System->PollForCompilationComplete(true)) { ++Pending; }
    }
    return Pending;
}

void ANWStartupWarmupDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished || !GetWorld()) { return; }

    ElapsedSeconds += DeltaSeconds;
    EnsureLocalLoadingScreens();
    if (!bInputBlocked) { SetPlayerInputBlocked(true); }

    const int32 RemainingPSO = static_cast<int32>(FShaderPipelineCache::NumPrecompilesRemaining());
    const int32 RemainingNiagara = CountPendingCombatNiagara();
    PeakOutstandingPSOs = FMath::Max(PeakOutstandingPSOs, RemainingPSO);
    PeakOutstandingNiagara = FMath::Max(PeakOutstandingNiagara, RemainingNiagara);

    if (RemainingPSO > 0 || RemainingNiagara > 0)
    {
        LastOutstandingWorkTime = ElapsedSeconds;
    }

    const bool bMinimumTimeReached = ElapsedSeconds >= MinimumLoadingSeconds;
    const bool bQuietLongEnough = RemainingPSO <= 0 && RemainingNiagara <= 0 &&
        (ElapsedSeconds - LastOutstandingWorkTime) >= QuietWindowSeconds;
    const bool bTimedOut = ElapsedSeconds >= MaximumLoadingSeconds;
    const bool bShouldFinish = bTimedOut || (bMinimumTimeReached && bQuietLongEnough);

    for (auto& Pair : LoadingWidgets)
    {
        if (UNWStartupLoadingWidget* Widget = Pair.Value.Get())
        {
            Widget->UpdateWarmupStatus(RemainingPSO + RemainingNiagara,
                PeakOutstandingPSOs + PeakOutstandingNiagara,
                ElapsedSeconds,
                bShouldFinish);
        }
    }

    if (bShouldFinish)
    {
        FinishWarmup(bTimedOut);
    }
}

void ANWStartupWarmupDirector::EnsureLocalLoadingScreens()
{
    if (!GetWorld()) { return; }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }

        TWeakObjectPtr<UNWStartupLoadingWidget>& Existing = LoadingWidgets.FindOrAdd(PC);
        if (Existing.IsValid()) { continue; }

        UNWStartupLoadingWidget* Widget = CreateWidget<UNWStartupLoadingWidget>(PC, UNWStartupLoadingWidget::StaticClass());
        if (!Widget) { continue; }

        Widget->AddToPlayerScreen(1000);
        Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
        Existing = Widget;
    }
}

void ANWStartupWarmupDirector::SetPlayerInputBlocked(bool bBlocked)
{
    if (!GetWorld()) { return; }
    bInputBlocked = bBlocked;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }
        PC->SetIgnoreMoveInput(bBlocked);
        PC->SetIgnoreLookInput(bBlocked);
        PC->bShowMouseCursor = false;
    }
}

void ANWStartupWarmupDirector::FinishWarmup(bool bTimedOut)
{
    if (bFinished) { return; }
    bFinished = true;

    const int32 RemainingNiagara = CountPendingCombatNiagara();
    const int32 RemainingPSO = static_cast<int32>(FShaderPipelineCache::NumPrecompilesRemaining());

    SetPlayerInputBlocked(false);
    FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);

    for (auto& Pair : LoadingWidgets)
    {
        if (UNWStartupLoadingWidget* Widget = Pair.Value.Get())
        {
            Widget->RemoveFromParent();
        }
    }
    LoadingWidgets.Reset();

    UE_LOG(LogTemp, Warning,
        TEXT("[STARTUP-V7] jogo liberado | tempo=%.1fs | picoPSO=%d | picoNiagara=%d | restantePSO=%d | restanteNiagara=%d | motivo=%s"),
        ElapsedSeconds,
        PeakOutstandingPSOs,
        PeakOutstandingNiagara,
        RemainingPSO,
        RemainingNiagara,
        bTimedOut ? TEXT("timeout") : TEXT("compilacao-prioritaria-pronta"));

    SetActorTickEnabled(false);
}

void ANWStartupWarmupDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (!bFinished)
    {
        SetPlayerInputBlocked(false);
        for (auto& Pair : LoadingWidgets)
        {
            if (UNWStartupLoadingWidget* Widget = Pair.Value.Get())
            {
                Widget->RemoveFromParent();
            }
        }
    }
    LoadingWidgets.Reset();
    CombatNiagaraSystems.Reset();
    Super::EndPlay(EndPlayReason);
}
