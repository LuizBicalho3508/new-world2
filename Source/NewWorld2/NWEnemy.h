#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetHealthRatio() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

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

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackRange = 175.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackDamage = 9.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackCooldown = 1.15f;

    UFUNCTION()
    void OnRep_Health();

private:
    AActor* FindBestTarget() const;
    void SpawnProceduralLoot(AController* EventInstigator, AActor* DamageCauser);
    void TryApplyLicensedCreatureVisual();

    float LastAttackTime = -1000.0f;
    float StaggeredUntilTime = -1000.0f;
};
