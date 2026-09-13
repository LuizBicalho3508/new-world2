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

    // O HUD V4 nasce junto do gate, mas fica coberto pela tela de carregamento ate
    // o warm-up terminar. Assim a camada do player ja esta pronta no primeiro frame jogavel.
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

        // Bloqueamos apenas gameplay; nao trocamos InputMode, evitando disputa com
        // Slate/viewport durante o bootstrap no Wayland/KDE.
        PC->SetIgnoreMoveInput(bBlocked);
        PC->SetIgnoreLookInput(bBlocked);
        PC->bShowMouseCursor = false;
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
