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

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Visual")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float MaxHealth = 60.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Health, VisibleAnywhere, Category="Combat")
    float Health = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category="AI")
    float MoveSpeed = 220.0f;

    UPROPERTY(EditDefaultsOnly, Category="AI")
    float DetectionRange = 1800.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackRange = 165.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackDamage = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackCooldown = 1.25f;

    UFUNCTION()
    void OnRep_Health();

private:
    APawn* FindNearestPlayer() const;

    float LastAttackTime = -1000.0f;
};
