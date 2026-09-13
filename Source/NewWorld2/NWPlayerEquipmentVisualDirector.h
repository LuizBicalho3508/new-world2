#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWPlayerEquipmentVisualDirector.generated.h"

class ANWCharacter;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Visual de equipamento do jogador, separado do diretor de mobs.
 * Prioriza armor modular com o mesmo Skeleton. Se o pack Fab disponibilizar
 * somente pecas StaticMesh, usa fallback seguro preso aos bones do personagem.
 * Cloth e skins arbitrarias continuam bloqueados para preservar estabilidade.
 */
UCLASS()
class NEWORLD2_API ANWPlayerEquipmentVisualDirector : public AActor
{
    GENERATED_BODY()

public:
    ANWPlayerEquipmentVisualDirector();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    struct FPlayerGearState
    {
        ENWWeaponType LastWeapon = ENWWeaponType::Greatsword;
        FName LastWeaponStyle = NAME_None;
        uint32 LastArmorSignature = 0;
        bool bInitialized = false;
        TWeakObjectPtr<UStaticMeshComponent> RightWeapon;
        TWeakObjectPtr<UStaticMeshComponent> LeftWeapon;
        TMap<uint8, TWeakObjectPtr<USkeletalMeshComponent>> ArmorParts;
        TMap<uint8, TWeakObjectPtr<UStaticMeshComponent>> StaticArmorParts;
    };

    void ScanAssets();
    void UpdatePlayers();
    void UpdatePlayer(ANWCharacter* Character, FPlayerGearState& State);
    void UpdateWeapon(ANWCharacter* Character, FPlayerGearState& State);
    void UpdateArmor(ANWCharacter* Character, FPlayerGearState& State);

    UStaticMesh* FindWeaponMesh(ENWWeaponType Type, FName StyleId) const;
    UStaticMesh* FindShieldMesh(FName StyleId) const;
    USkeletalMesh* FindArmorMesh(ANWCharacter* Character, const FNWGeneratedItem& Item) const;
    UStaticMesh* FindStaticArmorMesh(const FNWGeneratedItem& Item) const;
    USkeletalMesh* FindBaseBodyMesh(ANWCharacter* Character) const;

    UStaticMeshComponent* EnsureWeaponComponent(ANWCharacter* Character, TWeakObjectPtr<UStaticMeshComponent>& Existing, const FName Name) const;
    USkeletalMeshComponent* EnsureArmorComponent(ANWCharacter* Character, FPlayerGearState& State, ENWEquipmentSlot Slot) const;
    UStaticMeshComponent* EnsureStaticArmorComponent(ANWCharacter* Character, FPlayerGearState& State, ENWEquipmentSlot Slot) const;
    FName FindHandSocket(ANWCharacter* Character, bool bRight) const;
    FName FindArmorAttachPoint(ANWCharacter* Character, ENWEquipmentSlot Slot) const;
    FTransform GetStaticArmorRelativeTransform(ENWEquipmentSlot Slot, UStaticMesh* Mesh) const;
    float ComputeStaticScale(UStaticMesh* Mesh, float TargetDimension) const;
    float WeaponTargetDimension(ENWWeaponType Type, bool bRight) const;
    uint32 BuildArmorSignature(const ANWCharacter* Character) const;
    FName GetActiveWeaponStyle(const ANWCharacter* Character) const;
    bool IsUnsafePath(const FString& Path) const;

    TArray<FAssetData> StaticAssets;
    TArray<FAssetData> SkeletalAssets;
    TMap<TWeakObjectPtr<ANWCharacter>, FPlayerGearState> States;
    TSet<TWeakObjectPtr<ANWCharacter>> BaseBodyAttempted;
};