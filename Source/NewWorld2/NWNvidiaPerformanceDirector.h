#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWNvidiaPerformanceDirector.generated.h"

/**
 * Camada de configuracao RTX/DLSS sem dependencia binaria do plugin NVIDIA.
 *
 * O projeto continua compilando quando o plugin DLSS/Streamline nao esta
 * instalado. Em runtime, o ator procura os CVars publicados pelo plugin oficial
 * e ativa apenas os recursos que realmente existem na plataforma atual.
 *
 * Isso e importante no BigLinux: o plugin UE oficial da NVIDIA e atualmente
 * distribuido para Win64. No Linux nativo o ator preserva TSR/Vulkan e registra
 * claramente o fallback. Em um build Win64/Proton ou Windows com o plugin
 * oficial instalado, DLSS SR, Reflex e Frame Generation sao habilitados
 * automaticamente quando os CVars estiverem disponiveis.
 */
UCLASS()
class NEWORLD2_API ANWNvidiaPerformanceDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWNvidiaPerformanceDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    bool TrySetInt(const TCHAR* Name, int32 Value) const;
    bool TrySetFloat(const TCHAR* Name, float Value) const;
    void ApplyNvidiaPerformanceProfile();
    void LogProfileSummary(bool bDlss, bool bFrameGeneration, bool bReflex);

    float ElapsedSeconds = 0.0f;
    bool bAppliedOnce = false;
    bool bFinalRetryDone = false;
};
