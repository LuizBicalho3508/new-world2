#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NWCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ANWCharacter();

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, Category="Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, Category="Visual")
    TObjectPtr<UStaticMeshComponent> DebugBody;

    UPROPERTY(EditDefaultsOnly, Category="Movement")
    float WalkSpeed = 600.0f;

    UPROPERTY(EditDefaultsOnly, Category="Movement")
    float SprintSpeed = 850.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float MaxHealth = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Health, VisibleAnywhere, Category="Combat")
    float Health = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AttackDamage = 28.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AbilityDamage = 18.0f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float AbilityCooldown = 4.0f;

    UFUNCTION()
    void OnRep_Health();

    UFUNCTION(Server, Reliable)
    void ServerAttack();

    UFUNCTION(Server, Reliable)
    void ServerAbility1();

    UFUNCTION(Server, Reliable)
    void ServerSetSprinting(bool bSprinting);

    UFUNCTION(Server, Reliable)
    void ServerRegenerateWorld();

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void StartSprint();
    void StopSprint();
    void Attack();
    void Ability1();
    void RegenerateWorld();

    void ExecuteAttack();
    void ExecuteAbility1();
    void ApplySprintState(bool bSprinting);

    float LastAbilityTime = -1000.0f;
};
