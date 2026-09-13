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
    PrimaryActorTick.TickInterval = 0.15f;
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

    NiagaraAssets.Reset();
    if (bEnableNiagaraPresentation)
    {
        FARFilter NiagaraFilter;
        NiagaraFilter.PackagePaths.Add(FName(TEXT("/Game")));
        NiagaraFilter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
        NiagaraFilter.bRecursivePaths = true;
        Registry.GetAssets(NiagaraFilter, NiagaraAssets);
    }

    UE_LOG(LogTemp, Warning, TEXT("[FAB] Catalogo visual: StaticMesh=%d | SkeletalMesh=%d | AnimBP=%d | Niagara=%d (%s)"),
        StaticMeshAssets.Num(), SkeletalMeshAssets.Num(), AnimBlueprintAssets.Num(), NiagaraAssets.Num(),
        bEnableNiagaraPresentation ? TEXT("ativo") : TEXT("desativado para estabilidade"));
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

    // Precarrega apenas herois explicitamente conhecidos. Isso e deterministico e
    // evita procurar/compilar uma criatura arbitraria no meio do combate.
    static const TCHAR* ParagonHeroes[] = {
        TEXT("Grux"), TEXT("Sevarog"), TEXT("Rampage"), TEXT("Khaimera"),
        TEXT("Countess"), TEXT("Revenant")
    };
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
    Right->SetRelativeTransform(FTransform::Identity);
    Left->SetRelativeTransform(FTransform::Identity);

    TArray<FString> Preferred = {
        TEXT("Realistic"), TEXT("PBR"), TEXT("Medieval"), TEXT("Steel"), TEXT("Iron"),
        TEXT("Historical"), TEXT("GameReady"), TEXT("Game_Ready"), TEXT("Weapon")
    };
    if (!StyleId.IsNone()) { Preferred.Add(StyleId.ToString()); }

    UStaticMesh* PrimaryMesh = FindBestStaticMesh(GetWeaponMeshKeywords(WeaponType), Preferred);
    if (!PrimaryMesh)
    {
        PrimaryMesh = GetFallbackWeaponMesh(WeaponType);
    }

    if (!PrimaryMesh)
    {
        UE_LOG(LogTemp, Display, TEXT("[VISUAL] Sem mesh confiavel para %s (%s); slot visual oculto sem afetar gameplay."),
            *NWCombat::WeaponTypeToString(WeaponType), *StyleId.ToString());
        return;
    }

    ConfigureWeaponComponent(Character, Right, PrimaryMesh, WeaponType, true);

    if (WeaponType == ENWWeaponType::DualSwords || WeaponType == ENWWeaponType::Daggers)
    {
        ConfigureWeaponComponent(Character, Left, PrimaryMesh, WeaponType, false);
    }
    else if (WeaponType == ENWWeaponType::SwordShield)
    {
        UStaticMesh* ShieldMesh = FindBestStaticMesh(
            { TEXT("Shield"), TEXT("Aegis"), TEXT("Buckler") },
            { TEXT("Realistic"), TEXT("PBR"), TEXT("Knight"), TEXT("Medieval"), TEXT("Iron"), TEXT("Wood") });
        if (ShieldMesh)
        {
            ConfigureWeaponComponent(Character, Left, ShieldMesh, WeaponType, false);
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[VISUAL] %s estabilizada -> %s | escala aplicada uma unica vez"),
        *NWCombat::WeaponTypeToString(WeaponType), *PrimaryMesh->GetPathName());
}

void ANWContentPresentationManager::ConfigureWeaponComponent(
    ANWCharacter* Character,
    UStaticMeshComponent* Component,
    UStaticMesh* Mesh,
    ENWWeaponType WeaponType,
    bool bRightHand) const
{
    if (!Character || !Character->GetMesh() || !Component || !Mesh) { return; }

    const FName Socket = FindHandSocket(Character, bRightHand);
    Component->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
    Component->SetStaticMesh(Mesh);
    Component->SetRelativeLocation(FVector::ZeroVector);
    Component->SetRelativeRotation(FRotator::ZeroRotator);
    Component->SetRelativeScale3D(FVector(ComputeUniformMeshScale(Mesh, GetWeaponTargetDimension(WeaponType, bRightHand))));
    Component->SetVisibility(true, true);
}

UStaticMesh* ANWContentPresentationManager::GetFallbackWeaponMesh(ENWWeaponType WeaponType) const
{
    // Apenas fallbacks cuja forma ainda comunica a arma sem ocupar a tela inteira.
    // Espadas/arcos continuam ocultos se nenhum asset apropriado existir.
    if (WeaponType == ENWWeaponType::Staff)
    {
        return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    }
    if (WeaponType == ENWWeaponType::Daggers)
    {
        return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    }
    return nullptr;
}

float ANWContentPresentationManager::GetWeaponTargetDimension(ENWWeaponType WeaponType, bool bRightHand) const
{
    switch (WeaponType)
    {
        case ENWWeaponType::Staff: return 185.0f;
        case ENWWeaponType::Greatsword: return 155.0f;
        case ENWWeaponType::DualSwords: return 105.0f;
        case ENWWeaponType::SwordShield: return bRightHand ? 105.0f : 72.0f;
        case ENWWeaponType::Daggers: return 55.0f;
        case ENWWeaponType::Bow: return 140.0f;
        case ENWWeaponType::Firearm: return 125.0f;
        default: return 110.0f;
    }
}

float ANWContentPresentationManager::ComputeUniformMeshScale(UStaticMesh* Mesh, float TargetMaxDimension) const
{
    if (!Mesh) { return 1.0f; }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const float SizeX = static_cast<float>(Bounds.BoxExtent.X * 2.0);
    const float SizeY = static_cast<float>(Bounds.BoxExtent.Y * 2.0);
    const float SizeZ = static_cast<float>(Bounds.BoxExtent.Z * 2.0);
    const float MaxDimension = FMath::Max(SizeX, FMath::Max(SizeY, SizeZ));
    if (!FMath::IsFinite(MaxDimension) || MaxDimension <= 1.0f) { return 1.0f; }
    return FMath::Clamp(TargetMaxDimension / MaxDimension, 0.02f, 4.0f);
}

float ANWContentPresentationManager::ComputeSkeletalScale(USkeletalMesh* Mesh, float TargetHeight) const
{
    if (!Mesh) { return 1.0f; }
    const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
    const float Height = static_cast<float>(Bounds.BoxExtent.Z * 2.0);
    if (!FMath::IsFinite(Height) || Height <= 1.0f) { return 1.0f; }
    return FMath::Clamp(TargetHeight / Height, 0.20f, 2.5f);
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
    UPointLightComponent* Light = EnsureBrutalLight(Character, State);
    if (!Light) { return; }

    UNiagaraComponent* Aura = nullptr;
    if (bEnableNiagaraPresentation)
    {
        Aura = EnsureBrutalAura(Character, State);
    }

    const bool bActive = Character->IsBrutalTransformationActive();
    if (bActive)
    {
        if (Aura && !Aura->GetAsset())
        {
            Aura->SetAsset(FindBestNiagara(
                { TEXT("Aura"), TEXT("Power"), TEXT("Rage"), TEXT("Energy"), TEXT("Buff") },
                { TEXT("Legendary"), TEXT("Brutal"), TEXT("Fire"), TEXT("Lightning") }));
        }
        if (Aura && Aura->GetAsset()) { Aura->Activate(true); }
        Light->SetLightColor(FLinearColor(1.0f, 0.14f, 0.02f));
        Light->SetIntensity(2400.0f);
        Character->GetMesh()->SetRelativeScale3D(FVector(1.06f));
    }
    else
    {
        if (Aura) { Aura->Deactivate(); }
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

    UNiagaraSystem* TrailSystem = nullptr;
    if (bEnableNiagaraPresentation)
    {
        const uint8 ElementKey = static_cast<uint8>(Character->GetArrowElement());
        if (TWeakObjectPtr<UNiagaraSystem>* Cached = CachedArrowTrails.Find(ElementKey))
        {
            TrailSystem = Cached->Get();
        }
        if (!TrailSystem)
        {
            TrailSystem = FindBestNiagara(GetArrowElementKeywords(Character->GetArrowElement()), { TEXT("Arrow"), TEXT("Projectile"), TEXT("Trail") });
            CachedArrowTrails.Add(ElementKey, TrailSystem);
        }
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
        TryLoadParagonHero(FString(BossHeroes[Variant]), Mesh, AnimClass);
    }
    else if (Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Zombie)
    {
        if (!TryLoadParagonHero(TEXT("Revenant"), Mesh, AnimClass))
        {
            TryLoadParagonHero(TEXT("Khaimera"), Mesh, AnimClass);
        }
    }
    else if (Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Ghost)
    {
        if (!TryLoadParagonHero(TEXT("Sevarog"), Mesh, AnimClass))
        {
            TryLoadParagonHero(TEXT("Countess"), Mesh, AnimClass);
        }
    }
    else
    {
        TryLoadParagonHero(TEXT("Grux"), Mesh, AnimClass);
    }

    if (!Mesh)
    {
        UE_LOG(LogTemp, Display, TEXT("[MONSTRO-VISUAL] %s sem pack curado; fallback de capsule mantido."), *Enemy->GetName());
        return;
    }

    Enemy->GetMesh()->SetSkeletalMeshAsset(Mesh);
    if (AnimClass)
    {
        Enemy->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        Enemy->GetMesh()->SetAnimInstanceClass(AnimClass);
    }

    const bool bBoss = Enemy->IsWorldBoss();
    const float TargetHeight = bBoss ? 315.0f : 188.0f;
    Enemy->GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, bBoss ? -112.0f : -90.0f));
    Enemy->GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    Enemy->GetMesh()->SetRelativeScale3D(FVector(ComputeSkeletalScale(Mesh, TargetHeight)));
    Enemy->GetMesh()->SetVisibility(true, true);
    HideEnemyDebugMeshes(Enemy);

    UE_LOG(LogTemp, Display, TEXT("[MONSTRO-VISUAL] %s -> %s | escala deterministica"), *Enemy->GetName(), *Mesh->GetPathName());
}

UStaticMesh* ANWContentPresentationManager::FindBestStaticMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
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
    return BestScore > 0 ? BestMesh : nullptr;
}

USkeletalMesh* ANWContentPresentationManager::FindBestSkeletalMesh(const TArray<FString>& PrimaryKeywords, const TArray<FString>& PreferredKeywords, bool bRequireAnimation, UClass** OutAnimationClass) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* BestMesh = nullptr;
    UClass* BestAnimClass = nullptr;

    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        bool bPrimaryMatch = false;
        const int32 Score = ScoreAsset(Asset, PrimaryKeywords, PreferredKeywords, bPrimaryMatch);
        if (!bPrimaryMatch || Score <= BestScore) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh) { continue; }
        UClass* CandidateAnimClass = FindAnimationClassForMesh(Mesh, PreferredKeywords);
        if (bRequireAnimation && !CandidateAnimClass) { continue; }

        BestScore = Score;
        BestMesh = Mesh;
        BestAnimClass = CandidateAnimClass;
    }

    if (OutAnimationClass) { *OutAnimationClass = BestAnimClass; }
    return BestScore > 0 ? BestMesh : nullptr;
}

USkeletalMesh* ANWContentPresentationManager::FindCompatibleArmorMesh(ANWCharacter* Character, const FNWGeneratedItem& Item) const
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset()) { return nullptr; }
    USkeleton* BaseSkeleton = Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
    if (!BaseSkeleton) { return nullptr; }

    TArray<FString> Preferred = {
        TEXT("Realistic"), TEXT("PBR"), TEXT("Armor"), TEXT("Armour"), TEXT("Modular"),
        TEXT("Medieval"), TEXT("Knight"), TEXT("Plate"), TEXT("Chainmail")
    };
    if (!Item.StyleId.IsNone()) { Preferred.Add(Item.StyleId.ToString()); }

    int32 BestScore = TNumericLimits<int32>::Lowest();
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
    return BestScore > 0 ? BestMesh : nullptr;
}

UClass* ANWContentPresentationManager::FindAnimationClassForMesh(USkeletalMesh* Mesh, const TArray<FString>& PreferredKeywords) const
{
    if (!Mesh || !Mesh->GetSkeleton()) { return nullptr; }

    int32 BestScore = TNumericLimits<int32>::Lowest();
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
    if (!bEnableNiagaraPresentation) { return nullptr; }

    int32 BestScore = TNumericLimits<int32>::Lowest();
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
    return BestScore > 0 ? BestSystem : nullptr;
}

UStaticMeshComponent* ANWContentPresentationManager::EnsureStaticVisualComponent(ANWCharacter* Character, TWeakObjectPtr<UStaticMeshComponent>& Existing, const FName BaseName) const
{
    if (Existing.IsValid()) { return Existing.Get(); }
    if (!Character) { return nullptr; }

    const FName UniqueName = MakeUniqueObjectName(Character, UStaticMeshComponent::StaticClass(), BaseName);
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Character, UniqueName, RF_Transient);
    if (!Component) { return nullptr; }

    Character->AddInstanceComponent(Component);
    Component->SetupAttachment(Character->GetMesh());
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCastShadow(true);
    Component->SetVisibility(false, true);
    Component->RegisterComponent();
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
    Component->SetVisibility(false, true);
    Component->RegisterComponent();
    State.ArmorParts.Add(SlotKey, Component);
    return Component;
}

UNiagaraComponent* ANWContentPresentationManager::EnsureBrutalAura(ANWCharacter* Character, FPlayerVisualState& State) const
{
    if (State.BrutalAura.IsValid()) { return State.BrutalAura.Get(); }
    if (!Character || !bEnableNiagaraPresentation) { return nullptr; }

    UNiagaraComponent* Component = NewObject<UNiagaraComponent>(Character, MakeUniqueObjectName(Character, UNiagaraComponent::StaticClass(), FName(TEXT("NW_BrutalAura"))), RF_Transient);
    if (!Component) { return nullptr; }

    Character->AddInstanceComponent(Component);
    Component->SetupAttachment(Character->GetRootComponent());
    Component->SetAutoActivate(false); // antes do RegisterComponent: evita warning e ativacao prematura
    Component->SetRelativeLocation(FVector(0.0f, 0.0f, -85.0f));
    Component->RegisterComponent();
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
    Component->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));
    Component->SetAttenuationRadius(520.0f);
    Component->SetCastShadows(false);
    Component->SetIntensity(0.0f);
    Component->RegisterComponent();
    State.BrutalLight = Component;
    return Component;
}

FName ANWContentPresentationManager::FindHandSocket(ANWCharacter* Character, bool bRightHand) const
{
    if (!Character || !Character->GetMesh()) { return NAME_None; }

    // Sockets dedicados tem prioridade sobre o osso cru da mao. O codigo anterior
    // encontrava hand_r primeiro e nunca chegava em weapon_r/WeaponSocket.
    const TArray<FName> Candidates = bRightHand
        ? TArray<FName>{ TEXT("weapon_r"), TEXT("WeaponSocket"), TEXT("weapon_socket_r"), TEXT("RightHand"), TEXT("hand_r") }
        : TArray<FName>{ TEXT("weapon_l"), TEXT("ShieldSocket"), TEXT("weapon_socket_l"), TEXT("LeftHand"), TEXT("hand_l") };

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
        case ENWWeaponType::Staff: return { TEXT("Staff"), TEXT("Rod"), TEXT("Wand"), TEXT("Scepter") };
        case ENWWeaponType::Greatsword: return { TEXT("Greatsword"), TEXT("Great_Sword"), TEXT("LongSword"), TEXT("Long_Sword"), TEXT("Sword"), TEXT("Blade") };
        case ENWWeaponType::DualSwords: return { TEXT("Sword"), TEXT("Blade"), TEXT("Sabre"), TEXT("Saber") };
        case ENWWeaponType::SwordShield: return { TEXT("Sword"), TEXT("Blade"), TEXT("Sabre"), TEXT("Saber") };
        case ENWWeaponType::Daggers: return { TEXT("Dagger"), TEXT("Knife"), TEXT("ShortBlade"), TEXT("Short_Blade") };
        case ENWWeaponType::Bow: return { TEXT("Bow"), TEXT("Recurve"), TEXT("Longbow") };
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
            Score += 28;
        }
    }
    for (const FString& Keyword : PreferredKeywords)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 10; }
    }

    if (Searchable.Contains(TEXT("classic_medieval")) || Searchable.Contains(TEXT("medieval_knight"))) { Score += 90; }
    if (Searchable.Contains(TEXT("realistic"))) { Score += 80; }
    if (Searchable.Contains(TEXT("pbr"))) { Score += 42; }
    if (Searchable.Contains(TEXT("4k"))) { Score += 22; }
    if (Searchable.Contains(TEXT("medieval"))) { Score += 18; }
    if (Searchable.Contains(TEXT("steel")) || Searchable.Contains(TEXT("iron"))) { Score += 14; }
    if (Searchable.Contains(TEXT("gameready")) || Searchable.Contains(TEXT("game_ready"))) { Score += 14; }

    if (Searchable.Contains(TEXT("lowpoly")) || Searchable.Contains(TEXT("low_poly")) || Searchable.Contains(TEXT("low-poly"))) { Score -= 300; }
    if (Searchable.Contains(TEXT("stylized")) || Searchable.Contains(TEXT("stylised"))) { Score -= 260; }
    if (Searchable.Contains(TEXT("cartoon")) || Searchable.Contains(TEXT("toon")) || Searchable.Contains(TEXT("chibi")) || Searchable.Contains(TEXT("voxel"))) { Score -= 360; }

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
