#include "NWNvidiaPerformanceDirector.h"

#include "HAL/IConsoleManager.h"

ANWNvidiaPerformanceDirector::ANWNvidiaPerformanceDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.0f;
    bReplicates = false;
}

void ANWNvidiaPerformanceDirector::BeginPlay()
{
    Super::BeginPlay();
    ApplyNvidiaPerformanceProfile();
}

void ANWNvidiaPerformanceDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    ElapsedSeconds += DeltaSeconds;

    // Alguns plugins registram os CVars alguns frames depois do GameMode. Uma
    // segunda tentativa cobre esse caso sem manter polling permanente.
    if (!bFinalRetryDone && ElapsedSeconds >= 3.0f)
    {
        bFinalRetryDone = true;
        ApplyNvidiaPerformanceProfile();
        SetActorTickEnabled(false);
    }
}

bool ANWNvidiaPerformanceDirector::TrySetInt(const TCHAR* Name, int32 Value) const
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value, ECVF_SetByGameSetting);
        return true;
    }
    return false;
}

bool ANWNvidiaPerformanceDirector::TrySetFloat(const TCHAR* Name, float Value) const
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value, ECVF_SetByGameSetting);
        return true;
    }
    return false;
}

void ANWNvidiaPerformanceDirector::ApplyNvidiaPerformanceProfile()
{
    // VSync desligado e importante para Frame Generation; a NVIDIA recomenda
    // evitar VSync controlado pelo jogo quando DLSS-G esta ativo.
    TrySetInt(TEXT("r.VSync"), 0);

    bool bDlss = false;
    bool bFrameGeneration = false;
    bool bReflex = false;

    const bool bNgxAvailable = TrySetInt(TEXT("r.NGX.Enable"), 1);
    const bool bDlssCVarAvailable = TrySetInt(TEXT("r.NGX.DLSS.Enable"), 1);
    bDlss = bNgxAvailable && bDlssCVarAvailable;

    if (bDlss)
    {
        // 67% e um alvo conservador para qualidade; o plugin oficial pode
        // sobrescrever internamente conforme o modo de DLSS selecionado.
        TrySetFloat(TEXT("r.ScreenPercentage"), 67.0f);
        TrySetInt(TEXT("r.NGX.DLSS.AutoExposure"), 1);
        TrySetInt(TEXT("r.NGX.DLSS.DilateMotionVectors"), 1);
    }

    const bool bReflexEnable = TrySetInt(TEXT("t.Streamline.Reflex.Enable"), 1);
    const bool bReflexMode = TrySetInt(TEXT("t.Streamline.Reflex.Mode"), 2);
    TrySetInt(TEXT("t.Streamline.Reflex.Auto"), 1);
    TrySetInt(TEXT("t.Streamline.Reflex.HandleMaxTickRate"), 0);
    bReflex = bReflexEnable || bReflexMode;

    // Este CVar so existe quando o plugin Streamline/DLSS-G realmente foi
    // carregado. Em Linux nativo ele normalmente nao existe; nesse caso nao
    // forçamos nada e deixamos TSR/Vulkan intactos.
    bFrameGeneration = TrySetInt(TEXT("r.Streamline.DLSSG.Enable"), 1);

    if (bFrameGeneration)
    {
        TrySetInt(TEXT("r.Streamline.DLSSG.AdjustMotionBlurTimeScale"), 1);
        TrySetInt(TEXT("r.Streamline.TagUIColorAlpha"), 1);
    }

    LogProfileSummary(bDlss, bFrameGeneration, bReflex);
    bAppliedOnce = true;
}

void ANWNvidiaPerformanceDirector::LogProfileSummary(bool bDlss, bool bFrameGeneration, bool bReflex)
{
#if PLATFORM_WINDOWS
    const TCHAR* PlatformLabel = TEXT("Windows/Win64");
#elif PLATFORM_LINUX
    const TCHAR* PlatformLabel = TEXT("Linux/Vulkan");
#else
    const TCHAR* PlatformLabel = TEXT("Outra plataforma");
#endif

    if (bDlss || bFrameGeneration || bReflex)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[RTX-V5] %s | DLSS-SR=%s | FrameGeneration=%s | Reflex=%s | plugin NVIDIA/Streamline detectado por CVars."),
            PlatformLabel,
            bDlss ? TEXT("ATIVO") : TEXT("indisponivel"),
            bFrameGeneration ? TEXT("ATIVO") : TEXT("indisponivel"),
            bReflex ? TEXT("ATIVO") : TEXT("indisponivel"));
        return;
    }

#if PLATFORM_LINUX
    UE_LOG(LogTemp, Warning,
        TEXT("[RTX-V5] Linux/Vulkan: plugin DLSS/Streamline oficial nao carregado; mantendo TSR. O caminho DLSS 4.5/MFG fica pronto para Win64/Proton/Windows quando o plugin oficial UE 5.8 estiver instalado."));
#else
    UE_LOG(LogTemp, Warning,
        TEXT("[RTX-V5] CVars NVIDIA nao encontrados. Instale/ative o plugin oficial DLSS 4.5 para UE 5.8 para habilitar DLSS SR, Reflex e Frame Generation."));
#endif
}
