#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NWCombatTypes.h"
#include "NWEnemy.generated.h"

class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWEnemy : public ACharacter
{
    GENERATED_BODY()

public:
    ANWEnemy();

    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void ApplyStagger(float DurationSeconds);
    void ConfigureEnemy(ENWEnemyArchetype InArchetype, bool bInWorldBoss = false, int32 InBossTier = 1);

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetHealthRatio() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

    UFUNCTION(BlueprintPure, Category="Enemy")
    ENWEnemyArchetype GetEnemyArchetype() const { return EnemyArchetype; }

    UFUNCTION(BlueprintPure, Category="Enemy")
    bool IsWorldBoss() const { return bWorldBoss; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Visual")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float MaxHealth = 70.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Health, VisibleAnywhere, Category="Combat")
    float Health = 70.0f;

    UPROPERTY(EditDefaultsOnly, Category="AI")
    float MoveSpeed = 235.0f;

    UPROPERTY(EditDefaultsOnly, Category="AI")
    float PlayerAggroRange = 2100.0f;

    UPROPERTY(EditDefaultsOnly, Category="AI")
    float WorldTargetRange = 9000.0f;

    UPROPERTY(EditDefaultsOnly, Category="AI|Performance")
    float SleepDistanceFromPlayers = 5200.0f;

    UPROPERTY(EditDefaultsOnly, Category="AI|Performance")
    float NormalThinkInterval = 0.10f;

    UPROPERTY(EditDefaultsOnly, Category="AI|Performance")
    float SleepingThinkInterval = 0.50f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackRange = 175.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackDamage = 9.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackCooldown = 1.15f;

    UPROPERTY(ReplicatedUsing=OnRep_EnemyIdentity, VisibleAnywhere, Category="Enemy")
    ENWEnemyArchetype EnemyArchetype = ENWEnemyArchetype::Brute;

    UPROPERTY(ReplicatedUsing=OnRep_EnemyIdentity, VisibleAnywhere, Category="Enemy")
    bool bWorldBoss = false;

    UPROPERTY(ReplicatedUsing=OnRep_EnemyIdentity, VisibleAnywhere, Category="Enemy")
    int32 BossTier = 1;

    UFUNCTION()
    void OnRep_Health();

    UFUNCTION()
    void OnRep_EnemyIdentity();

private:
    AActor* FindBestTarget() const;
    float GetNearestPlayerDistance() const;
    void ApplyArchetypeStats();
    void SpawnProceduralLoot(AController* EventInstigator, AActor* DamageCauser);
    void SpawnLootItem(const FNWGeneratedItem& Item, const FVector& Offset);
    void TryApplyLicensedCreatureVisual();
    class USkeletalMesh* FindInstalledCreatureMesh(const TArray<FString>& Keywords) const;

    TWeakObjectPtr<AActor> CachedTarget;
    float NextTargetRefreshTime = -1000.0f;
    float LastAttackTime = -1000.0f;
    float StaggeredUntilTime = -1000.0f;
};
