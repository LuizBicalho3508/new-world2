#include "NWStartupWarmupDirector.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
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
        TEXT("[STARTUP-V10] gate curto PSO-only | Niagara forcado=0 | minimo=%.1fs | timeout=%.1fs"),
        MinimumLoadingSeconds, MaximumLoadingSeconds);
}

void ANWStartupWarmupDirector::PrimeCombatNiagara()
{
    // V10 deliberately does not load or poll every NiagaraSystem under /Game.
    // The combat VFX director owns a tiny deterministic whitelist instead.
    CombatNiagaraSystems.Reset();
}

int32 ANWStartupWarmupDirector::CountPendingCombatNiagara()
{
    return 0;
}

void ANWStartupWarmupDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished || !GetWorld()) { return; }

    ElapsedSeconds += DeltaSeconds;
    EnsureLocalLoadingScreens();
    if (!bInputBlocked) { SetPlayerInputBlocked(true); }

    const int32 RemainingPSO = static_cast<int32>(FShaderPipelineCache::NumPrecompilesRemaining());
    const int32 RemainingNiagara = 0;
    PeakOutstandingPSOs = FMath::Max(PeakOutstandingPSOs, RemainingPSO);

    if (RemainingPSO > 0)
    {
        LastOutstandingWorkTime = ElapsedSeconds;
    }

    const bool bMinimumTimeReached = ElapsedSeconds >= MinimumLoadingSeconds;
    const bool bQuietLongEnough = RemainingPSO <= 0 &&
        (ElapsedSeconds - LastOutstandingWorkTime) >= QuietWindowSeconds;
    const bool bTimedOut = ElapsedSeconds >= MaximumLoadingSeconds;
    const bool bShouldFinish = bTimedOut || (bMinimumTimeReached && bQuietLongEnough);

    for (auto& Pair : LoadingWidgets)
    {
        if (UNWStartupLoadingWidget* Widget = Pair.Value.Get())
        {
            Widget->UpdateWarmupStatus(RemainingPSO,
                PeakOutstandingPSOs,
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
        TEXT("[STARTUP-V10] jogo liberado | tempo=%.1fs | picoPSO=%d | restantePSO=%d | motivo=%s"),
        ElapsedSeconds,
        PeakOutstandingPSOs,
        RemainingPSO,
        bTimedOut ? TEXT("timeout-controlado") : TEXT("fila-prioritaria-pronta"));

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
