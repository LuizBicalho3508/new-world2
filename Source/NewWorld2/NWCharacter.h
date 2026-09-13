#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NWCombatTypes.h"
#include "NWCharacter.generated.h"

class ANWLootPickup;
class UNWCombatHUDWidget;
class UAnimSequence;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ANWCharacter();

    virtual void Tick(float DeltaSeconds) override;
    virtual void PawnClientRestart() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    bool TryAddInventoryItem(const FNWGeneratedItem& Item);
    void ApplyStagger(float DurationSeconds, AActor* SourceActor = nullptr);

    // Premium V6: API pequena e explicita para os diretores de UX/visual sem
    // expor os arrays internos para actors externos.
    void ConfigurePremiumV6StarterLoadout();
    void PremiumV6StabilizeLocomotion();
    bool HasEquippedWeaponItem(ENWWeaponType WeaponType) const;
    float GetInventoryComparisonDelta(int32 InventoryIndex) const;
    FString GetInventoryComparisonLabel(int32 InventoryIndex) const;
    void SetSelectedInventoryIndexSafe(int32 NewIndex);
    void EquipInventoryItemAtIndex(int32 InventoryIndex);

    UFUNCTION(BlueprintPure, Category="Combat")
    ENWWeaponType GetActiveWeapon() const { return ActiveWeapon; }

    UFUNCTION(BlueprintPure, Category="Combat")
    ENWWeaponType GetPrimaryWeapon() const { return PrimaryWeapon; }

    UFUNCTION(BlueprintPure, Category="Combat")
    ENWWeaponType GetSecondaryWeapon() const { return SecondaryWeapon; }

    UFUNCTION(BlueprintPure, Category="Combat")
    FString GetActiveWeaponName() const;

    UFUNCTION(BlueprintPure, Category="Combat")
    ENWArrowElement GetArrowElement() const { return ActiveArrowElement; }

    UFUNCTION(BlueprintPure, Category="Combat")
    FString GetArrowElementLabel() const;

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetHealth() const { return Health; }

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetStamina() const { return Stamina; }

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetMaxStamina() const { return MaxStamina; }

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetAbilityCooldownRemaining(int32 AbilityIndex) const;

    UFUNCTION(BlueprintPure, Category="Combat")
    FString GetCombatStateLabel() const;

    UFUNCTION(BlueprintPure, Category="Equipment")
    TArray<FNWGeneratedItem> GetEquippedItems() const { return EquippedItems; }

    UFUNCTION(BlueprintPure, Category="Equipment")
    TArray<FNWGeneratedItem> GetEquippedWeaponItems() const { return EquippedWeaponItems; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    TArray<FNWGeneratedItem> GetInventoryItems() const { return InventoryItems; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    int32 GetSelectedInventoryIndex() const { return SelectedInventoryIndex; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    FString GetNearbyLootLabel() const;

    UFUNCTION(BlueprintPure, Category="Travel")
    FString GetSelectedFastTravelLabel() const;

    UFUNCTION(BlueprintPure, Category="Equipment")
    bool IsBrutalTransformationActive() const { return bBrutalTransformationActive; }

    UFUNCTION(BlueprintPure, Category="Equipment")
    float GetBrutalTransformationRemaining() const;

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

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float BaseMaxStamina = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Health, VisibleAnywhere, Category="Combat")
    float Health = 100.0f;

    UPROPERTY(Replicated, VisibleAnywhere, Category="Combat")
    float Stamina = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Loadout, VisibleAnywhere, Category="Combat")
    ENWWeaponType PrimaryWeapon = ENWWeaponType::Greatsword;

    UPROPERTY(ReplicatedUsing=OnRep_Loadout, VisibleAnywhere, Category="Combat")
    ENWWeaponType SecondaryWeapon = ENWWeaponType::Staff;

    UPROPERTY(ReplicatedUsing=OnRep_Loadout, VisibleAnywhere, Category="Combat")
    ENWWeaponType ActiveWeapon = ENWWeaponType::Greatsword;

    UPROPERTY(Replicated, VisibleAnywhere, Category="Combat")
    ENWArrowElement ActiveArrowElement = ENWArrowElement::Physical;

    UPROPERTY(Replicated, VisibleAnywhere, Category="Combat")
    ENWCombatState CombatState = ENWCombatState::Normal;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float ComboWindowSeconds = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float CrossWeaponComboMultiplier = 1.18f;

    UPROPERTY(ReplicatedUsing=OnRep_Equipment, VisibleAnywhere, Category="Equipment")
    TArray<FNWGeneratedItem> EquippedItems;

    UPROPERTY(ReplicatedUsing=OnRep_Equipment, VisibleAnywhere, Category="Equipment")
    TArray<FNWGeneratedItem> EquippedWeaponItems;

    UPROPERTY(ReplicatedUsing=OnRep_Inventory, VisibleAnywhere, Category="Inventory")
    TArray<FNWGeneratedItem> InventoryItems;

    UPROPERTY(Replicated, VisibleAnywhere, Category="Combat")
    TArray<float> AbilityReadyTimes;

    UPROPERTY(ReplicatedUsing=OnRep_BrutalTransformation, VisibleAnywhere, Category="Equipment")
    bool bBrutalTransformationActive = false;

    UFUNCTION()
    void OnRep_Health();

    UFUNCTION()
    void OnRep_Loadout();

    UFUNCTION()
    void OnRep_Equipment();

    UFUNCTION()
    void OnRep_Inventory();

    UFUNCTION()
    void OnRep_BrutalTransformation();

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

    UFUNCTION(Server, Reliable)
    void ServerSetBlocking(bool bBlocking);

    UFUNCTION(Server, Reliable)
    void ServerDodge(FVector_NetQuantizeNormal Direction);

    UFUNCTION(Server, Reliable)
    void ServerPickupNearestLoot();

    UFUNCTION(Server, Reliable)
    void ServerEquipInventoryItem(int32 ItemSeed);

    UFUNCTION(Server, Reliable)
    void ServerCycleArrowElement();

    UFUNCTION(Server, Reliable)
    void ServerFastTravel(int32 DestinationIndex);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayCombatAnimation(uint8 ActionCode);

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
    void StartBlock();
    void StopBlock();
    void Dodge();
    void PickupNearestLoot();
    void ToggleInventory();
    void InventoryPrevious();
    void InventoryNext();
    void EquipSelectedInventoryItem();
    void CycleArrowElement();
    void CycleFastTravelDestination();
    void ConfirmFastTravel();

    void ExecuteAttack();
    void ExecuteAbility(uint8 AbilityIndex);
    void ExecuteDamageAbility(const FNWWeaponAbilityDefinition& Ability, float ComboMultiplier);
    void ExecuteSetBlocking(bool bBlocking);
    void ExecuteDodge(const FVector& Direction);
    void SetActiveWeaponInternal(ENWWeaponType RequestedWeapon);
    void ApplySprintState(bool bSprinting);
    void ApplyWeaponDamage(AActor* Target, float BaseDamage, bool bAllowStatusEffects, bool bFromAbility = false);
    void ApplyStatusDamage(AActor* Target, float DamagePerTick, int32 Ticks, float IntervalSeconds);
    void ApplyShockChain(AActor* PrimaryTarget, float BaseDamage, float OverrideMultiplier = 0.0f);
    void ApplyFrostbite(AActor* Target, float MinimumMagnitude = 0.0f);
    void ApplyElementalArrow(AActor* Target, float Damage);
    float GetTargetHealthRatio(AActor* Target) const;

    void EquipInventoryItemBySeed(int32 ItemSeed);
    void EquipWeaponItem(const FNWGeneratedItem& NewItem);
    void ConsumeItem(const FNWGeneratedItem& Item);
    void ActivateBrutalTransformation();
    void EndBrutalTransformation();
    void SortInventory();
    void EnsureStarterEquipment();
    void RecalculateEquipmentStats();
    void ApplyWeaponPassiveStats(ENWWeaponType WeaponType);
    float GetAffixTotal(ENWAffixType AffixType) const;
    bool HasAffix(ENWAffixType AffixType) const;
    float RollDamageWithStats(float BaseDamage, bool& bOutCritical) const;
    void LogLoadout() const;

    void EnsureCombatHUD();
    void TryApplyLicensedCharacterVisual();
    void PlayCombatAnimationLocal(uint8 ActionCode);
    bool TryPlayCompatibleSequence(const TArray<FString>& Keywords, float PlayRate = 1.0f);
    bool PlaySequenceByPath(const TCHAR* ObjectPath, float PlayRate = 1.0f);

    float MaxHealth = 100.0f;
    float MaxStamina = 100.0f;
    float DamageMultiplier = 1.0f;
    float HealingMultiplier = 1.0f;
    float CooldownMultiplier = 1.0f;
    float PrecisionChance = 0.0f;
    float LifeStealPercent = 0.0f;
    float ArmorValue = 0.0f;
    float PoisonDamagePerTick = 0.0f;
    float FireDamagePerTick = 0.0f;
    float FrostbiteMagnitude = 0.0f;
    float ShockChainMagnitude = 0.0f;
    float AbilityEchoMagnitude = 0.0f;
    float CooldownOnCritMagnitude = 0.0f;
    float GuardMagnitude = 0.0f;
    float ParryHealMagnitude = 0.0f;
    float DodgeEmpowerMagnitude = 0.0f;
    float BleedDamagePerTick = 0.0f;
    float ExecutionerMagnitude = 0.0f;
    float PassiveRangeMultiplier = 1.0f;
    float PassiveStatusMultiplier = 1.0f;

    float LastAbilityTime = -1000.0f;
    ENWWeaponType LastAbilityWeapon = ENWWeaponType::Greatsword;
    float ParryWindowEndTime = -1000.0f;
    float InvulnerableUntilTime = -1000.0f;
    float CombatStateEndTime = -1000.0f;
    float DodgeEmpowerEndTime = -1000.0f;
    float LastDamageTime = -1000.0f;
    float LastFastTravelTime = -1000.0f;
    float BrutalTransformationEndTime = -1000.0f;
    int32 AttackAnimationIndex = 0;

    int32 SelectedInventoryIndex = 0;
    int32 SelectedFastTravelIndex = 0;
    float PickupRadius = 260.0f;

    FTimerHandle BrutalTransformationTimer;

    UPROPERTY(Transient)
    TObjectPtr<UNWCombatHUDWidget> CombatHUD;
};