#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "NWContentPresentationManager.h"
#include "NWRealisticContentPresentationManager.generated.h"

class ANWCharacter;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class NEWORLD2_API ANWRealisticContentPresentationManager : public ANWContentPresentationManager
{
    GENERATED_BODY()

public:
    ANWRealisticContentPresentationManager();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void ScanRealisticAssets();
    void ReportRealisticContent() const;
    void ApplyRealisticPriority(ANWCharacter* Character);
    void ApplyRealisticWeaponPriority(ANWCharacter* Character);
    void ApplyRealisticArmorPriority(ANWCharacter* Character);

    UStaticMesh* FindBestRealisticStaticMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const;
    USkeletalMesh* FindBestCompatibleRealisticArmor(ANWCharacter* Character, const FNWGeneratedItem& Item) const;

    int32 ScoreRealism(const FAssetData& Asset, const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, bool& bPrimaryMatch) const;
    FName GetActiveWeaponStyleId(const ANWCharacter* Character) const;
    TArray<FString> GetWeaponKeywords(ENWWeaponType WeaponType) const;
    TArray<FString> GetArmorSlotKeywords(ENWEquipmentSlot Slot) const;
    UStaticMeshComponent* FindPresentationComponent(ANWCharacter* Character, const FString& NameFragment) const;

    TArray<FAssetData> StaticMeshAssets;
    TArray<FAssetData> SkeletalMeshAssets;
    float PriorityAccumulator = 0.0f;
};
