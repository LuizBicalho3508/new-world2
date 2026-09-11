#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWLootPickup.generated.h"

class ANWCharacter;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class NEWORLD2_API ANWLootPickup : public AActor
{
    GENERATED_BODY()

public:
    ANWLootPickup();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void InitializeLoot(const FNWGeneratedItem& InItem);
    bool TryPickup(ANWCharacter* Character);

    UFUNCTION(BlueprintPure, Category="Loot")
    const FNWGeneratedItem& GetItem() const { return Item; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Loot")
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(VisibleAnywhere, Category="Loot")
    TObjectPtr<UStaticMeshComponent> LootMesh;

    UPROPERTY(VisibleAnywhere, Category="Loot")
    TObjectPtr<UPointLightComponent> RarityLight;

    UPROPERTY(VisibleAnywhere, Category="Loot")
    TObjectPtr<UTextRenderComponent> Label;

    UPROPERTY(ReplicatedUsing=OnRep_Item, VisibleAnywhere, Category="Loot")
    FNWGeneratedItem Item;

    UFUNCTION()
    void OnRep_Item();

private:
    void RefreshVisuals();
    float BaseZ = 0.0f;
    float LifeSeconds = 0.0f;
};
