#include "NWLootPickup.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"
#include "UObject/ConstructorHelpers.h"

ANWLootPickup::ANWLootPickup()
{
    PrimaryActorTick.bCanEverTick = true;
    // Movimento visual de loot nao precisa atualizar a 33 Hz. 15 Hz reduz custo
    // quando varios drops ficam no chao sem alterar colisao/interacao.
    PrimaryActorTick.TickInterval = 0.066f;
    bReplicates = true;
    SetReplicateMovement(true);

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    SetRootComponent(InteractionSphere);
    InteractionSphere->InitSphereRadius(75.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    LootMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootMesh"));
    LootMesh->SetupAttachment(InteractionSphere);
    LootMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LootMesh->SetRelativeScale3D(FVector(0.28f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (MeshAsset.Succeeded()) { LootMesh->SetStaticMesh(MeshAsset.Object); }

    RarityLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RarityLight"));
    RarityLight->SetupAttachment(InteractionSphere);
    RarityLight->SetIntensity(2200.0f);
    RarityLight->SetAttenuationRadius(260.0f);
    RarityLight->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(InteractionSphere);
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetVerticalAlignment(EVRTA_TextCenter);
    Label->SetWorldSize(24.0f);
    Label->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
    Label->SetTextRenderColor(FColor::White);

    InitialLifeSpan = 180.0f;
}

void ANWLootPickup::BeginPlay()
{
    Super::BeginPlay();
    RefreshVisuals();
}

void ANWLootPickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    LifeSeconds += DeltaSeconds;
    const float FloatOffset = FMath::Sin(LifeSeconds * 2.0f) * 9.0f;
    LootMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 42.0f + FloatOffset));
    LootMesh->AddLocalRotation(FRotator(0.0f, DeltaSeconds * 42.0f, 0.0f));
}

void ANWLootPickup::InitializeLoot(const FNWGeneratedItem& InItem)
{
    if (!HasAuthority()) { return; }
    Item = InItem;
    RefreshVisuals();
    ForceNetUpdate();
}

bool ANWLootPickup::TryPickup(ANWCharacter* Character)
{
    if (!HasAuthority() || !Character) { return false; }
    if (!Character->TryAddInventoryItem(Item)) { return false; }
    Destroy();
    return true;
}

void ANWLootPickup::OnRep_Item() { RefreshVisuals(); }

void ANWLootPickup::RefreshVisuals()
{
    const FLinearColor Color = NWCombat::RarityColor(Item.Rarity);
    RarityLight->SetLightColor(Color);
    Label->SetText(FText::FromString(Item.Name.IsEmpty() ? TEXT("Loot") : Item.Name));
    Label->SetTextRenderColor(Color.ToFColor(true));

    const float RarityScale = 0.24f + static_cast<int32>(Item.Rarity) * 0.025f;
    LootMesh->SetRelativeScale3D(FVector(RarityScale));
}

void ANWLootPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWLootPickup, Item);
}
