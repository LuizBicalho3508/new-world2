#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameFramework/Actor.h"
#include "NWCombatTypes.h"
#include "NWContentPresentationManager.generated.h"

class ANWCharacter;
class ANWEnemy;
class UAnimMontage;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWContentPresentationManager : public AActor
{
    GENERATED_BODY()

public:
    ANWContentPresentationManager();

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    struct FPlayerVisualState
    {
        TWeakObjectPtr<UStaticMeshComponent> RightWeapon;
        TWeakObjectPtr<UStaticMeshComponent> LeftWeapon;
        TMap<uint8, TWeakObjectPtr<USkeletalMeshComponent>> ArmorParts;
        TWeakObjectPtr<UNiagaraComponent> BrutalAura;
        TWeakObjectPtr<UPointLightComponent> BrutalLight;
        TWeakObjectPtr<UAnimMontage> LastMontage;
        ENWWeaponType LastWeapon = ENWWeaponType::Greatsword;
        FName LastStyleId = NAME_None;
        uint32 ArmorSignature = 0;
        bool bLastBrutalState = false;
        bool bInitialized = false;
    };

    void ScanProjectAssets();
    void ReportDetectedLibraryContent();
    void UpdateCharacters();
    void UpdateEnemies();
    void UpdateCharacterPresentation(ANWCharacter* Character, FPlayerVisualState& State);
    void ApplyWeaponPresentation(ANWCharacter* Character, FPlayerVisualState& State, ENWWeaponType WeaponType, FName StyleId);
    void ApplyArmorPresentation(ANWCharacter* Character, FPlayerVisualState& State);
    void UpdateBrutalPresentation(ANWCharacter* Character, FPlayerVisualState& State);
    void DetectAndSpawnBowPresentation(ANWCharacter* Character, FPlayerVisualState& State);
    void SpawnBowProjectilePresentation(ANWCharacter* Character, float YawOffsetDegrees, float PitchOffsetDegrees = 0.0f);
    void ApplyEnemyPresentation(ANWEnemy* Enemy);

    UStaticMesh* FindBestStaticMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords = {}) const;
    USkeletalMesh* FindBestSkeletalMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords = {}, bool bRequireAnimation = false, UClass** OutAnimationClass = nullptr) const;
    USkeletalMesh* FindCompatibleArmorMesh(ANWCharacter* Character, const FNWGeneratedItem& Item) const;
    UClass* FindAnimationClassForMesh(USkeletalMesh* Mesh, const TArray<FString>& PreferredKeywords = {}) const;
    UNiagaraSystem* FindBestNiagara(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords = {}) const;

    UStaticMeshComponent* EnsureStaticVisualComponent(ANWCharacter* Character, TWeakObjectPtr<UStaticMeshComponent>& Existing, const FName BaseName) const;
    USkeletalMeshComponent* EnsureArmorVisualComponent(ANWCharacter* Character, FPlayerVisualState& State, ENWEquipmentSlot Slot) const;
    UNiagaraComponent* EnsureBrutalAura(ANWCharacter* Character, FPlayerVisualState& State) const;
    UPointLightComponent* EnsureBrutalLight(ANWCharacter* Character, FPlayerVisualState& State) const;

    FName FindHandSocket(ANWCharacter* Character, bool bRightHand) const;
    FName GetActiveWeaponStyleId(const ANWCharacter* Character) const;
    uint32 BuildArmorSignature(const ANWCharacter* Character) const;
    TArray<FString> GetWeaponMeshKeywords(ENWWeaponType WeaponType) const;
    TArray<FString> GetArrowElementKeywords(ENWArrowElement Element) const;
    TArray<FString> GetArmorSlotKeywords(ENWEquipmentSlot Slot) const;
    bool TryLoadParagonHero(const FString& HeroName, USkeletalMesh*& OutMesh, UClass*& OutAnimClass) const;
    bool HasAssetKeywords(const TArray<FString>& Keywords) const;
    int32 ScoreAsset(const FAssetData& Asset, const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, bool& bPrimaryMatch) const;
    void HideEnemyDebugMeshes(ANWEnemy* Enemy) const;
    void CleanupDeadState();

    TArray<FAssetData> StaticMeshAssets;
    TArray<FAssetData> SkeletalMeshAssets;
    TArray<FAssetData> AnimBlueprintAssets;
    TArray<FAssetData> NiagaraAssets;

    TMap<TWeakObjectPtr<ANWCharacter>, FPlayerVisualState> PlayerVisualStates;
    TMap<TWeakObjectPtr<ANWEnemy>, int32> EnemyVisualSignatures;

    TWeakObjectPtr<UStaticMesh> CachedArrowMesh;
    TMap<uint8, TWeakObjectPtr<UNiagaraSystem>> CachedArrowTrails;

    float CleanupAccumulator = 0.0f;
};
