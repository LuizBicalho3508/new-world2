#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NWCombatTypes.h"
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

    void ReceiveProceduralLoot(int32 LootSeed);

    UFUNCTION(BlueprintPure, Category="Combat")
    ENWWeaponType GetActiveWeapon() const { return ActiveWeapon; }

    UFUNCTION(BlueprintPure, Category="Combat")
    FString GetActiveWeaponName() const;

    UFUNCTION(BlueprintPure, Category="Equipment")
    TArray<FNWGeneratedItem> GetEquippedItems() const { return EquippedItems; }

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
    float BaseMaxHealth = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Health, VisibleAnywhere, Category="Combat")
    float Health = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Loadout, VisibleAnywhere, Category="Combat")
    ENWWeaponType PrimaryWeapon = ENWWeaponType::Greatsword;

    UPROPERTY(ReplicatedUsing=OnRep_Loadout, VisibleAnywhere, Category="Combat")
    ENWWeaponType SecondaryWeapon = ENWWeaponType::Staff;

    UPROPERTY(ReplicatedUsing=OnRep_Loadout, VisibleAnywhere, Category="Combat")
    ENWWeaponType ActiveWeapon = ENWWeaponType::Greatsword;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float ComboWindowSeconds = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float CrossWeaponComboMultiplier = 1.18f;

    UPROPERTY(ReplicatedUsing=OnRep_Equipment, VisibleAnywhere, Category="Equipment")
    TArray<FNWGeneratedItem> EquippedItems;

    UFUNCTION()
    void OnRep_Health();

    UFUNCTION()
    void OnRep_Loadout();

    UFUNCTION()
    void OnRep_Equipment();

    UFUNCTION(Server, Reliable)
    void ServerAttack();

    UFUNCTION(Server, Reliable)
    void ServerUseAbility(uint8 AbilityIndex);

    UFUNCTION(Server, Reliable)
    void ServerSetActiveWeapon(ENWWeaponType RequestedWeapon);

    UFUNCTION(Server, Reliable)
    void ServerCycleLoadout(bool bPrimarySlot);

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
    void Ability2();
    void Ability3();
    void SelectPrimaryWeapon();
    void SelectSecondaryWeapon();
    void QuickSwapWeapon();
    void CyclePrimaryWeapon();
    void CycleSecondaryWeapon();
    void RegenerateWorld();

    void ExecuteAttack();
    void ExecuteAbility(uint8 AbilityIndex);
    void ExecuteDamageAbility(const FNWWeaponAbilityDefinition& Ability, float ComboMultiplier);
    void SetActiveWeaponInternal(ENWWeaponType RequestedWeapon);
    void ApplySprintState(bool bSprinting);
    void ApplyWeaponDamage(AActor* Target, float BaseDamage, bool bAllowPoison);
    void ApplyPoison(AActor* Target, float DamagePerTick);

    FNWGeneratedItem GenerateProceduralItem(int32 LootSeed) const;
    void EquipIfUpgrade(const FNWGeneratedItem& Item);
    void EnsureStarterEquipment();
    void RecalculateEquipmentStats();
    float GetAffixTotal(ENWAffixType AffixType) const;
    float GetItemScore(const FNWGeneratedItem& Item) const;
    bool HasPoisonCoating() const;
    float RollDamageWithStats(float BaseDamage) const;
    void LogLoadout() const;

    float MaxHealth = 100.0f;
    float DamageMultiplier = 1.0f;
    float HealingMultiplier = 1.0f;
    float CooldownMultiplier = 1.0f;
    float PrecisionChance = 0.0f;
    float LifeStealPercent = 0.0f;
    float PoisonDamagePerTick = 0.0f;

    float AbilityReadyTimes[3] = { 0.0f, 0.0f, 0.0f };
    float LastAbilityTime = -1000.0f;
    ENWWeaponType LastAbilityWeapon = ENWWeaponType::Greatsword;
};
