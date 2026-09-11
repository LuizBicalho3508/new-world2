#include "NWRealisticContentPresentationManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"

ANWRealisticContentPresentationManager::ANWRealisticContentPresentationManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.10f;
}

void ANWRealisticContentPresentationManager::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { return; }

    ScanRealisticAssets();
    ReportRealisticContent();
}

void ANWRealisticContentPresentationManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetNetMode() == NM_DedicatedServer || !GetWorld()) { return; }

    PriorityAccumulator += DeltaSeconds;
    if (PriorityAccumulator < 0.25f) { return; }
    PriorityAccumulator = 0.0f;

    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It)) { ApplyRealisticPriority(*It); }
    }
}

void ANWRealisticContentPresentationManager::ScanRealisticAssets()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    FARFilter StaticFilter;
    StaticFilter.PackagePaths.Add(FName(TEXT("/Game")));
    StaticFilter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    StaticFilter.bRecursivePaths = true;
    Registry.GetAssets(StaticFilter, StaticMeshAssets);

    FARFilter SkeletalFilter;
    SkeletalFilter.PackagePaths.Add(FName(TEXT("/Game")));
    SkeletalFilter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
    SkeletalFilter.bRecursivePaths = true;
    Registry.GetAssets(SkeletalFilter, SkeletalMeshAssets);

    UE_LOG(LogTemp, Warning, TEXT("[REALISMO] Catalogo pronto: StaticMesh=%d | SkeletalMesh=%d"), StaticMeshAssets.Num(), SkeletalMeshAssets.Num());
}

void ANWRealisticContentPresentationManager::ReportRealisticContent() const
{
    auto ContainsAny = [this](const TArray<FString>& Terms)
    {
        for (const FAssetData& Asset : StaticMeshAssets)
        {
            const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
            for (const FString& Term : Terms)
            {
                if (Searchable.Contains(Term.ToLower())) { return true; }
            }
        }
        for (const FAssetData& Asset : SkeletalMeshAssets)
        {
            const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
            for (const FString& Term : Terms)
            {
                if (Searchable.Contains(Term.ToLower())) { return true; }
            }
        }
        return false;
    };

    UE_LOG(LogTemp, Display, TEXT("[REALISMO] Classic Medieval Knight : %s"), ContainsAny({ TEXT("ClassicMedieval"), TEXT("MedievalKnight"), TEXT("KnightWarrior") }) ? TEXT("DETECTADO") : TEXT("nao instalado em /Game"));
    UE_LOG(LogTemp, Display, TEXT("[REALISMO] Medieval King          : %s"), ContainsAny({ TEXT("MedievalKing"), TEXT("Medieval_King") }) ? TEXT("DETECTADO") : TEXT("nao instalado em /Game"));
    UE_LOG(LogTemp, Display, TEXT("[REALISMO] Wasteland Warrior      : %s"), ContainsAny({ TEXT("WastelandWarrior"), TEXT("Wasteland_Warrior") }) ? TEXT("DETECTADO") : TEXT("nao instalado em /Game"));
    UE_LOG(LogTemp, Display, TEXT("[REALISMO] Paragon Serath         : %s"), ContainsAny({ TEXT("ParagonSerath"), TEXT("Serath") }) ? TEXT("DETECTADO") : TEXT("nao instalado em /Game"));
    UE_LOG(LogTemp, Display, TEXT("[REALISMO] armas PBR/realistas    : %s"), ContainsAny({ TEXT("Realistic"), TEXT("PBR"), TEXT("ShortSword"), TEXT("MedievalDagger"), TEXT("Atris"), TEXT("Recurve") }) ? TEXT("DETECTADAS") : TEXT("usar fallback"));
}

void ANWRealisticContentPresentationManager::ApplyRealisticPriority(ANWCharacter* Character)
{
    if (!Character || !Character->GetMesh()) { return; }
    ApplyRealisticWeaponPriority(Character);
    ApplyRealisticArmorPriority(Character);
}

void ANWRealisticContentPresentationManager::ApplyRealisticWeaponPriority(ANWCharacter* Character)
{
    const ENWWeaponType WeaponType = Character->GetActiveWeapon();
    const FName StyleId = GetActiveWeaponStyleId(Character);

    TArray<FString> Preferred = {
        TEXT("Realistic"), TEXT("PBR"), TEXT("4K"), TEXT("Medieval"), TEXT("Steel"), TEXT("Iron"),
        TEXT("Historical"), TEXT("GameReady"), TEXT("Game_Ready"), TEXT("AAA")
    };
    if (!StyleId.IsNone()) { Preferred.Add(StyleId.ToString()); }

    switch (WeaponType)
    {
        case ENWWeaponType::Greatsword:
        case ENWWeaponType::DualSwords:
        case ENWWeaponType::SwordShield:
            Preferred.Append({ TEXT("FreeSword"), TEXT("ShortSword"), TEXT("Short_Sword"), TEXT("Atris"), TEXT("LongSword"), TEXT("Long_Sword") });
            break;
        case ENWWeaponType::Daggers:
            Preferred.Append({ TEXT("MedievalDagger"), TEXT("Medieval_Dagger"), TEXT("Dagger"), TEXT("Daggers") });
            break;
        case ENWWeaponType::Bow:
            Preferred.Append({ TEXT("Ethereal"), TEXT("Recurve"), TEXT("RealisticBow"), TEXT("Longbow") });
            break;
        default:
            break;
    }

    UStaticMesh* BetterMesh = FindBestRealisticStaticMesh(GetWeaponKeywords(WeaponType), Preferred);
    if (!BetterMesh) { return; }

    UStaticMeshComponent* Right = FindPresentationComponent(Character, TEXT("NW_WeaponVisual_R"));
    UStaticMeshComponent* Left = FindPresentationComponent(Character, TEXT("NW_WeaponVisual_L"));

    if (Right && Right->GetStaticMesh() != BetterMesh)
    {
        Right->SetStaticMesh(BetterMesh);
        Right->SetVisibility(true, true);
        UE_LOG(LogTemp, Display, TEXT("[REALISMO-ARMA] %s priorizada: %s"), *NWCombat::WeaponTypeToString(WeaponType), *BetterMesh->GetPathName());
    }

    if ((WeaponType == ENWWeaponType::DualSwords || WeaponType == ENWWeaponType::Daggers) && Left)
    {
        Left->SetStaticMesh(BetterMesh);
        Left->SetVisibility(true, true);
    }

    if (WeaponType == ENWWeaponType::SwordShield && Left)
    {
        UStaticMesh* ShieldMesh = FindBestRealisticStaticMesh(
            { TEXT("Shield"), TEXT("Buckler"), TEXT("Aegis") },
            { TEXT("Realistic"), TEXT("PBR"), TEXT("4K"), TEXT("Medieval"), TEXT("Iron"), TEXT("Wood"), TEXT("Viking"), TEXT("Knight") });
        if (ShieldMesh)
        {
            Left->SetStaticMesh(ShieldMesh);
            Left->SetVisibility(true, true);
        }
    }
}

void ANWRealisticContentPresentationManager::ApplyRealisticArmorPriority(ANWCharacter* Character)
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset()) { return; }

    TArray<USkeletalMeshComponent*> Components;
    Character->GetComponents<USkeletalMeshComponent>(Components);

    for (const FNWGeneratedItem& Item : Character->GetEquippedItems())
    {
        if (Item.Kind != ENWItemKind::Armor) { continue; }

        USkeletalMesh* BetterMesh = FindBestCompatibleRealisticArmor(Character, Item);
        if (!BetterMesh) { continue; }

        const FString SlotFragment = FString::Printf(TEXT("NW_Armor_%d"), static_cast<int32>(Item.Slot));
        for (USkeletalMeshComponent* Component : Components)
        {
            if (!Component || !Component->GetName().Contains(SlotFragment, ESearchCase::IgnoreCase)) { continue; }
            if (Component->GetSkeletalMeshAsset() != BetterMesh)
            {
                Component->SetSkeletalMeshAsset(BetterMesh);
                Component->SetLeaderPoseComponent(Character->GetMesh(), false, false);
                Component->SetVisibility(true, true);
                UE_LOG(LogTemp, Display, TEXT("[REALISMO-ARMADURA] %s -> %s"), *Item.Name, *BetterMesh->GetPathName());
            }
            break;
        }
    }
}

UStaticMesh* ANWRealisticContentPresentationManager::FindBestRealisticStaticMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* BestMesh = nullptr;

    for (const FAssetData& Asset : StaticMeshAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreRealism(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }

        UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
        if (!Mesh) { continue; }
        BestScore = Score;
        BestMesh = Mesh;
    }

    return BestScore >= 20 ? BestMesh : nullptr;
}

USkeletalMesh* ANWRealisticContentPresentationManager::FindBestCompatibleRealisticArmor(ANWCharacter* Character, const FNWGeneratedItem& Item) const
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset()) { return nullptr; }
    USkeleton* BaseSkeleton = Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
    if (!BaseSkeleton) { return nullptr; }

    TArray<FString> Preferred = {
        TEXT("Realistic"), TEXT("PBR"), TEXT("4K"), TEXT("Medieval"), TEXT("Knight"), TEXT("Plate"),
        TEXT("Chainmail"), TEXT("Steel"), TEXT("Iron"), TEXT("Armor"), TEXT("Armour"), TEXT("Modular")
    };
    if (!Item.StyleId.IsNone()) { Preferred.Add(Item.StyleId.ToString()); }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* BestMesh = nullptr;

    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreRealism(Asset, GetArmorSlotKeywords(Item.Slot), Preferred, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh || Mesh->GetSkeleton() != BaseSkeleton) { continue; }
        BestScore = Score;
        BestMesh = Mesh;
    }

    return BestScore >= 20 ? BestMesh : nullptr;
}

int32 ANWRealisticContentPresentationManager::ScoreRealism(const FAssetData& Asset, const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, bool& bPrimaryMatch) const
{
    const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
    int32 Score = 0;
    bPrimaryMatch = PrimaryKeywords.IsEmpty();

    for (const FString& Keyword : PrimaryKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower()))
        {
            bPrimaryMatch = true;
            Score += 28;
        }
    }

    for (const FString& Keyword : PreferredKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 12; }
    }

    const TArray<TPair<FString, int32>> QualityBoosts = {
        { TEXT("realistic"), 90 }, { TEXT("pbr"), 48 }, { TEXT("4k"), 34 }, { TEXT("cinematic"), 28 },
        { TEXT("medieval"), 24 }, { TEXT("steel"), 18 }, { TEXT("iron"), 18 }, { TEXT("historical"), 18 },
        { TEXT("gameready"), 16 }, { TEXT("game_ready"), 16 }, { TEXT("aaa"), 16 },
        { TEXT("shortsword"), 28 }, { TEXT("short_sword"), 28 }, { TEXT("medievaldagger"), 28 },
        { TEXT("medieval_dagger"), 28 }, { TEXT("atris"), 30 }, { TEXT("ethereal"), 24 },
        { TEXT("recurve"), 24 }, { TEXT("viking"), 18 }, { TEXT("knight"), 18 }, { TEXT("chainmail"), 22 }
    };
    for (const TPair<FString, int32>& Entry : QualityBoosts)
    {
        if (Searchable.Contains(Entry.Key)) { Score += Entry.Value; }
    }

    const TArray<TPair<FString, int32>> Penalties = {
        { TEXT("lowpoly"), -320 }, { TEXT("low_poly"), -320 }, { TEXT("low-poly"), -320 },
        { TEXT("stylized"), -300 }, { TEXT("stylised"), -300 }, { TEXT("cartoon"), -360 },
        { TEXT("toon"), -360 }, { TEXT("chibi"), -400 }, { TEXT("voxel"), -400 }, { TEXT("pixel"), -250 }
    };
    for (const TPair<FString, int32>& Entry : Penalties)
    {
        if (Searchable.Contains(Entry.Key)) { Score += Entry.Value; }
    }

    return Score;
}

FName ANWRealisticContentPresentationManager::GetActiveWeaponStyleId(const ANWCharacter* Character) const
{
    if (!Character) { return NAME_None; }
    for (const FNWGeneratedItem& Item : Character->GetEquippedWeaponItems())
    {
        if (Item.Kind == ENWItemKind::Weapon && Item.WeaponType == Character->GetActiveWeapon()) { return Item.StyleId; }
    }
    return NAME_None;
}

TArray<FString> ANWRealisticContentPresentationManager::GetWeaponKeywords(ENWWeaponType WeaponType) const
{
    switch (WeaponType)
    {
        case ENWWeaponType::Staff: return { TEXT("Staff"), TEXT("Rod"), TEXT("Wand"), TEXT("Scepter") };
        case ENWWeaponType::Greatsword: return { TEXT("Greatsword"), TEXT("Great_Sword"), TEXT("LongSword"), TEXT("Long_Sword"), TEXT("Sword") };
        case ENWWeaponType::DualSwords: return { TEXT("Sword"), TEXT("Sabre"), TEXT("Saber"), TEXT("Blade") };
        case ENWWeaponType::SwordShield: return { TEXT("Sword"), TEXT("Sabre"), TEXT("Saber"), TEXT("Blade") };
        case ENWWeaponType::Daggers: return { TEXT("Dagger"), TEXT("Knife"), TEXT("ShortBlade"), TEXT("Short_Blade") };
        case ENWWeaponType::Bow: return { TEXT("Bow"), TEXT("Recurve"), TEXT("Longbow") };
        case ENWWeaponType::Firearm: return { TEXT("Musket"), TEXT("Rifle"), TEXT("Gun"), TEXT("Firearm") };
        default: return { TEXT("Weapon") };
    }
}

TArray<FString> ANWRealisticContentPresentationManager::GetArmorSlotKeywords(ENWEquipmentSlot Slot) const
{
    switch (Slot)
    {
        case ENWEquipmentSlot::Head: return { TEXT("Helmet"), TEXT("Helm"), TEXT("Head") };
        case ENWEquipmentSlot::Chest: return { TEXT("Chest"), TEXT("Torso"), TEXT("Cuirass"), TEXT("Breastplate") };
        case ENWEquipmentSlot::Gloves: return { TEXT("Glove"), TEXT("Gauntlet"), TEXT("Hand") };
        case ENWEquipmentSlot::Legs: return { TEXT("Leg"), TEXT("Greave"), TEXT("Pants"), TEXT("Trouser") };
        case ENWEquipmentSlot::Boots: return { TEXT("Boot"), TEXT("Foot"), TEXT("Sabaton") };
        default: return { TEXT("Armor"), TEXT("Armour") };
    }
}

UStaticMeshComponent* ANWRealisticContentPresentationManager::FindPresentationComponent(ANWCharacter* Character, const FString& NameFragment) const
{
    if (!Character) { return nullptr; }
    TArray<UStaticMeshComponent*> Components;
    Character->GetComponents<UStaticMeshComponent>(Components);
    for (UStaticMeshComponent* Component : Components)
    {
        if (Component && Component->GetName().Contains(NameFragment, ESearchCase::IgnoreCase)) { return Component; }
    }
    return nullptr;
}
