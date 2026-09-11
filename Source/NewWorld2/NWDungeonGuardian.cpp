#include "NWDungeonGuardian.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "NWCombatLibrary.h"
#include "NWCharacter.h"
#include "NWLootPickup.h"
#include "NWProceduralWorldManager.h"

ANWDungeonGuardian::ANWDungeonGuardian()
{
    MaxHealth = 320.0f;
    Health = MaxHealth;
    AttackDamage = 17.0f;
    AttackCooldown = 0.92f;
    MoveSpeed = 255.0f;
    PlayerAggroRange = 3200.0f;
    WorldTargetRange = 5200.0f;
}

void ANWDungeonGuardian::ConfigureGuardian(FName InDungeonTheme, int32 InTier)
{
    DungeonTheme = InDungeonTheme.IsNone() ? FName(TEXT("DarkCastle")) : InDungeonTheme;
    DungeonTier = FMath::Max(1, InTier);

    MaxHealth = 280.0f + DungeonTier * 95.0f;
    Health = MaxHealth;
    AttackDamage = 14.0f + DungeonTier * 2.8f;
    MoveSpeed = 235.0f + DungeonTier * 6.0f;
    SetActorScale3D(FVector(1.12f + DungeonTier * 0.035f));
}

float ANWDungeonGuardian::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const bool bPredictedLethal = HasAuthority() && !bLegendaryRewardsSpawned && DamageAmount > 0.0f && DamageAmount >= Health;
    if (bPredictedLethal)
    {
        SpawnLegendaryRewards(EventInstigator, DamageCauser);
        bLegendaryRewardsSpawned = true;
    }

    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ANWDungeonGuardian::SpawnLegendaryRewards(AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || !GetWorld()) { return; }

    ANWCharacter* Killer = EventInstigator ? Cast<ANWCharacter>(EventInstigator->GetPawn()) : Cast<ANWCharacter>(DamageCauser);
    if (!Killer) { return; }

    int32 Epoch = 1;
    for (TActorIterator<ANWProceduralWorldManager> It(GetWorld()); It; ++It)
    {
        Epoch = It->GetWorldEpoch();
        break;
    }

    const int32 RewardCount = DungeonTier >= 4 ? 3 : 2;
    const int32 BaseSeed = GetUniqueID() * 193 + DungeonTier * 1009 + FMath::RoundToInt(GetWorld()->GetTimeSeconds() * 10.0f);
    for (int32 Index = 0; Index < RewardCount; ++Index)
    {
        const int32 Seed = BaseSeed + Index * 7919;
        const FNWGeneratedItem Reward = NWCombat::GenerateLegendaryDungeonItem(Seed, Epoch + DungeonTier, DungeonTheme);

        const float Angle = (2.0f * PI * Index) / FMath::Max(1, RewardCount);
        const FVector Offset(FMath::Cos(Angle) * 95.0f, FMath::Sin(Angle) * 95.0f, 70.0f);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ANWLootPickup* Pickup = GetWorld()->SpawnActor<ANWLootPickup>(ANWLootPickup::StaticClass(), GetActorLocation() + Offset, FRotator::ZeroRotator, Params);
        if (Pickup) { Pickup->InitializeLoot(Reward); }
    }

    // Guardioes de dungeon tem chance relevante, mas nao garantida, da metamorfose.
    // World bosses espalhados no mapa continuam garantindo a pocao para recompensar o risco maior.
    FRandomStream PotionRoll(BaseSeed ^ 0xB417A1);
    if (PotionRoll.FRand() <= 0.35f)
    {
        const FNWGeneratedItem Potion = NWCombat::GenerateBrutalTransformationPotion(BaseSeed ^ 0x51A7B0, Epoch + DungeonTier);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ANWLootPickup* PotionPickup = GetWorld()->SpawnActor<ANWLootPickup>(ANWLootPickup::StaticClass(), GetActorLocation() + FVector(0.0f, 0.0f, 135.0f), FRotator::ZeroRotator, Params);
        if (PotionPickup) { PotionPickup->InitializeLoot(Potion); }
        UE_LOG(LogTemp, Warning, TEXT("[DUNGEON] Pocao da Armadura Brutal Lendaria encontrada."));
    }

    UE_LOG(LogTemp, Warning, TEXT("[DUNGEON] Guardiao derrotado: %d recompensas lendarias do set %s."), RewardCount, *DungeonTheme.ToString());
}
