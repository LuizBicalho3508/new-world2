#include "NWPremiumHUDDirector.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NWActionReticleWidget.h"
#include "NWCharacter.h"
#include "NWCombatHUDWidget.h"

ANWPremiumHUDDirector::ANWPremiumHUDDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.50f;
    bReplicates = false;
}

void ANWPremiumHUDDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld() || GetNetMode() == NM_DedicatedServer) { return; }
    EnsureHUDForLocalPlayers();

    for (auto It = ReparentedControllers.CreateIterator(); It; ++It)
    {
        if (!It->IsValid()) { It.RemoveCurrent(); }
    }
}

void ANWPremiumHUDDirector::EnsureHUDForLocalPlayers()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }
        ANWCharacter* Character = Cast<ANWCharacter>(PC->GetPawn());
        if (!Character) { continue; }

        TArray<UUserWidget*> HUDs;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, HUDs, UNWCombatHUDWidget::StaticClass(), false);

        UNWCombatHUDWidget* PrimaryHUD = nullptr;
        for (UUserWidget* Candidate : HUDs)
        {
            if (UNWCombatHUDWidget* HUD = Cast<UNWCombatHUDWidget>(Candidate))
            {
                if (!PrimaryHUD) { PrimaryHUD = HUD; }
                else { HUD->RemoveFromParent(); }
            }
        }

        if (!PrimaryHUD)
        {
            PrimaryHUD = CreateWidget<UNWCombatHUDWidget>(PC, UNWCombatHUDWidget::StaticClass());
        }

        if (PrimaryHUD)
        {
            PrimaryHUD->SetObservedCharacter(Character);
            PrimaryHUD->SetVisibility(ESlateVisibility::HitTestInvisible);
            PrimaryHUD->SetRenderOpacity(1.0f);

            // Forca uma unica vez o player screen layer. AddToViewport no Linux
            // pode acabar em uma camada diferente da viewport local usada pelo game.
            if (!ReparentedControllers.Contains(PC) || !PrimaryHUD->IsInViewport())
            {
                PrimaryHUD->RemoveFromParent();
                PrimaryHUD->AddToPlayerScreen(60);
                ReparentedControllers.Add(PC);
                UE_LOG(LogTemp, Warning, TEXT("[HUD-V4] HUD fixado no PlayerScreen para %s."), *Character->GetName());
            }
        }

        // Mantem apenas um reticulo separado. O CombatHUD tambem possui um marcador
        // central discreto, portanto removemos duplicatas de widgets de reticulo.
        TArray<UUserWidget*> Reticles;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, Reticles, UNWActionReticleWidget::StaticClass(), false);
        UNWActionReticleWidget* PrimaryReticle = nullptr;
        for (UUserWidget* Candidate : Reticles)
        {
            if (UNWActionReticleWidget* Reticle = Cast<UNWActionReticleWidget>(Candidate))
            {
                if (!PrimaryReticle) { PrimaryReticle = Reticle; }
                else { Reticle->RemoveFromParent(); }
            }
        }
        if (!PrimaryReticle)
        {
            PrimaryReticle = CreateWidget<UNWActionReticleWidget>(PC, UNWActionReticleWidget::StaticClass());
            if (PrimaryReticle) { PrimaryReticle->AddToPlayerScreen(100); }
        }
        else if (!PrimaryReticle->IsInViewport())
        {
            PrimaryReticle->AddToPlayerScreen(100);
        }
    }
}
