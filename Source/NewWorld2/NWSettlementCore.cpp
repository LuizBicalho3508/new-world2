#include "NWSettlementCore.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ANWSettlementCore::ANWSettlementCore()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);

    CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
    SetRootComponent(CoreMesh);
    CoreMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CoreMesh->SetRelativeScale3D(FVector(1.8f, 1.8f, 4.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        CoreMesh->SetStaticMesh(CubeMesh.Object);
    }
}

void ANWSettlementCore::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        Health = MaxHealth;
    }
}

float ANWSettlementCore::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (!HasAuthority() || AppliedDamage <= 0.0f)
    {
        return AppliedDamage;
    }

    Health = FMath::Clamp(Health - AppliedDamage, 0.0f, MaxHealth);
    if (Health <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[INVASAO] Assentamento %d foi rompido. O nucleo sera restaurado para manter o teste continuo."), SettlementIndex);
        Health = MaxHealth;
    }

    return AppliedDamage;
}

void ANWSettlementCore::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWSettlementCore, Health);
    DOREPLIFETIME(ANWSettlementCore, SettlementIndex);
}

void ANWSettlementCore::OnRep_Health()
{
}
