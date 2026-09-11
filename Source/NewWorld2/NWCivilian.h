#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NWCivilian.generated.h"

class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWCivilian : public ACharacter
{
    GENERATED_BODY()

public:
    ANWCivilian();

    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void SetHomeLocation(const FVector& InHomeLocation);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Visual")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(EditDefaultsOnly, Category="NPC")
    float MaxHealth = 80.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Health, VisibleAnywhere, Category="NPC")
    float Health = 80.0f;

    UFUNCTION()
    void OnRep_Health();

private:
    FVector HomeLocation = FVector::ZeroVector;
    FVector WanderDirection = FVector::ForwardVector;
    float WanderRadius = 900.0f;
    float MoveSpeed = 95.0f;
    float NextDirectionChangeTime = 0.0f;
};
