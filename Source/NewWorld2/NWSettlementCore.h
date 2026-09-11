#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWSettlementCore.generated.h"

class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWSettlementCore : public AActor
{
    GENERATED_BODY()

public:
    ANWSettlementCore();

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void SetSettlementIndex(int32 InIndex) { SettlementIndex = InIndex; }

    UFUNCTION(BlueprintPure, Category="Settlement")
    float GetHealth() const { return Health; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Visual")
    TObjectPtr<UStaticMeshComponent> CoreMesh;

    UPROPERTY(EditDefaultsOnly, Category="Settlement")
    float MaxHealth = 650.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Health, VisibleAnywhere, Category="Settlement")
    float Health = 650.0f;

    UPROPERTY(Replicated, VisibleAnywhere, Category="Settlement")
    int32 SettlementIndex = 0;

    UFUNCTION()
    void OnRep_Health();
};
