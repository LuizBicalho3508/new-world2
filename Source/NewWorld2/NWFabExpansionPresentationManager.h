#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "NWRealisticContentPresentationManager.h"
#include "NWWorldTypes.h"
#include "NWFabExpansionPresentationManager.generated.h"

class ANWCharacter;
class ANWDungeonSite;
class ANWEnemy;
class ANWProceduralWorldManager;
class ANWWorldEventDirector;
class UAnimBlueprint;
class UAudioComponent;
class UNiagaraSystem;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWFabExpansionPresentationManager : public ANWRealisticContentPresentationManager
{
    GENERATED_BODY()

public:
    ANWFabExpansionPresentationManager();

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void ScanExpansionAssets();
    void ReportExpansionAssets() const;
    void UpdatePlayerWeaponSamples();
    void UpdateEnemyVariants();
    void UpdateDungeonDecor();
    void UpdateLocalAudio();
    void TryPlaceDesertRuinsLandmark();

    UStaticMesh* FindBestStaticMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords = {}) const;
    USkeletalMesh* FindBestSkeletalMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, UClass** OutAnimClass) const;
    UClass* FindAnimationClassForMesh(USkeletalMesh* Mesh, const TArray<FString>& PreferredKeywords = {}) const;
    UNiagaraSystem* FindBestNiagara(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords = {}) const;
    USoundBase* FindBestSound(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords = {}) const;
    int32 ScoreAsset(const FAssetData& Asset, const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, bool& bPrimaryMatch) const;
    bool ContainsAnyAsset(const TArray<FString>& Keywords) const;

    UStaticMeshComponent* FindStaticPresentationComponent(ANWCharacter* Character, const FString& NameFragment) const;
    void HideEnemyFallbackMeshes(ANWEnemy* Enemy) const;
    bool TryLoadTerra(USkeletalMesh*& OutMesh, UClass*& OutAnimClass) const;
    TArray<FString> GetBiomeMusicKeywords(ENWBiomeType Biome) const;

    TArray<FAssetData> StaticMeshAssets;
    TArray<FAssetData> SkeletalMeshAssets;
    TArray<FAssetData> AnimBlueprintAssets;
    TArray<FAssetData> NiagaraAssets;
    TArray<FAssetData> SoundAssets;

    TSet<TWeakObjectPtr<ANWEnemy>> CustomizedEnemies;
    TSet<TWeakObjectPtr<ANWDungeonSite>> DecoratedDungeons;
    TMap<TWeakObjectPtr<ANWCharacter>, ENWBiomeType> PreviousAudioBiomes;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> ActiveBiomeMusic;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> DesertLandmarkCenter;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> DesertLandmarkParts;

    float ExpansionAccumulator = 0.0f;
    bool bDesertLandmarkPlaced = false;
};
