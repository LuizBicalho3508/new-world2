#include "NWStartupWarmupDirector.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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

    FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
    LastOutstandingPSOTime = 0.0f;
    EnsureLocalLoadingScreens();
    SetPlayerInputBlocked(true);
    UE_LOG(LogTemp, Warning, TEXT("[STARTUP-V4] loading gate ativo | ShaderPipelineCache=Fast | minimo=%.1fs | timeout=%.1fs"),
        MinimumLoadingSeconds, MaximumLoadingSeconds);
}

void ANWStartupWarmupDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished || !GetWorld()) { return; }

    ElapsedSeconds += DeltaSeconds;
    EnsureLocalLoadingScreens();
    if (!bInputBlocked) { SetPlayerInputBlocked(true); }

    const int32 Remaining = static_cast<int32>(FShaderPipelineCache::NumPrecompilesRemaining());
    PeakOutstandingPSOs = FMath::Max(PeakOutstandingPSOs, Remaining);
    if (Remaining > 0)
    {
        LastOutstandingPSOTime = ElapsedSeconds;
    }

    const bool bMinimumTimeReached = ElapsedSeconds >= MinimumLoadingSeconds;
    const bool bQuietLongEnough = Remaining <= 0 && (ElapsedSeconds - LastOutstandingPSOTime) >= QuietWindowSeconds;
    const bool bTimedOut = ElapsedSeconds >= MaximumLoadingSeconds;
    const bool bShouldFinish = bTimedOut || (bMinimumTimeReached && bQuietLongEnough);

    for (auto& Pair : LoadingWidgets)
    {
        if (UNWStartupLoadingWidget* Widget = Pair.Value.Get())
        {
            Widget->UpdateWarmupStatus(Remaining, PeakOutstandingPSOs, ElapsedSeconds, bShouldFinish);
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
        if (bBlocked)
        {
            FInputModeUIOnly Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(Mode);
            PC->bShowMouseCursor = false;
        }
        else
        {
            FInputModeGameOnly Mode;
            PC->SetInputMode(Mode);
            PC->bShowMouseCursor = false;
        }
    }
}

void ANWStartupWarmupDirector::FinishWarmup(bool bTimedOut)
{
    if (bFinished) { return; }
    bFinished = true;

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

    UE_LOG(LogTemp, Warning, TEXT("[STARTUP-V4] jogo liberado | tempo=%.1fs | picoPSO=%d | motivo=%s"),
        ElapsedSeconds,
        PeakOutstandingPSOs,
        bTimedOut ? TEXT("timeout-seguro") : TEXT("fila-inicial-pronta"));

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
    Super::EndPlay(EndPlayReason);
}
