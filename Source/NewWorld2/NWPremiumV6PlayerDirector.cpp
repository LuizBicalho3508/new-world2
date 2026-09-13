#include "NWPremiumV6PlayerDirector.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "NWCharacter.h"

ANWPremiumV6PlayerDirector::ANWPremiumV6PlayerDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.02f;
    bReplicates = false;
}

void ANWPremiumV6PlayerDirector::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("[MOVE-V6] diretor de locomocao camera-relative ativo; root motion externo nao bloqueia A/S/D."));
}

void ANWPremiumV6PlayerDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()) { return; }

    UpdatePlayers();

    LogAccumulator += DeltaSeconds;
    if (LogAccumulator >= 15.0f)
    {
        LogAccumulator = 0.0f;
        for (auto It = StarterConfigured.CreateIterator(); It; ++It)
        {
            if (!It->IsValid()) { It.RemoveCurrent(); }
        }
    }
}

void ANWPremiumV6PlayerDirector::UpdatePlayers()
{
    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character)) { continue; }

        if (Character->HasAuthority() && !StarterConfigured.Contains(Character))
        {
            Character->ConfigurePremiumV6StarterLoadout();
            StarterConfigured.Add(Character);
        }

        if (Character->IsLocallyControlled())
        {
            Character->PremiumV6StabilizeLocomotion();
        }
    }
}
