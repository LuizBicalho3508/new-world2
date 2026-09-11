#include "NWContentPresentationManager.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NWArrowPresentationProjectile.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"
#include "NWEnemy.h"

ANWContentPresentationManager::ANWContentPresentationManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.10f;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    NetUpdateFrequency = 1.0f;
}

void ANWContentPresentationManager::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { return; }

    ScanProjectAssets();
    ReportDetectedLibraryContent();
}

void ANWContentPresentationManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetNetMode() == NM_DedicatedServer || !GetWorld()) { return; }

    UpdateCharacters();
    UpdateEnemies();

    CleanupAccumulator += DeltaSeconds;
    if (CleanupAccumulator >= 5.0f)
    {
        CleanupAccumulator = 0.0f;
        CleanupDeadState();
    }
}

void ANWContentPresentationManager::ScanProjectAssets()
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

    FARFilter AnimFilter;
    AnimFilter.PackagePaths.Add(FName(TEXT("/Game")));
    AnimFilter.ClassPaths.Add(UAnimBlueprint::StaticClass()->GetClassPathName());
    AnimFilter.bRecursivePaths = true;
    Registry.GetAssets(AnimFilter, AnimBlueprintAssets);

    FARFilter NiagaraFilter;
    NiagaraFilter.PackagePaths.Add(FName(TEXT("/Game")));
    NiagaraFilter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
    NiagaraFilter.bRecursivePaths = true;
    Registry.GetAssets(NiagaraFilter, NiagaraAssets);

    UE_LOG(LogTemp, Warning, TEXT("[FAB] Catalogo /Game: StaticMesh=%d | SkeletalMesh=%d | AnimBP=%d | Niagara=%d"),
        StaticMeshAssets.Num(), SkeletalMeshAssets.Num(), AnimBlueprintAssets.Num(), NiagaraAssets.Num());
}

void ANWContentPresentationManager::ReportDetectedLibraryContent()
{
    auto ReportKeywords = [this](const TCHAR* Label, const TArray<FString>& Keywords)
    {
        UE_LOG(LogTemp, Display, TEXT("[FAB] %-36s : %s"), Label, HasAssetKeywords(Keywords) ? TEXT("DETECTADO NO PROJETO") : TEXT("ainda nao instalado em /Game"));
    };

    ReportKeywords(TEXT("Free Sword / Melee Weapons"), { TEXT("Sword") });
    ReportKeywords(TEXT("Necromancer Bone Sword"), { TEXT("Necromancer"), TEXT("Sword") });
    ReportKeywords(TEXT("Ethereal Recurve Bow"), { TEXT("Ethereal"), TEXT("Bow") });
    ReportKeywords(TEXT("Medieval Knight Shield"), { TEXT("Shield") });
    ReportKeywords(TEXT("Modular Medieval Armor"), { TEXT("Armor") });

    static const TCHAR* ParagonHeroes[] = { TEXT("Sevarog"), TEXT("Rampage"), TEXT("Khaimera"), TEXT("Countess"), TEXT("Revenant") };
    for (const TCHAR* HeroName : ParagonHeroes)
    {
        USkeletalMesh* Mesh = nullptr;
        UClass* AnimClass = nullptr;
        const FString Hero(HeroName);
        UE_LOG(LogTemp, Display, TEXT("[FAB] Paragon %-27s : %s"), *Hero, TryLoadParagonHero(Hero, Mesh, AnimClass) ? TEXT("DETECTADO") : TEXT("nao instalado"));
    }
}

void ANWContentPresentationManager::UpdateCharacters()
{
    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character)) { continue; }
        FPlayerVisualState& State = PlayerVisualStates.FindOrAdd(Character);
        UpdateCharacterPresentation(Character, State);
    }
}

void ANWContentPresentationManager::UpdateEnemies()
{
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }

        const int32 Signature = static_cast<int32>(Enemy->GetEnemyArchetype()) + (Enemy->IsWorldBoss() ? 100 : 0);
        const int32* PreviousSignature = EnemyVisualSignatures.Find(Enemy);
        if (!PreviousSignature || *PreviousSignature != Signature)
        {
            ApplyEnemyPresentation(Enemy);
            EnemyVisualSignatures.Add(Enemy, Signature);
        }
    }
}

void ANWContentPresentationManager::UpdateCharacterPresentation(ANWCharacter* Character, FPlayerVisualState& State)
{
    if (!Character) { return; }

    const ENWWeaponType ActiveWeapon = Character->GetActiveWeapon();
    const FName StyleId = GetActiveWeaponStyleId(Character);
    if (!State.bInitialized || State.LastWeapon != ActiveWeapon || State.LastStyleId != StyleId)
    {
        ApplyWeaponPresentation(Character, State, ActiveWeapon, StyleId);
        State.LastWeapon = ActiveWeapon;
        State.LastStyleId = StyleId;
    }

    const uint32 ArmorSignature = BuildArmorSignature(Character);
    if (!State.bInitialized || State.ArmorSignature != ArmorSignature)
    {
        ApplyArmorPresentation(Character, State);
        State.ArmorSignature = ArmorSignature;
    }

    if (!State.bInitialized || State.bLastBrutalState != Character->IsBrutalTransformationActive())
    {
        UpdateBrutalPresentation(Character, State);
        State.bLastBrutalState = Character->IsBrutalTransformationActive();
    }

    DetectAndSpawnBowPresentation(Character, State);
    State.bInitialized = true;
}

void ANWContentPresentationManager::ApplyWeaponPresentation(ANWCharacter* Character, FPlayerVisualState& State, ENWWeaponType WeaponType, FName StyleId)
{
    UStaticMeshComponent* Right = EnsureStaticVisualComponent(Character, State.RightWeapon, FName(TEXT("NW_WeaponVisual_R")));
    UStaticMeshComponent* Left = EnsureStaticVisualComponent(Character, State.LeftWeapon, FName(TEXT("NW_WeaponVisual_L")));
    if (!Right || !Left || !Character->GetMesh()) { return; }

    Right->SetVisibility(false, true);
    Left->SetVisibility(false, true);
    Right->SetStaticMesh(nullptr);
    Left->SetStaticMesh(nullptr);

    TArray<FString> Preferred;
    if (!StyleId.IsNone()) { Preferred.Add(StyleId.ToString()); }
    Preferred.Add(TEXT("Weapon"));
    Preferred.Add(TEXT("Melee"));
    Preferred.Add(TEXT("Fantasy"));

    UStaticMesh* PrimaryMesh = FindBestStaticMesh(GetWeaponMeshKeywords(WeaponType), Preferred);
    if (!PrimaryMesh)
    {
        UE_LOG(LogTemp, Display, TEXT("[VISUAL] Sem mesh instalado para %s (%s). Mantendo combate funcional sem arma visual."),
            *NWCombat::WeaponTypeToString(WeaponType), *StyleId.ToString());
        return;
    }

    Right->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FindHandSocket(Character, true));
    Right->SetRelativeTransform(FTransform::Identity);
    Right->SetStaticMesh(PrimaryMesh);
    Right->SetVisibility(true, true);

    if (WeaponType == ENWWeaponType::DualSwords || WeaponType == ENWWeaponType::Daggers)
    {
        Left->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FindHandSocket(Character, false));
        Left->SetRelativeTransform(FTransform::Identity);
        Left->SetStaticMesh(PrimaryMesh);
        Left->SetVisibility(true, true);
    }
    else if (WeaponType == ENWWeaponType::SwordShield)
    {
        UStaticMesh* ShieldMesh = FindBestStaticMesh({ TEXT("Shield"), TEXT("Aegis"), TEXT("Buckler") }, { TEXT("Knight"), TEXT("Medieval"), TEXT("Fantasy") });
        if (ShieldMesh)
        {
            Left->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FindHandSocket(Character, false));
            Left->SetRelativeTransform(FTransform::Identity);
            Left->SetStaticMesh(ShieldMesh);
            Left->SetVisibility(true, true);
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[VISUAL] %s -> %s"), *NWCombat::WeaponTypeToString(WeaponType), *PrimaryMesh->GetPathName());
}

void ANWContentPresentationManager::ApplyArmorPresentation(ANWCharacter* Character, FPlayerVisualState& State)
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset()) { return; }

    TSet<uint8> ActiveSlots;
    for (const FNWGeneratedItem& Item : Character->GetEquippedItems())
    {
        if (Item.Kind != ENWItemKind::Armor) { continue; }
        const uint8 SlotKey = static_cast<uint8>(Item.Slot);
        ActiveSlots.Add(SlotKey);

        USkeletalMeshComponent* Visual = EnsureArmorVisualComponent(Character, State, Item.Slot);
        if (!Visual) { continue; }

        USkeletalMesh* ArmorMesh = FindCompatibleArmorMesh(Character, Item);
        if (!ArmorMesh)
        {
            Visual->SetVisibility(false, true);
            continue;
        }

        Visual->SetSkeletalMeshAsset(ArmorMesh);
        Visual->SetLeaderPoseComponent(Character->GetMesh(), false, false);
        Visual->SetRelativeTransform(FTransform::Identity);
        Visual->SetVisibility(true, true);
        UE_LOG(LogTemp, Display, TEXT("[ARMOR-VISUAL] %s -> %s"), *Item.Name, *ArmorMesh->GetPathName());
    }

    for (auto& Pair : State.ArmorParts)
    {
        if (!ActiveSlots.Contains(Pair.Key) && Pair.Value.IsValid()) { Pair.Value->SetVisibility(false, true); }
    }
}

void ANWContentPresentationManager::UpdateBrutalPresentation(ANWCharacter* Character, FPlayerVisualState& State)
{
    UNiagaraComponent* Aura = EnsureBrutalAura(Character, State);
    UPointLightComponent* Light = EnsureBrutalLight(Character, State);
    if (!Aura || !Light) { return; }

    const bool bActive = Character->IsBrutalTransformationActive();
    if (bActive)
    {
        if (!Aura->GetAsset())
        {
            Aura->SetAsset(FindBestNiagara(
                { TEXT("Aura"), TEXT("Power"), TEXT("Rage"), TEXT("Energy"), TEXT("Buff") },
                { TEXT("Legendary"), TEXT("Brutal"), TEXT("Fire"), TEXT("Lightning") }));
        }
        if (Aura->GetAsset()) { Aura->Activate(true); }
        Light->SetLightColor(FLinearColor(1.0f, 0.14f, 0.02f));
        Light->SetIntensity(5200.0f);
        Character->GetMesh()->SetRelativeScale3D(FVector(1.08f));
    }
    else
    {
        Aura->Deactivate();
        Light->SetIntensity(0.0f);
        Character->GetMesh()->SetRelativeScale3D(FVector(1.0f));
    }
}

void ANWContentPresentationManager::DetectAndSpawnBowPresentation(ANWCharacter* Character, FPlayerVisualState& State)
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetAnimInstance()) { return; }

    UAnimMontage* CurrentMontage = Character->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage();
    if (!CurrentMontage)
    {
        State.LastMontage.Reset();
        return;
    }

    if (State.LastMontage.Get() == CurrentMontage) { return; }
    State.LastMontage = CurrentMontage;

    if (Character->GetActiveWeapon() != ENWWeaponType::Bow) { return; }

    const FString Name = CurrentMontage->GetName();
    if (Name.Contains(TEXT("Attack_Primary"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Ability_Q"), ESearchCase::IgnoreCase))
    {
        SpawnBowProjectilePresentation(Character, 0.0f);
    }
    else if (Name.Contains(TEXT("Ability_E"), ESearchCase::IgnoreCase))
    {
        static const float Spread[] = { -10.0f, -5.0f, 0.0f, 5.0f, 10.0f };
        for (const float YawOffset : Spread)
        {
            SpawnBowProjectilePresentation(Character, YawOffset, 5.0f);
        }
    }
}

void ANWContentPresentationManager::SpawnBowProjectilePresentation(ANWCharacter* Character, float YawOffsetDegrees, float PitchOffsetDegrees)
{
    if (!Character || !GetWorld()) { return; }

    if (!CachedArrowMesh.IsValid())
    {
        CachedArrowMesh = FindBestStaticMesh({ TEXT("Arrow"), TEXT("Projectile") }, { TEXT("Ethereal"), TEXT("Recurve"), TEXT("Bow"), TEXT("Weapon") });
    }

    const uint8 ElementKey = static_cast<uint8>(Character->GetArrowElement());
    UNiagaraSystem* TrailSystem = nullptr;
    if (TWeakObjectPtr<UNiagaraSystem>* Cached = CachedArrowTrails.Find(ElementKey))
    {
        TrailSystem = Cached->Get();
    }
    if (!TrailSystem)
    {
        TrailSystem = FindBestNiagara(GetArrowElementKeywords(Character->GetArrowElement()), { TEXT("Arrow"), TEXT("Projectile"), TEXT("Trail") });
        CachedArrowTrails.Add(ElementKey, TrailSystem);
    }

    FVector Origin = Character->GetActorLocation() + FVector(0.0f, 0.0f, 95.0f) + Character->GetActorForwardVector() * 75.0f;
    if (const FPlayerVisualState* State = PlayerVisualStates.Find(Character))
    {
        if (State->RightWeapon.IsValid() && State->RightWeapon->IsVisible()) { Origin = State->RightWeapon->GetComponentLocation(); }
    }

    FRotator AimRotation = Character->GetBaseAimRotation();
    AimRotation.Yaw += YawOffsetDegrees;
    AimRotation.Pitch += PitchOffsetDegrees;
    const FVector Direction = AimRotation.Vector();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    ANWArrowPresentationProjectile* Projectile = GetWorld()->SpawnActor<ANWArrowPresentationProjectile>(
        ANWArrowPresentationProjectile::StaticClass(), Origin, Direction.Rotation(), Params);

    if (Projectile)
    {
        Projectile->InitializePresentation(CachedArrowMesh.Get(), TrailSystem, Character->GetArrowElement(), Direction, 3900.0f, 1.0f);
    }
}

void ANWContentPresentationManager::ApplyEnemyPresentation(ANWEnemy* Enemy)
{
    if (!Enemy || !Enemy->GetMesh()) { return; }

    USkeletalMesh* Mesh = nullptr;
    UClass* AnimClass = nullptr;

    if (Enemy->IsWorldBoss())
    {
        const int32 Variant = static_cast<int32>(Enemy->GetUniqueID() % 5u);
        static const TCHAR* BossHeroes[] = { TEXT("Rampage"), TEXT("Sevarog"), TEXT("Khaimera"), TEXT("Countess"), TEXT("Revenant") };
        const FString PreferredHero = BossHeroes[Variant];
        TryLoadParagonHero(PreferredHero, Mesh, AnimClass);

        if (!Mesh)
        {
            Mesh = FindBestSkeletalMesh({ TEXT("Boss"), TEXT("Monster"), TEXT("Demon"), TEXT("Warlord") }, { TEXT("Enemy"), TEXT("Creature") }, true, &AnimClass);
        }
    }
    else if (Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Zombie)
    {
        Mesh = FindBestSkeletalMesh({ TEXT("Zombie"), TEXT("Undead"), TEXT("Ghoul") }, { TEXT("Enemy"), TEXT("Character"), TEXT("Monster") }, true, &AnimClass);
        if (!Mesh)
        {
            if (!TryLoadParagonHero(TEXT("Revenant"), Mesh, AnimClass)) { TryLoadParagonHero(TEXT("Khaimera"), Mesh, AnimClass); }
        }
    }
    else if (Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Ghost)
    {
        Mesh = FindBestSkeletalMesh({ TEXT("Ghost"), TEXT("Wraith"), TEXT("Specter"), TEXT("Spirit") }, { TEXT("Enemy"), TEXT("Character"), TEXT("Monster") }, true, &AnimClass);
        if (!Mesh)
        {
            if (!TryLoadParagonHero(TEXT("Sevarog"), Mesh, AnimClass)) { TryLoadParagonHero(TEXT("Countess"), Mesh, AnimClass); }
        }
    }
    else
    {
        return;
    }

    if (!Mesh) { return; }

    if (!AnimClass) { AnimClass = FindAnimationClassForMesh(Mesh, {}); }
    Enemy->GetMesh()->SetSkeletalMeshAsset(Mesh);
    if (AnimClass)
    {
        Enemy->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        Enemy->GetMesh()->SetAnimInstanceClass(AnimClass);
    }
    Enemy->GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, Enemy->IsWorldBoss() ? -112.0f : -90.0f));
    Enemy->GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    Enemy->GetMesh()->SetRelativeScale3D(Enemy->IsWorldBoss() ? FVector(1.18f) : FVector(1.0f));
    Enemy->GetMesh()->SetVisibility(true, true);
    HideEnemyDebugMeshes(Enemy);

    UE_LOG(LogTemp, Warning, TEXT("[MONSTRO-VISUAL] %s%s -> %s"),
        Enemy->IsWorldBoss() ? TEXT("BOSS ") : TEXT(""),
        Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Zombie ? TEXT("Zumbi") : Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Ghost ? TEXT("Fantasma") : TEXT("Bruto"),
        *Mesh->GetPathName());
}

UStaticMesh* ANWContentPresentationManager::FindBestStaticMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const
{
    int32 BestScore = 0;
    UStaticMesh* BestMesh = nullptr;
    for (const FAssetData& Asset : StaticMeshAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset()))
        {
            BestScore = Score;
            BestMesh = Mesh;
        }
    }
    return BestMesh;
}

USkeletalMesh* ANWContentPresentationManager::FindBestSkeletalMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, bool bRequireAnimation, UClass** OutAnimationClass) const
{
    int32 BestScore = 0;
    USkeletalMesh* BestMesh = nullptr;
    UClass* BestAnimClass = nullptr;

    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh) { continue; }
        UClass* AnimClass = FindAnimationClassForMesh(Mesh, PreferredKeywords);
        if (bRequireAnimation && !AnimClass) { continue; }

        BestScore = Score;
        BestMesh = Mesh;
        BestAnimClass = AnimClass;
    }

    if (OutAnimationClass) { *OutAnimationClass = BestAnimClass; }
    return BestMesh;
}

USkeletalMesh* ANWContentPresentationManager::FindCompatibleArmorMesh(ANWCharacter* Character, const FNWGeneratedItem& Item) const
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset()) { return nullptr; }
    USkeleton* BaseSkeleton = Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
    if (!BaseSkeleton) { return nullptr; }

    TArray<FString> Preferred = { TEXT("Armor"), TEXT("Armour"), TEXT("Modular"), TEXT("Medieval"), TEXT("Knight") };
    if (!Item.StyleId.IsNone()) { Preferred.Add(Item.StyleId.ToString()); }

    int32 BestScore = 0;
    USkeletalMesh* BestMesh = nullptr;
    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, GetArmorSlotKeywords(Item.Slot), Preferred, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh || Mesh->GetSkeleton() != BaseSkeleton) { continue; }
        BestScore = Score;
        BestMesh = Mesh;
    }
    return BestMesh;
}

UClass* ANWContentPresentationManager::FindAnimationClassForMesh(USkeletalMesh* Mesh, const TArray<FString>& PreferredKeywords) const
{
    if (!Mesh || !Mesh->GetSkeleton()) { return nullptr; }

    int32 BestScore = -1;
    UClass* BestClass = nullptr;
    for (const FAssetData& Asset : AnimBlueprintAssets)
    {
        UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset.GetAsset());
        if (!AnimBlueprint || AnimBlueprint->TargetSkeleton != Mesh->GetSkeleton() || !AnimBlueprint->GeneratedClass) { continue; }

        bool bPrimaryMatch = true;
        const int32 Score = ScoreAsset(Asset, {}, PreferredKeywords, bPrimaryMatch);
        if (Score > BestScore)
        {
            BestScore = Score;
            BestClass = AnimBlueprint->GeneratedClass;
        }
    }
    return BestClass;
}

UNiagaraSystem* ANWContentPresentationManager::FindBestNiagara(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const
{
    int32 BestScore = 0;
    UNiagaraSystem* BestSystem = nullptr;
    for (const FAssetData& Asset : NiagaraAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }
        if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset.GetAsset()))
        {
            BestScore = Score;
            BestSystem = System;
        }
    }
    return BestSystem;
}

UStaticMeshComponent* ANWContentPresentationManager::EnsureStaticVisualComponent(ANWCharacter* Character, TWeakObjectPtr<UStaticMeshComponent>& Existing, const FName BaseName) const
{
    if (Existing.IsValid()) { return Existing.Get(); }
    if (!Character) { return nullptr; }

    const FName UniqueName = MakeUniqueObjectName(Character, UStaticMeshComponent::StaticClass(), BaseName);
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Character, UniqueName, RF_Transient);
    if (!Component) { return nullptr; }

    Character->AddInstanceComponent(Component);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->RegisterComponent();
    Component->SetVisibility(false, true);
    Existing = Component;
    return Component;
}

USkeletalMeshComponent* ANWContentPresentationManager::EnsureArmorVisualComponent(ANWCharacter* Character, FPlayerVisualState& State, ENWEquipmentSlot Slot) const
{
    const uint8 SlotKey = static_cast<uint8>(Slot);
    if (TWeakObjectPtr<USkeletalMeshComponent>* Existing = State.ArmorParts.Find(SlotKey))
    {
        if (Existing->IsValid()) { return Existing->Get(); }
    }

    const FString Name = FString::Printf(TEXT("NW_Armor_%d"), static_cast<int32>(Slot));
    USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Character, MakeUniqueObjectName(Character, USkeletalMeshComponent::StaticClass(), FName(*Name)), RF_Transient);
    if (!Component) { return nullptr; }

    Character->AddInstanceComponent(Component);
    Component->SetupAttachment(Character->GetMesh());
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->RegisterComponent();
    Component->SetVisibility(false, true);
    State.ArmorParts.Add(SlotKey, Component);
    return Component;
}

UNiagaraComponent* ANWContentPresentationManager::EnsureBrutalAura(ANWCharacter* Character, FPlayerVisualState& State) const
{
    if (State.BrutalAura.IsValid()) { return State.BrutalAura.Get(); }
    if (!Character) { return nullptr; }

    UNiagaraComponent* Component = NewObject<UNiagaraComponent>(Character, MakeUniqueObjectName(Character, UNiagaraComponent::StaticClass(), FName(TEXT("NW_BrutalAura"))), RF_Transient);
    if (!Component) { return nullptr; }

    Character->AddInstanceComponent(Component);
    Component->SetupAttachment(Character->GetRootComponent());
    Component->RegisterComponent();
    Component->SetRelativeLocation(FVector(0.0f, 0.0f, -85.0f));
    Component->SetAutoActivate(false);
    State.BrutalAura = Component;
    return Component;
}

UPointLightComponent* ANWContentPresentationManager::EnsureBrutalLight(ANWCharacter* Character, FPlayerVisualState& State) const
{
    if (State.BrutalLight.IsValid()) { return State.BrutalLight.Get(); }
    if (!Character) { return nullptr; }

    UPointLightComponent* Component = NewObject<UPointLightComponent>(Character, MakeUniqueObjectName(Character, UPointLightComponent::StaticClass(), FName(TEXT("NW_BrutalLight"))), RF_Transient);
    if (!Component) { return nullptr; }

    Character->AddInstanceComponent(Component);
    Component->SetupAttachment(Character->GetRootComponent());
    Component->RegisterComponent();
    Component->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));
    Component->SetAttenuationRadius(520.0f);
    Component->SetCastShadows(false);
    Component->SetIntensity(0.0f);
    State.BrutalLight = Component;
    return Component;
}

FName ANWContentPresentationManager::FindHandSocket(ANWCharacter* Character, bool bRightHand) const
{
    if (!Character || !Character->GetMesh()) { return NAME_None; }

    const TArray<FName> Candidates = bRightHand
        ? TArray<FName>{ TEXT("hand_r"), TEXT("RightHand"), TEXT("weapon_r"), TEXT("WeaponSocket"), TEXT("weapon_socket_r") }
        : TArray<FName>{ TEXT("hand_l"), TEXT("LeftHand"), TEXT("weapon_l"), TEXT("ShieldSocket"), TEXT("weapon_socket_l") };

    for (const FName Candidate : Candidates)
    {
        if (Character->GetMesh()->DoesSocketExist(Candidate)) { return Candidate; }
    }
    return NAME_None;
}

FName ANWContentPresentationManager::GetActiveWeaponStyleId(const ANWCharacter* Character) const
{
    if (!Character) { return NAME_None; }
    for (const FNWGeneratedItem& Item : Character->GetEquippedWeaponItems())
    {
        if (Item.Kind == ENWItemKind::Weapon && Item.WeaponType == Character->GetActiveWeapon()) { return Item.StyleId; }
    }
    return NAME_None;
}

uint32 ANWContentPresentationManager::BuildArmorSignature(const ANWCharacter* Character) const
{
    if (!Character) { return 0; }
    uint32 Signature = 0x4E573241u;
    for (const FNWGeneratedItem& Item : Character->GetEquippedItems())
    {
        Signature = HashCombine(Signature, GetTypeHash(Item.ItemSeed));
        Signature = HashCombine(Signature, GetTypeHash(Item.AppearanceSeed));
        Signature = HashCombine(Signature, GetTypeHash(Item.StyleId));
    }
    return Signature;
}

TArray<FString> ANWContentPresentationManager::GetWeaponMeshKeywords(ENWWeaponType WeaponType) const
{
    switch (WeaponType)
    {
        case ENWWeaponType::Staff: return { TEXT("Staff"), TEXT("Rod"), TEXT("Wand") };
        case ENWWeaponType::Greatsword: return { TEXT("Greatsword"), TEXT("LongSword"), TEXT("Long_Sword"), TEXT("Sword"), TEXT("Blade") };
        case ENWWeaponType::DualSwords: return { TEXT("Sword"), TEXT("Blade"), TEXT("Sabre"), TEXT("Saber") };
        case ENWWeaponType::SwordShield: return { TEXT("Sword"), TEXT("Blade"), TEXT("Sabre"), TEXT("Saber") };
        case ENWWeaponType::Daggers: return { TEXT("Dagger"), TEXT("Knife"), TEXT("ShortBlade") };
        case ENWWeaponType::Bow: return { TEXT("Bow"), TEXT("Recurve") };
        case ENWWeaponType::Firearm: return { TEXT("Musket"), TEXT("Rifle"), TEXT("Gun"), TEXT("Firearm") };
        default: return { TEXT("Weapon") };
    }
}

TArray<FString> ANWContentPresentationManager::GetArrowElementKeywords(ENWArrowElement Element) const
{
    switch (Element)
    {
        case ENWArrowElement::Fire: return { TEXT("Fire"), TEXT("Flame"), TEXT("Ember") };
        case ENWArrowElement::Poison: return { TEXT("Poison"), TEXT("Venom"), TEXT("Toxic"), TEXT("Acid") };
        case ENWArrowElement::Lightning: return { TEXT("Lightning"), TEXT("Electric"), TEXT("Shock"), TEXT("Thunder") };
        case ENWArrowElement::Frost: return { TEXT("Frost"), TEXT("Ice"), TEXT("Snow"), TEXT("Cold") };
        default: return { TEXT("Arrow"), TEXT("Wind"), TEXT("Trail") };
    }
}

TArray<FString> ANWContentPresentationManager::GetArmorSlotKeywords(ENWEquipmentSlot Slot) const
{
    switch (Slot)
    {
        case ENWEquipmentSlot::Head: return { TEXT("Helmet"), TEXT("Helm"), TEXT("Head") };
        case ENWEquipmentSlot::Chest: return { TEXT("Chest"), TEXT("Torso"), TEXT("Body"), TEXT("Cuirass") };
        case ENWEquipmentSlot::Gloves: return { TEXT("Glove"), TEXT("Gauntlet"), TEXT("Hand") };
        case ENWEquipmentSlot::Legs: return { TEXT("Leg"), TEXT("Pants"), TEXT("Trouser") };
        case ENWEquipmentSlot::Boots: return { TEXT("Boot"), TEXT("Foot"), TEXT("Greave") };
        default: return { TEXT("Armor"), TEXT("Armour") };
    }
}

bool ANWContentPresentationManager::TryLoadParagonHero(const FString& HeroName, USkeletalMesh*& OutMesh, UClass*& OutAnimClass) const
{
    OutMesh = nullptr;
    OutAnimClass = nullptr;

    const FString MeshPath = FString::Printf(TEXT("/Game/Paragon%s/Characters/Heroes/%s/Meshes/%s.%s"), *HeroName, *HeroName, *HeroName, *HeroName);
    const FString AnimPath = FString::Printf(TEXT("/Game/Paragon%s/Characters/Heroes/%s/%s_AnimBlueprint.%s_AnimBlueprint_C"), *HeroName, *HeroName, *HeroName, *HeroName);

    OutMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
    OutAnimClass = LoadClass<UAnimInstance>(nullptr, *AnimPath);
    if (OutMesh && !OutAnimClass) { OutAnimClass = FindAnimationClassForMesh(OutMesh, { HeroName }); }
    return OutMesh != nullptr;
}

bool ANWContentPresentationManager::HasAssetKeywords(const TArray<FString>& Keywords) const
{
    auto MatchesAll = [&Keywords](const FAssetData& Asset)
    {
        const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        for (const FString& Keyword : Keywords)
        {
            if (!Searchable.Contains(Keyword.ToLower())) { return false; }
        }
        return true;
    };

    for (const FAssetData& Asset : StaticMeshAssets) { if (MatchesAll(Asset)) { return true; } }
    for (const FAssetData& Asset : SkeletalMeshAssets) { if (MatchesAll(Asset)) { return true; } }
    return false;
}

int32 ANWContentPresentationManager::ScoreAsset(const FAssetData& Asset, const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, bool& bPrimaryMatch) const
{
    const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
    int32 Score = 0;
    bPrimaryMatch = PrimaryKeywords.IsEmpty();

    for (const FString& Keyword : PrimaryKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower()))
        {
            bPrimaryMatch = true;
            Score += 20;
        }
    }
    for (const FString& Keyword : PreferredKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 7; }
    }

    if (Searchable.Contains(TEXT("weapon"))) { Score += 2; }
    if (Searchable.Contains(TEXT("skeletal")) || Searchable.Contains(TEXT("static"))) { Score += 1; }
    return Score;
}

void ANWContentPresentationManager::HideEnemyDebugMeshes(ANWEnemy* Enemy) const
{
    if (!Enemy) { return; }
    TArray<UStaticMeshComponent*> StaticComponents;
    Enemy->GetComponents<UStaticMeshComponent>(StaticComponents);
    for (UStaticMeshComponent* Component : StaticComponents)
    {
        if (Component) { Component->SetVisibility(false, true); }
    }
}

void ANWContentPresentationManager::CleanupDeadState()
{
    for (auto It = PlayerVisualStates.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
    for (auto It = EnemyVisualSignatures.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
}
