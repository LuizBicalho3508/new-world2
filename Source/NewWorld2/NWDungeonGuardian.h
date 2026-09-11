#pragma once

#include "CoreMinimal.h"
#include "NWEnemy.h"
#include "NWDungeonGuardian.generated.h"

UCLASS()
class NEWORLD2_API ANWDungeonGuardian : public ANWEnemy
{
    GENERATED_BODY()

public:
    ANWDungeonGuardian();

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    void ConfigureGuardian(FName InDungeonTheme, int32 InTier);

private:
    void SpawnLegendaryRewards(AController* EventInstigator, AActor* DamageCauser);

    FName DungeonTheme = TEXT("DarkCastle");
    int32 DungeonTier = 1;
    bool bLegendaryRewardsSpawned = false;
};
