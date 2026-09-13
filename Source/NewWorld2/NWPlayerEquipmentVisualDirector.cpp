#include "NWPlayerEquipmentVisualDirector.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"

ANWPlayerEquipmentVisualDirector::ANWPlayerEquipmentVisualDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.18f;
    bReplicates = false;
}

void ANWPlayerEquipmentVisualDirector::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { return; }
    ScanAssets();
    UpdatePlayers();
}

void ANWPlayerEquipmentVisualDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetNetMode() == NM_DedicatedServer || !GetWorld()) { return; }
    UpdatePlayers();
}

bool ANWPlayerEquipmentVisualDirector::IsUnsafePath(const FString& InPath) const
{
    const FString Path = InPath.ToLower();
    static const TCHAR* Blocked[] = {
        TEXT("/demo/"), TEXT("/preview"), TEXT("/tutorial"), TEXT("/test/"),
        TEXT("animstarterpack"), TEXT("paragonminions"), TEXT("/fx/skeletalmeshes/"),
        TEXT("_proto"), TEXT("/buff/"), TEXT("/skins/"), TEXT("lowpoly"),
        TEXT("low_poly"), TEXT("stylized"), TEXT("cartoon"), TEXT("collision"), TEXT("proxy")
    };
    for (const TCHAR* Token : Blocked)
    {
        if (Path.Contains(Token)) { return true; }
    }
    return false;
}

void ANWPlayerEquipmentVisualDirector::ScanAssets()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    FARFilter StaticFilter;
    StaticFilter.PackagePaths.Add(FName(TEXT("/Game")));
    StaticFilter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    StaticFilter.bRecursivePaths = true;
    Registry.GetAssets(StaticFilter, StaticAssets);

    FARFilter SkeletalFilter;
    SkeletalFilter.PackagePaths.Add(FName(TEXT("/Game")));
    SkeletalFilter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
    SkeletalFilter.bRecursivePaths = true;
    Registry.GetAssets(SkeletalFilter, SkeletalAssets);

    UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V7] catalogo: static=%d skeletal=%d | modular compatível primeiro, StaticMesh por bone como fallback."),
        StaticAssets.Num(), SkeletalAssets.Num());
}

void ANWPlayerEquipmentVisualDirector::UpdatePlayers()
{
    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character) || !Character->GetMesh()) { continue; }
        FPlayerGearState& State = States.FindOrAdd(Character);
        UpdatePlayer(Character, State);
    }

    for (auto It = States.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
}

void ANWPlayerEquipmentVisualDirector::UpdatePlayer(ANWCharacter* Character, FPlayerGearState& State)
{
    const FName Style = GetActiveWeaponStyle(Character);
    if (!State.bInitialized || State.LastWeapon != Character->GetActiveWeapon() || State.LastWeaponStyle != Style)
    {
        UpdateWeapon(Character, State);
        State.LastWeapon = Character->GetActiveWeapon();
        State.LastWeaponStyle = Style;
    }

    const uint32 ArmorSignature = BuildArmorSignature(Character);
    if (!State.bInitialized || State.LastArmorSignature != ArmorSignature)
    {
        UpdateArmor(Character, State);
        State.LastArmorSignature = ArmorSignature;
    }

    if (!BaseBodyAttempted.Contains(Character) && Character->GetEquippedItems().IsEmpty())
    {
        BaseBodyAttempted.Add(Character);
        if (USkeletalMesh* BaseBody = FindBaseBodyMesh(Character))
        {
            Character->GetMesh()->SetSkeletalMeshAsset(BaseBody);
            UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V7] corpo-base/underlayer compativel ativado: %s"), *BaseBody->GetPathName());
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("[PLAYER-GEAR-V7] mantendo corpo-base atual; nenhuma underlayer compativel instalada."));
        }
    }

    State.bInitialized = true;
}

void ANWPlayerEquipmentVisualDirector::UpdateWeapon(ANWCharacter* Character, FPlayerGearState& State)
{
    UStaticMeshComponent* Right = EnsureWeaponComponent(Character, State.RightWeapon, TEXT("NWV7_Weapon_R"));
    UStaticMeshComponent* Left = EnsureWeaponComponent(Character, State.LeftWeapon, TEXT("NWV7_Weapon_L"));
    if (!Right || !Left) { return; }

    Right->SetVisibility(false, true);
    Left->SetVisibility(false, true);
    Right->SetStaticMesh(nullptr);
    Left->SetStaticMesh(nullptr);

    const ENWWeaponType Type = Character->GetActiveWeapon();
    if (!Character->HasEquippedWeaponItem(Type))
    {
        UE_LOG(LogTemp, Display, TEXT("[PLAYER-GEAR-V7] %s sem item de arma equipado: maos visuais livres."), *Character->GetName());
        return;
    }

    const FName Style = GetActiveWeaponStyle(Character);
    UStaticMesh* WeaponMesh = FindWeaponMesh(Type, Style);
    if (!WeaponMesh)
    {
        UE_LOG(LogTemp, Display, TEXT("[PLAYER-GEAR-V7] nenhum mesh realista instalado para %s/%s."),
            *NWCombat::WeaponTypeToString(Type), *Style.ToString());
        return;
    }

    auto Configure = [&](UStaticMeshComponent* Component, bool bRight, UStaticMesh* Mesh)
    {
        const FName Socket = FindHandSocket(Character, bRight);
        if (Socket.IsNone() || !Mesh) { return; }
        Component->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
        Component->SetStaticMesh(Mesh);
        Component->SetRelativeLocation(FVector::ZeroVector);
        Component->SetRelativeRotation(FRotator::ZeroRotator);
        Component->SetRelativeScale3D(FVector(ComputeStaticScale(Mesh, WeaponTargetDimension(Type, bRight))));
        Component->SetVisibility(true, true);
    };

    Configure(Right, true, WeaponMesh);
    if (Type == ENWWeaponType::Daggers || Type == ENWWeaponType::DualSwords)
    {
        Configure(Left, false, WeaponMesh);
    }
    else if (Type == ENWWeaponType::SwordShield)
    {
        if (UStaticMesh* Shield = FindShieldMesh(Style)) { Configure(Left, false, Shield); }
    }

    UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V7] arma equipada visivel: %s -> %s"),
        *NWCombat::WeaponTypeToString(Type), *WeaponMesh->GetPathName());
}

void ANWPlayerEquipmentVisualDirector::UpdateArmor(ANWCharacter* Character, FPlayerGearState& State)
{
    TSet<uint8> ActiveSlots;
    for (const FNWGeneratedItem& Item : Character->GetEquippedItems())
    {
        if (Item.Kind != ENWItemKind::Armor) { continue; }
        const uint8 Key = static_cast<uint8>(Item.Slot);
        ActiveSlots.Add(Key);

        USkeletalMeshComponent* SkeletalPart = EnsureArmorComponent(Character, State, Item.Slot);
        UStaticMeshComponent* StaticPart = EnsureStaticArmorComponent(Character, State, Item.Slot);
        if (SkeletalPart) { SkeletalPart->SetVisibility(false, true); }
        if (StaticPart) { StaticPart->SetVisibility(false, true); StaticPart->SetStaticMesh(nullptr); }

        if (USkeletalMesh* Mesh = FindArmorMesh(Character, Item))
        {
            if (!SkeletalPart) { continue; }
            SkeletalPart->SetSkeletalMeshAsset(Mesh);
            SkeletalPart->SetLeaderPoseComponent(Character->GetMesh(), false, false);
            SkeletalPart->SetRelativeTransform(FTransform::Identity);
            SkeletalPart->SetVisibility(true, true);
            UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V7] armadura modular visivel: %s -> %s"), *Item.Name, *Mesh->GetPathName());
            continue;
        }

        if (UStaticMesh* StaticMesh = FindStaticArmorMesh(Item))
        {
            if (!StaticPart) { continue; }
            const FName AttachPoint = FindArmorAttachPoint(Character, Item.Slot);
            if (AttachPoint.IsNone())
            {
                UE_LOG(LogTemp, Display, TEXT("[PLAYER-GEAR-V7] %s encontrou mesh, mas rig nao possui bone/socket de ancoragem."), *Item.Name);
                continue;
            }
            StaticPart->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachPoint);
            StaticPart->SetStaticMesh(StaticMesh);
            StaticPart->SetRelativeTransform(GetStaticArmorRelativeTransform(Item.Slot, StaticMesh));
            StaticPart->SetVisibility(true, true);
            UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V7] armadura StaticMesh visivel: %s -> %s @ %s"),
                *Item.Name, *StaticMesh->GetPathName(), *AttachPoint.ToString());
            continue;
        }

        UE_LOG(LogTemp, Display, TEXT("[PLAYER-GEAR-V7] %s sem visual compatível instalado; item continua valido no gameplay."), *Item.Name);
    }

    for (auto& Pair : State.ArmorParts)
    {
        if (!ActiveSlots.Contains(Pair.Key) && Pair.Value.IsValid()) { Pair.Value->SetVisibility(false, true); }
    }
    for (auto& Pair : State.StaticArmorParts)
    {
        if (!ActiveSlots.Contains(Pair.Key) && Pair.Value.IsValid())
        {
            Pair.Value->SetVisibility(false, true);
            Pair.Value->SetStaticMesh(nullptr);
        }
    }
}

UStaticMesh* ANWPlayerEquipmentVisualDirector::FindWeaponMesh(ENWWeaponType Type, FName StyleId) const
{
    TArray<FString> Keywords;
    switch (Type)
    {
        case ENWWeaponType::Staff: Keywords = { TEXT("staff"), TEXT("scepter"), TEXT("wand"), TEXT("rod") }; break;
        case ENWWeaponType::Greatsword: Keywords = { TEXT("greatsword"), TEXT("great_sword"), TEXT("longsword"), TEXT("sword") }; break;
        case ENWWeaponType::DualSwords: Keywords = { TEXT("sword"), TEXT("blade"), TEXT("saber") }; break;
        case ENWWeaponType::SwordShield: Keywords = { TEXT("sword"), TEXT("blade") }; break;
        case ENWWeaponType::Daggers: Keywords = { TEXT("dagger"), TEXT("knife"), TEXT("shortblade") }; break;
        case ENWWeaponType::Bow: Keywords = { TEXT("bow"), TEXT("recurve"), TEXT("longbow") }; break;
        case ENWWeaponType::Firearm: Keywords = { TEXT("musket"), TEXT("rifle"), TEXT("gun") }; break;
        default: break;
    }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* Best = nullptr;
    for (const FAssetData& Asset : StaticAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search) || Search.Contains(TEXT("/engine/basicshapes/"))) { continue; }

        bool bMatch = false;
        int32 Score = 0;
        for (const FString& K : Keywords) if (Search.Contains(K)) { bMatch = true; Score += 44; }
        if (!bMatch) { continue; }
        if (!StyleId.IsNone() && Search.Contains(StyleId.ToString().ToLower())) { Score += 95; }
        if (Search.Contains(TEXT("fab")) || Search.Contains(TEXT("realistic"))) { Score += 65; }
        if (Search.Contains(TEXT("medieval")) || Search.Contains(TEXT("pbr")) || Search.Contains(TEXT("game_ready")) || Search.Contains(TEXT("gameready"))) { Score += 40; }
        if (Search.Contains(TEXT("classic_medieval")) || Search.Contains(TEXT("dark_knight")) || Search.Contains(TEXT("thornblade")) || Search.Contains(TEXT("ethereal"))) { Score += 75; }
        if (Search.Contains(TEXT("sample")) && !Search.Contains(TEXT("weapon"))) { Score -= 20; }
        if (Score <= BestScore) { continue; }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset())) { BestScore = Score; Best = Mesh; }
    }
    return Best;
}

UStaticMesh* ANWPlayerEquipmentVisualDirector::FindShieldMesh(FName StyleId) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* Best = nullptr;
    for (const FAssetData& Asset : StaticAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search) || (!Search.Contains(TEXT("shield")) && !Search.Contains(TEXT("aegis")))) { continue; }
        int32 Score = 40;
        if (!StyleId.IsNone() && Search.Contains(StyleId.ToString().ToLower())) { Score += 80; }
        if (Search.Contains(TEXT("realistic")) || Search.Contains(TEXT("medieval")) || Search.Contains(TEXT("fab")) || Search.Contains(TEXT("viking"))) { Score += 50; }
        if (Score <= BestScore) { continue; }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset())) { BestScore = Score; Best = Mesh; }
    }
    return Best;
}

USkeletalMesh* ANWPlayerEquipmentVisualDirector::FindArmorMesh(ANWCharacter* Character, const FNWGeneratedItem& Item) const
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset()) { return nullptr; }
    USkeleton* Skeleton = Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
    if (!Skeleton) { return nullptr; }

    TArray<FString> Keywords;
    switch (Item.Slot)
    {
        case ENWEquipmentSlot::Head: Keywords = { TEXT("helmet"), TEXT("helm"), TEXT("head") }; break;
        case ENWEquipmentSlot::Chest: Keywords = { TEXT("chest"), TEXT("torso"), TEXT("body"), TEXT("cuirass") }; break;
        case ENWEquipmentSlot::Gloves: Keywords = { TEXT("glove"), TEXT("gauntlet"), TEXT("hand") }; break;
        case ENWEquipmentSlot::Legs: Keywords = { TEXT("legs"), TEXT("pants"), TEXT("trouser"), TEXT("greave") }; break;
        case ENWEquipmentSlot::Boots: Keywords = { TEXT("boots"), TEXT("foot"), TEXT("shoe") }; break;
        default: break;
    }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* Best = nullptr;
    for (const FAssetData& Asset : SkeletalAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search)) { continue; }
        bool bMatch = false;
        int32 Score = 0;
        for (const FString& K : Keywords) if (Search.Contains(K)) { bMatch = true; Score += 36; }
        if (!bMatch) { continue; }
        if (!Item.StyleId.IsNone() && Search.Contains(Item.StyleId.ToString().ToLower())) { Score += 80; }
        if (Search.Contains(TEXT("armor")) || Search.Contains(TEXT("armour")) || Search.Contains(TEXT("modular"))) { Score += 45; }
        if (Search.Contains(TEXT("fab")) || Search.Contains(TEXT("realistic")) || Search.Contains(TEXT("medieval"))) { Score += 35; }
        if (Score <= BestScore) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh || Mesh->GetSkeleton() != Skeleton || Mesh->HasActiveClothingAssets()) { continue; }
        BestScore = Score;
        Best = Mesh;
    }
    return Best;
}

UStaticMesh* ANWPlayerEquipmentVisualDirector::FindStaticArmorMesh(const FNWGeneratedItem& Item) const
{
    TArray<FString> Keywords;
    switch (Item.Slot)
    {
        case ENWEquipmentSlot::Head: Keywords = { TEXT("helmet"), TEXT("helm"), TEXT("headgear") }; break;
        case ENWEquipmentSlot::Chest: Keywords = { TEXT("chest"), TEXT("cuirass"), TEXT("breastplate"), TEXT("armor_body"), TEXT("armour_body") }; break;
        case ENWEquipmentSlot::Gloves: Keywords = { TEXT("glove"), TEXT("gauntlet") }; break;
        case ENWEquipmentSlot::Legs: Keywords = { TEXT("greave"), TEXT("leg_armor"), TEXT("legarmor"), TEXT("pants") }; break;
        case ENWEquipmentSlot::Boots: Keywords = { TEXT("boot"), TEXT("sabat"), TEXT("foot_armor"), TEXT("footarmor") }; break;
        default: break;
    }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* Best = nullptr;
    for (const FAssetData& Asset : StaticAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search) || Search.Contains(TEXT("weapon")) || Search.Contains(TEXT("shield")) || Search.Contains(TEXT("sword")) || Search.Contains(TEXT("axe"))) { continue; }
        bool bMatch = false;
        int32 Score = 0;
        for (const FString& K : Keywords)
        {
            if (Search.Contains(K)) { bMatch = true; Score += 52; }
        }
        if (!bMatch) { continue; }
        if (!Item.StyleId.IsNone() && Search.Contains(Item.StyleId.ToString().ToLower())) { Score += 100; }
        if (Search.Contains(TEXT("armor")) || Search.Contains(TEXT("armour")) || Search.Contains(TEXT("knight"))) { Score += 45; }
        if (Search.Contains(TEXT("realistic")) || Search.Contains(TEXT("pbr")) || Search.Contains(TEXT("medieval")) || Search.Contains(TEXT("fab"))) { Score += 42; }
        if (Search.Contains(TEXT("statue")) || Search.Contains(TEXT("pedestal")) || Search.Contains(TEXT("environment"))) { Score -= 300; }
        if (Score <= BestScore) { continue; }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset())) { BestScore = Score; Best = Mesh; }
    }
    return Best;
}

USkeletalMesh* ANWPlayerEquipmentVisualDirector::FindBaseBodyMesh(ANWCharacter* Character) const
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset()) { return nullptr; }
    USkeleton* Skeleton = Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
    if (!Skeleton) { return nullptr; }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* Best = nullptr;
    for (const FAssetData& Asset : SkeletalAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search)) { continue; }
        if (!Search.Contains(TEXT("basebody")) && !Search.Contains(TEXT("base_body")) &&
            !Search.Contains(TEXT("underwear")) && !Search.Contains(TEXT("underlayer"))) { continue; }
        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh || Mesh->GetSkeleton() != Skeleton || Mesh->HasActiveClothingAssets()) { continue; }
        int32 Score = 50;
        if (Search.Contains(TEXT("underwear")) || Search.Contains(TEXT("underlayer"))) { Score += 45; }
        if (Score > BestScore) { BestScore = Score; Best = Mesh; }
    }
    return Best;
}

UStaticMeshComponent* ANWPlayerEquipmentVisualDirector::EnsureWeaponComponent(ANWCharacter* Character, TWeakObjectPtr<UStaticMeshComponent>& Existing, const FName Name) const
{
    if (Existing.IsValid()) { return Existing.Get(); }
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Character, MakeUniqueObjectName(Character, UStaticMeshComponent::StaticClass(), Name), RF_Transient);
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

USkeletalMeshComponent* ANWPlayerEquipmentVisualDirector::EnsureArmorComponent(ANWCharacter* Character, FPlayerGearState& State, ENWEquipmentSlot Slot) const
{
    const uint8 Key = static_cast<uint8>(Slot);
    if (TWeakObjectPtr<USkeletalMeshComponent>* Existing = State.ArmorParts.Find(Key))
    {
        if (Existing->IsValid()) { return Existing->Get(); }
    }
    const FName Name(*FString::Printf(TEXT("NWV7_ArmorSkeletal_%d"), static_cast<int32>(Slot)));
    USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Character, MakeUniqueObjectName(Character, USkeletalMeshComponent::StaticClass(), Name), RF_Transient);
    if (!Component) { return nullptr; }
    Character->AddInstanceComponent(Component);
    Component->SetupAttachment(Character->GetMesh());
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCastShadow(true);
    Component->SetVisibility(false, true);
    Component->RegisterComponent();
    State.ArmorParts.Add(Key, Component);
    return Component;
}

UStaticMeshComponent* ANWPlayerEquipmentVisualDirector::EnsureStaticArmorComponent(ANWCharacter* Character, FPlayerGearState& State, ENWEquipmentSlot Slot) const
{
    const uint8 Key = static_cast<uint8>(Slot);
    if (TWeakObjectPtr<UStaticMeshComponent>* Existing = State.StaticArmorParts.Find(Key))
    {
        if (Existing->IsValid()) { return Existing->Get(); }
    }
    const FName Name(*FString::Printf(TEXT("NWV7_ArmorStatic_%d"), static_cast<int32>(Slot)));
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Character, MakeUniqueObjectName(Character, UStaticMeshComponent::StaticClass(), Name), RF_Transient);
    if (!Component) { return nullptr; }
    Character->AddInstanceComponent(Component);
    Component->SetupAttachment(Character->GetMesh());
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCastShadow(true);
    Component->SetVisibility(false, true);
    Component->RegisterComponent();
    State.StaticArmorParts.Add(Key, Component);
    return Component;
}

FName ANWPlayerEquipmentVisualDirector::FindHandSocket(ANWCharacter* Character, bool bRight) const
{
    if (!Character || !Character->GetMesh()) { return NAME_None; }
    const TArray<FName> Candidates = bRight
        ? TArray<FName>{ TEXT("weapon_r"), TEXT("WeaponSocket"), TEXT("weapon_socket_r"), TEXT("RightHand"), TEXT("hand_r") }
        : TArray<FName>{ TEXT("weapon_l"), TEXT("ShieldSocket"), TEXT("weapon_socket_l"), TEXT("LeftHand"), TEXT("hand_l") };
    for (FName Candidate : Candidates)
    {
        if (Character->GetMesh()->DoesSocketExist(Candidate) || Character->GetMesh()->GetBoneIndex(Candidate) != INDEX_NONE) { return Candidate; }
    }
    return NAME_None;
}

FName ANWPlayerEquipmentVisualDirector::FindArmorAttachPoint(ANWCharacter* Character, ENWEquipmentSlot Slot) const
{
    if (!Character || !Character->GetMesh()) { return NAME_None; }
    TArray<FName> Candidates;
    switch (Slot)
    {
        case ENWEquipmentSlot::Head: Candidates = { TEXT("head"), TEXT("Head"), TEXT("neck_01") }; break;
        case ENWEquipmentSlot::Chest: Candidates = { TEXT("spine_03"), TEXT("spine_02"), TEXT("spine_01"), TEXT("chest") }; break;
        case ENWEquipmentSlot::Gloves: Candidates = { TEXT("hand_r"), TEXT("RightHand"), TEXT("lowerarm_r") }; break;
        case ENWEquipmentSlot::Legs: Candidates = { TEXT("pelvis"), TEXT("Pelvis"), TEXT("thigh_r") }; break;
        case ENWEquipmentSlot::Boots: Candidates = { TEXT("foot_r"), TEXT("Foot_R"), TEXT("calf_r") }; break;
        default: break;
    }
    for (FName Candidate : Candidates)
    {
        if (Character->GetMesh()->DoesSocketExist(Candidate) || Character->GetMesh()->GetBoneIndex(Candidate) != INDEX_NONE) { return Candidate; }
    }
    return NAME_None;
}

FTransform ANWPlayerEquipmentVisualDirector::GetStaticArmorRelativeTransform(ENWEquipmentSlot Slot, UStaticMesh* Mesh) const
{
    float TargetDimension = 55.0f;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    switch (Slot)
    {
        case ENWEquipmentSlot::Head: TargetDimension = 38.0f; break;
        case ENWEquipmentSlot::Chest: TargetDimension = 78.0f; break;
        case ENWEquipmentSlot::Gloves: TargetDimension = 28.0f; break;
        case ENWEquipmentSlot::Legs: TargetDimension = 76.0f; break;
        case ENWEquipmentSlot::Boots: TargetDimension = 34.0f; break;
        default: break;
    }
    const float Scale = ComputeStaticScale(Mesh, TargetDimension);
    return FTransform(Rotation, Location, FVector(Scale));
}

float ANWPlayerEquipmentVisualDirector::ComputeStaticScale(UStaticMesh* Mesh, float TargetDimension) const
{
    if (!Mesh) { return 1.0f; }
    const FBoxSphereBounds B = Mesh->GetBounds();
    const float MaxDim = FMath::Max3(static_cast<float>(B.BoxExtent.X * 2.0), static_cast<float>(B.BoxExtent.Y * 2.0), static_cast<float>(B.BoxExtent.Z * 2.0));
    return (!FMath::IsFinite(MaxDim) || MaxDim <= 1.0f) ? 1.0f : FMath::Clamp(TargetDimension / MaxDim, 0.03f, 3.2f);
}

float ANWPlayerEquipmentVisualDirector::WeaponTargetDimension(ENWWeaponType Type, bool bRight) const
{
    switch (Type)
    {
        case ENWWeaponType::Staff: return 180.0f;
        case ENWWeaponType::Greatsword: return 155.0f;
        case ENWWeaponType::DualSwords: return 105.0f;
        case ENWWeaponType::SwordShield: return bRight ? 105.0f : 75.0f;
        case ENWWeaponType::Daggers: return 58.0f;
        case ENWWeaponType::Bow: return 142.0f;
        case ENWWeaponType::Firearm: return 128.0f;
        default: return 110.0f;
    }
}

uint32 ANWPlayerEquipmentVisualDirector::BuildArmorSignature(const ANWCharacter* Character) const
{
    uint32 Signature = 0x56374752u;
    if (!Character) { return Signature; }
    for (const FNWGeneratedItem& Item : Character->GetEquippedItems())
    {
        Signature = HashCombine(Signature, GetTypeHash(Item.ItemSeed));
        Signature = HashCombine(Signature, GetTypeHash(Item.AppearanceSeed));
        Signature = HashCombine(Signature, GetTypeHash(Item.StyleId));
    }
    return Signature;
}

FName ANWPlayerEquipmentVisualDirector::GetActiveWeaponStyle(const ANWCharacter* Character) const
{
    if (!Character) { return NAME_None; }
    for (const FNWGeneratedItem& Item : Character->GetEquippedWeaponItems())
    {
        if (Item.Kind == ENWItemKind::Weapon && Item.WeaponType == Character->GetActiveWeapon()) { return Item.StyleId; }
    }
    return NAME_None;
}
