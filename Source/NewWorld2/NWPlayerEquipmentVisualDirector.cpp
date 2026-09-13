#include "NWPlayerEquipmentVisualDirector.h"

#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"

namespace
{
    bool ContainsAny(const FString& Search, const TArray<FString>& Tokens)
    {
        for (const FString& Token : Tokens)
        {
            if (Search.Contains(Token, ESearchCase::IgnoreCase)) { return true; }
        }
        return false;
    }

    bool AreLeaderPoseMeshesCompatible(const USkeletalMesh* BaseMesh, const USkeletalMesh* PartMesh)
    {
        if (!BaseMesh || !PartMesh) { return false; }
        if (BaseMesh->GetSkeleton() && BaseMesh->GetSkeleton() == PartMesh->GetSkeleton()) { return true; }

        // Alguns packs Fab duplicam o USkeleton mesmo mantendo exatamente a mesma
        // hierarquia. Leader Pose funciona nesse caso se a tabela de bones e identica.
        const FReferenceSkeleton& BaseRef = BaseMesh->GetRefSkeleton();
        const FReferenceSkeleton& PartRef = PartMesh->GetRefSkeleton();
        if (BaseRef.GetNum() <= 0 || BaseRef.GetNum() != PartRef.GetNum()) { return false; }

        for (int32 Index = 0; Index < BaseRef.GetNum(); ++Index)
        {
            if (BaseRef.GetBoneName(Index) != PartRef.GetBoneName(Index)) { return false; }
            if (BaseRef.GetParentIndex(Index) != PartRef.GetParentIndex(Index)) { return false; }
        }
        return true;
    }

    UClass* LoadNeutralAnimClass(const FString& MeshPath)
    {
        const FString Lower = MeshPath.ToLower();
        if (Lower.Contains(TEXT("uefn_mannequin")) || Lower.Contains(TEXT("gameanimationsample")) || Lower.Contains(TEXT("game_animation_sample")))
        {
            if (UClass* Class = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Blueprints/ABP_SandboxCharacter.ABP_SandboxCharacter_C")))
            {
                return Class;
            }
        }

        if (Lower.Contains(TEXT("manny")) || Lower.Contains(TEXT("mannequin")))
        {
            static const TCHAR* MannyAnimCandidates[] = {
                TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C"),
                TEXT("/Game/Characters/Mannequins/Animations/ABP_Quinn.ABP_Quinn_C")
            };
            for (const TCHAR* Candidate : MannyAnimCandidates)
            {
                if (UClass* Class = LoadClass<UAnimInstance>(nullptr, Candidate)) { return Class; }
            }
        }
        return nullptr;
    }

    int32 NeutralBodyScore(const FString& Search)
    {
        if (Search.Contains(TEXT("greystone")) || Search.Contains(TEXT("paragon")) ||
            Search.Contains(TEXT("minion")) || Search.Contains(TEXT("enemy")) ||
            Search.Contains(TEXT("monster")) || Search.Contains(TEXT("boss")) ||
            Search.Contains(TEXT("weapon")) || Search.Contains(TEXT("helmet")) ||
            Search.Contains(TEXT("cuirass")) || Search.Contains(TEXT("gauntlet")) ||
            Search.Contains(TEXT("armor_piece")) || Search.Contains(TEXT("armour_piece")))
        {
            return TNumericLimits<int32>::Lowest();
        }

        int32 Score = 0;
        if (Search.Contains(TEXT("skm_manny_simple"))) Score += 1500;
        else if (Search.Contains(TEXT("skm_manny"))) Score += 1450;
        if (Search.Contains(TEXT("skm_uefn_mannequin"))) Score += 1420;
        if (Search.Contains(TEXT("uefn_mannequin"))) Score += 1200;
        if (Search.Contains(TEXT("/characters/mannequins/"))) Score += 1050;
        if (Search.Contains(TEXT("basebody")) || Search.Contains(TEXT("base_body"))) Score += 950;
        if (Search.Contains(TEXT("underwear")) || Search.Contains(TEXT("underlayer"))) Score += 900;
        if (Search.Contains(TEXT("mannequin")) || Search.Contains(TEXT("manny"))) Score += 700;
        if (Search.Contains(TEXT("neutral")) || Search.Contains(TEXT("body_base"))) Score += 520;

        // Um personagem completo de cavaleiro volta ao problema original: armadura
        // permanente por baixo das pecas equipadas. Nao o escolhemos como corpo-base.
        if (Search.Contains(TEXT("knight")) || Search.Contains(TEXT("warrior")) ||
            Search.Contains(TEXT("completecharacter")) || Search.Contains(TEXT("complete_character")))
        {
            Score -= 900;
        }
        return Score;
    }

    float LongestExtent(const FBoxSphereBounds& Bounds, int32& OutAxis)
    {
        OutAxis = 0;
        float Result = static_cast<float>(Bounds.BoxExtent.X);
        if (Bounds.BoxExtent.Y > Result) { Result = static_cast<float>(Bounds.BoxExtent.Y); OutAxis = 1; }
        if (Bounds.BoxExtent.Z > Result) { Result = static_cast<float>(Bounds.BoxExtent.Z); OutAxis = 2; }
        return Result;
    }
}

ANWPlayerEquipmentVisualDirector::ANWPlayerEquipmentVisualDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.16f;
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
        TEXT("paragonminions"), TEXT("/fx/skeletalmeshes/"), TEXT("_proto"),
        TEXT("/buff/"), TEXT("/skins/"), TEXT("lowpoly"), TEXT("low_poly"),
        TEXT("stylized"), TEXT("cartoon"), TEXT("collision"), TEXT("proxy")
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

    UE_LOG(LogTemp, Warning,
        TEXT("[PLAYER-GEAR-V8] catalogo: static=%d skeletal=%d | corpo neutro + familia de skeleton + fit de armas."),
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
    ResolveNeutralBase(Character, State);

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

    State.bInitialized = true;
}

void ANWPlayerEquipmentVisualDirector::ResolveNeutralBase(ANWCharacter* Character, FPlayerGearState& State)
{
    if (!Character || !Character->GetMesh() || State.bBaseResolved) { return; }
    State.bBaseResolved = true;

    USkeletalMesh* CurrentMesh = Character->GetMesh()->GetSkeletalMeshAsset();
    UClass* CurrentAnimClass = Character->GetMesh()->GetAnimClass();

    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* BestMesh = nullptr;
    UClass* BestAnimClass = nullptr;

    for (const FAssetData& Asset : SkeletalAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search)) { continue; }

        int32 Score = NeutralBodyScore(Search);
        if (Score <= BestScore || Score < 450) { continue; }

        USkeletalMesh* Candidate = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Candidate || Candidate->HasActiveClothingAssets()) { continue; }

        UClass* AnimClass = LoadNeutralAnimClass(Candidate->GetPathName());
        if (!AnimClass && CurrentMesh && CurrentAnimClass && AreLeaderPoseMeshesCompatible(CurrentMesh, Candidate))
        {
            // Underlayer da mesma familia do personagem atual pode reutilizar o AnimBP.
            AnimClass = CurrentAnimClass;
            Score += 120;
        }
        if (!AnimClass) { continue; }

        BestScore = Score;
        BestMesh = Candidate;
        BestAnimClass = AnimClass;
    }

    if (!BestMesh || !BestAnimClass)
    {
        State.BaseMeshPath = CurrentMesh ? CurrentMesh->GetPathName() : FString();
        State.bNeutralBaseActive = false;
        UE_LOG(LogTemp, Warning,
            TEXT("[PLAYER-BASE-V8] avatar neutro nao encontrado. Greystone/visual atual mantido temporariamente. Rode prepare-neutral-player-linux.sh ou instale Game Animation Sample."));
        return;
    }

    Character->GetMesh()->SetSkeletalMeshAsset(BestMesh);
    Character->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    Character->GetMesh()->SetAnimInstanceClass(BestAnimClass);
    Character->GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
    Character->GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    Character->GetMesh()->SetRelativeScale3D(FVector::OneVector);
    Character->GetMesh()->SetVisibility(true, true);

    State.BaseMeshPath = BestMesh->GetPathName();
    State.bNeutralBaseActive = true;

    UE_LOG(LogTemp, Warning,
        TEXT("[PLAYER-BASE-V8] corpo neutro ativo: %s | AnimClass=%s | armadura passa a procurar a mesma familia de bones."),
        *BestMesh->GetPathName(), *BestAnimClass->GetName());
}

void ANWPlayerEquipmentVisualDirector::UpdateWeapon(ANWCharacter* Character, FPlayerGearState& State)
{
    UStaticMeshComponent* Right = EnsureWeaponComponent(Character, State.RightWeapon, TEXT("NWV8_Weapon_R"));
    UStaticMeshComponent* Left = EnsureWeaponComponent(Character, State.LeftWeapon, TEXT("NWV8_Weapon_L"));
    if (!Right || !Left) { return; }

    Right->SetVisibility(false, true);
    Left->SetVisibility(false, true);
    Right->SetStaticMesh(nullptr);
    Left->SetStaticMesh(nullptr);

    const ENWWeaponType Type = Character->GetActiveWeapon();
    if (!Character->HasEquippedWeaponItem(Type))
    {
        UE_LOG(LogTemp, Display, TEXT("[PLAYER-GEAR-V8] %s sem item da arma ativa: maos livres."), *Character->GetName());
        return;
    }

    const FName Style = GetActiveWeaponStyle(Character);
    UStaticMesh* WeaponMesh = FindWeaponMesh(Type, Style);
    if (!WeaponMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V8] nenhum mesh de arma encontrado para %s/%s."),
            *NWCombat::WeaponTypeToString(Type), *Style.ToString());
        return;
    }

    auto Configure = [&](UStaticMeshComponent* Component, bool bRight, UStaticMesh* Mesh)
    {
        if (!Component || !Mesh) { return; }
        const FName Socket = FindHandSocket(Character, bRight);
        if (Socket.IsNone())
        {
            UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V8] rig sem socket/bone de mao %s para %s."),
                bRight ? TEXT("direita") : TEXT("esquerda"), *Character->GetName());
            return;
        }
        Component->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
        Component->SetStaticMesh(Mesh);
        Component->SetRelativeTransform(GetWeaponRelativeTransform(Type, bRight, Mesh));
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

    UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V8] arma visivel: %s -> %s | base=%s"),
        *NWCombat::WeaponTypeToString(Type), *WeaponMesh->GetPathName(), *State.BaseMeshPath);
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
        if (SkeletalPart) { SkeletalPart->SetVisibility(false, true); SkeletalPart->SetSkeletalMeshAsset(nullptr); }
        if (StaticPart) { StaticPart->SetVisibility(false, true); StaticPart->SetStaticMesh(nullptr); }

        if (USkeletalMesh* Mesh = FindArmorMesh(Character, Item))
        {
            if (!SkeletalPart) { continue; }
            SkeletalPart->SetSkeletalMeshAsset(Mesh);
            SkeletalPart->SetLeaderPoseComponent(Character->GetMesh(), false, false);
            SkeletalPart->SetRelativeTransform(FTransform::Identity);
            SkeletalPart->SetVisibility(true, true);
            UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V8] armadura VESTIDA: %s -> %s"), *Item.Name, *Mesh->GetPathName());
            continue;
        }

        // Static armor em tronco/pernas/luvas foi propositalmente desativada na V8:
        // prender uma peca rigida a um unico bone gera exatamente o efeito flutuando
        // relatado no playtest. Capacete e a unica excecao visualmente previsivel.
        if (Item.Slot == ENWEquipmentSlot::Head)
        {
            if (UStaticMesh* StaticMesh = FindStaticArmorMesh(Item))
            {
                if (StaticPart)
                {
                    const FName AttachPoint = FindArmorAttachPoint(Character, Item.Slot);
                    if (!AttachPoint.IsNone())
                    {
                        StaticPart->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachPoint);
                        StaticPart->SetStaticMesh(StaticMesh);
                        StaticPart->SetRelativeTransform(GetStaticArmorRelativeTransform(Item.Slot, StaticMesh));
                        StaticPart->SetVisibility(true, true);
                        UE_LOG(LogTemp, Warning, TEXT("[PLAYER-GEAR-V8] capacete StaticMesh seguro: %s -> %s"),
                            *Item.Name, *StaticMesh->GetPathName());
                        continue;
                    }
                }
            }
        }

        UE_LOG(LogTemp, Display,
            TEXT("[PLAYER-GEAR-V8] %s sem peca skeletal compatível com o corpo-base; fallback rigido omitido para nao flutuar."),
            *Item.Name);
    }

    for (auto& Pair : State.ArmorParts)
    {
        if (!ActiveSlots.Contains(Pair.Key) && Pair.Value.IsValid())
        {
            Pair.Value->SetVisibility(false, true);
            Pair.Value->SetSkeletalMeshAsset(nullptr);
        }
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
    TArray<FString> Primary;
    TArray<FString> Secondary;
    switch (Type)
    {
        case ENWWeaponType::Staff:
            Primary = { TEXT("staff"), TEXT("scepter"), TEXT("sceptre"), TEXT("wand"), TEXT("quarterstaff"), TEXT("magic_staff"), TEXT("magicstaff") };
            Secondary = { TEXT("rod"), TEXT("polearm"), TEXT("spear") };
            break;
        case ENWWeaponType::Greatsword: Primary = { TEXT("greatsword"), TEXT("great_sword"), TEXT("longsword") }; Secondary = { TEXT("sword") }; break;
        case ENWWeaponType::DualSwords: Primary = { TEXT("sword"), TEXT("blade"), TEXT("saber"), TEXT("sabre") }; break;
        case ENWWeaponType::SwordShield: Primary = { TEXT("sword"), TEXT("blade") }; break;
        case ENWWeaponType::Daggers: Primary = { TEXT("dagger"), TEXT("knife"), TEXT("shortblade"), TEXT("short_blade") }; break;
        case ENWWeaponType::Bow: Primary = { TEXT("bow"), TEXT("recurve"), TEXT("longbow") }; break;
        case ENWWeaponType::Firearm: Primary = { TEXT("musket"), TEXT("rifle"), TEXT("gun"), TEXT("firearm") }; break;
        default: break;
    }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* Best = nullptr;
    for (const FAssetData& Asset : StaticAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search) || Search.Contains(TEXT("/engine/basicshapes/"))) { continue; }
        if (Search.Contains(TEXT("environment")) || Search.Contains(TEXT("building")) || Search.Contains(TEXT("column")) ||
            Search.Contains(TEXT("lamppost")) || Search.Contains(TEXT("fence")) || Search.Contains(TEXT("foliage"))) { continue; }
        if (Search.Contains(TEXT("armor")) || Search.Contains(TEXT("armour")) || Search.Contains(TEXT("helmet"))) { continue; }

        int32 Score = 0;
        bool bMatch = false;
        for (const FString& K : Primary)
        {
            if (Search.Contains(K)) { Score += 70; bMatch = true; }
        }
        for (const FString& K : Secondary)
        {
            if (Search.Contains(K)) { Score += 28; bMatch = true; }
        }
        if (!bMatch) { continue; }

        if (!StyleId.IsNone() && Search.Contains(StyleId.ToString().ToLower())) { Score += 95; }
        if (Search.Contains(TEXT("weapon")) || Search.Contains(TEXT("weapons"))) { Score += 60; }
        if (Search.Contains(TEXT("fab")) || Search.Contains(TEXT("realistic"))) { Score += 55; }
        if (Search.Contains(TEXT("medieval")) || Search.Contains(TEXT("pbr")) || Search.Contains(TEXT("game_ready")) || Search.Contains(TEXT("gameready"))) { Score += 35; }
        if (Search.Contains(TEXT("classic_medieval")) || Search.Contains(TEXT("dark_knight")) || Search.Contains(TEXT("thornblade")) || Search.Contains(TEXT("ethereal"))) { Score += 65; }
        if (Search.Contains(TEXT("sample")) && !Search.Contains(TEXT("weapon"))) { Score -= 30; }

        UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
        if (!Mesh) { continue; }

        if (Type == ENWWeaponType::Staff)
        {
            const FBoxSphereBounds B = Mesh->GetBounds();
            const float A = static_cast<float>(B.BoxExtent.X);
            const float C = static_cast<float>(B.BoxExtent.Y);
            const float D = static_cast<float>(B.BoxExtent.Z);
            const float Longest = FMath::Max3(A, C, D);
            const float Smallest = FMath::Max(1.0f, FMath::Min3(A, C, D));
            if ((Longest / Smallest) >= 3.0f) Score += 55;
        }

        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Mesh;
        }
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
        if (Search.Contains(TEXT("icon")) || Search.Contains(TEXT("ui/"))) { continue; }

        int32 Score = 60;
        if (!StyleId.IsNone() && Search.Contains(StyleId.ToString().ToLower())) Score += 80;
        if (Search.Contains(TEXT("realistic")) || Search.Contains(TEXT("medieval")) || Search.Contains(TEXT("fab")) || Search.Contains(TEXT("viking"))) Score += 55;

        if (Score <= BestScore) { continue; }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset())) { BestScore = Score; Best = Mesh; }
    }
    return Best;
}

USkeletalMesh* ANWPlayerEquipmentVisualDirector::FindArmorMesh(ANWCharacter* Character, const FNWGeneratedItem& Item) const
{
    if (!Character || !Character->GetMesh()) { return nullptr; }
    USkeletalMesh* BaseMesh = Character->GetMesh()->GetSkeletalMeshAsset();
    if (!BaseMesh) { return nullptr; }

    TArray<FString> Keywords;
    switch (Item.Slot)
    {
        case ENWEquipmentSlot::Head: Keywords = { TEXT("helmet"), TEXT("helm"), TEXT("headgear"), TEXT("head_armor"), TEXT("headarmor") }; break;
        case ENWEquipmentSlot::Chest: Keywords = { TEXT("chest"), TEXT("torso"), TEXT("cuirass"), TEXT("breastplate"), TEXT("upperbody"), TEXT("upper_body") }; break;
        case ENWEquipmentSlot::Gloves: Keywords = { TEXT("glove"), TEXT("gauntlet"), TEXT("hand_armor"), TEXT("handarmor") }; break;
        case ENWEquipmentSlot::Legs: Keywords = { TEXT("legs"), TEXT("pants"), TEXT("trouser"), TEXT("greave"), TEXT("lowerbody"), TEXT("lower_body") }; break;
        case ENWEquipmentSlot::Boots: Keywords = { TEXT("boots"), TEXT("boot"), TEXT("shoe"), TEXT("foot_armor"), TEXT("footarmor") }; break;
        default: break;
    }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* Best = nullptr;
    for (const FAssetData& Asset : SkeletalAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search)) { continue; }
        if (Search.Contains(TEXT("skm_manny")) || Search.Contains(TEXT("uefn_mannequin")) ||
            Search.Contains(TEXT("greystone")) || Search.Contains(TEXT("weapon"))) { continue; }

        bool bMatch = false;
        int32 Score = 0;
        for (const FString& K : Keywords)
        {
            if (Search.Contains(K)) { bMatch = true; Score += 64; }
        }
        if (!bMatch) { continue; }

        if (!Item.StyleId.IsNone() && Search.Contains(Item.StyleId.ToString().ToLower())) Score += 85;
        if (Search.Contains(TEXT("armor")) || Search.Contains(TEXT("armour")) || Search.Contains(TEXT("modular"))) Score += 70;
        if (Search.Contains(TEXT("realistic")) || Search.Contains(TEXT("medieval")) || Search.Contains(TEXT("fab"))) Score += 40;
        if (Search.Contains(TEXT("fullbody")) || Search.Contains(TEXT("full_body")) || Search.Contains(TEXT("completecharacter"))) Score -= 150;

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh || Mesh->HasActiveClothingAssets() || !AreLeaderPoseMeshesCompatible(BaseMesh, Mesh)) { continue; }

        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Mesh;
        }
    }
    return Best;
}

UStaticMesh* ANWPlayerEquipmentVisualDirector::FindStaticArmorMesh(const FNWGeneratedItem& Item) const
{
    if (Item.Slot != ENWEquipmentSlot::Head) { return nullptr; }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    UStaticMesh* Best = nullptr;
    for (const FAssetData& Asset : StaticAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search) || (!Search.Contains(TEXT("helmet")) && !Search.Contains(TEXT("helm")) && !Search.Contains(TEXT("headgear")))) { continue; }
        if (Search.Contains(TEXT("weapon")) || Search.Contains(TEXT("shield")) || Search.Contains(TEXT("statue")) || Search.Contains(TEXT("environment"))) { continue; }

        int32 Score = 60;
        if (!Item.StyleId.IsNone() && Search.Contains(Item.StyleId.ToString().ToLower())) Score += 90;
        if (Search.Contains(TEXT("armor")) || Search.Contains(TEXT("armour")) || Search.Contains(TEXT("knight"))) Score += 45;
        if (Search.Contains(TEXT("realistic")) || Search.Contains(TEXT("pbr")) || Search.Contains(TEXT("medieval")) || Search.Contains(TEXT("fab"))) Score += 40;

        if (Score <= BestScore) { continue; }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset())) { BestScore = Score; Best = Mesh; }
    }
    return Best;
}

USkeletalMesh* ANWPlayerEquipmentVisualDirector::FindBaseBodyMesh(ANWCharacter* Character) const
{
    if (!Character || !Character->GetMesh()) { return nullptr; }
    USkeletalMesh* Current = Character->GetMesh()->GetSkeletalMeshAsset();
    if (!Current) { return nullptr; }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* Best = nullptr;
    for (const FAssetData& Asset : SkeletalAssets)
    {
        const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (IsUnsafePath(Search)) { continue; }
        if (!ContainsAny(Search, { TEXT("basebody"), TEXT("base_body"), TEXT("underwear"), TEXT("underlayer") })) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh || Mesh->HasActiveClothingAssets() || !AreLeaderPoseMeshesCompatible(Current, Mesh)) { continue; }
        int32 Score = 50;
        if (Search.Contains(TEXT("underwear")) || Search.Contains(TEXT("underlayer"))) Score += 50;
        if (Score > BestScore) { BestScore = Score; Best = Mesh; }
    }
    return Best;
}

UStaticMeshComponent* ANWPlayerEquipmentVisualDirector::EnsureWeaponComponent(
    ANWCharacter* Character,
    TWeakObjectPtr<UStaticMeshComponent>& Existing,
    const FName Name) const
{
    if (Existing.IsValid()) { return Existing.Get(); }
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Character,
        MakeUniqueObjectName(Character, UStaticMeshComponent::StaticClass(), Name), RF_Transient);
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

USkeletalMeshComponent* ANWPlayerEquipmentVisualDirector::EnsureArmorComponent(
    ANWCharacter* Character,
    FPlayerGearState& State,
    ENWEquipmentSlot Slot) const
{
    const uint8 Key = static_cast<uint8>(Slot);
    if (TWeakObjectPtr<USkeletalMeshComponent>* Existing = State.ArmorParts.Find(Key))
    {
        if (Existing->IsValid()) { return Existing->Get(); }
    }

    const FName Name(*FString::Printf(TEXT("NWV8_ArmorSkeletal_%d"), static_cast<int32>(Slot)));
    USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Character,
        MakeUniqueObjectName(Character, USkeletalMeshComponent::StaticClass(), Name), RF_Transient);
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

UStaticMeshComponent* ANWPlayerEquipmentVisualDirector::EnsureStaticArmorComponent(
    ANWCharacter* Character,
    FPlayerGearState& State,
    ENWEquipmentSlot Slot) const
{
    const uint8 Key = static_cast<uint8>(Slot);
    if (TWeakObjectPtr<UStaticMeshComponent>* Existing = State.StaticArmorParts.Find(Key))
    {
        if (Existing->IsValid()) { return Existing->Get(); }
    }

    const FName Name(*FString::Printf(TEXT("NWV8_ArmorStatic_%d"), static_cast<int32>(Slot)));
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Character,
        MakeUniqueObjectName(Character, UStaticMeshComponent::StaticClass(), Name), RF_Transient);
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
        ? TArray<FName>{ TEXT("weapon_r"), TEXT("WeaponSocket"), TEXT("weapon_socket_r"), TEXT("hand_rSocket"), TEXT("RightHand"), TEXT("hand_r") }
        : TArray<FName>{ TEXT("weapon_l"), TEXT("ShieldSocket"), TEXT("weapon_socket_l"), TEXT("hand_lSocket"), TEXT("LeftHand"), TEXT("hand_l") };

    for (FName Candidate : Candidates)
    {
        if (Character->GetMesh()->DoesSocketExist(Candidate) || Character->GetMesh()->GetBoneIndex(Candidate) != INDEX_NONE) return Candidate;
    }
    return NAME_None;
}

FName ANWPlayerEquipmentVisualDirector::FindArmorAttachPoint(ANWCharacter* Character, ENWEquipmentSlot Slot) const
{
    if (!Character || !Character->GetMesh()) { return NAME_None; }
    TArray<FName> Candidates;
    switch (Slot)
    {
        case ENWEquipmentSlot::Head: Candidates = { TEXT("head"), TEXT("Head"), TEXT("neck_02"), TEXT("neck_01") }; break;
        case ENWEquipmentSlot::Chest: Candidates = { TEXT("spine_05"), TEXT("spine_04"), TEXT("spine_03"), TEXT("spine_02"), TEXT("chest") }; break;
        case ENWEquipmentSlot::Gloves: Candidates = { TEXT("hand_r"), TEXT("RightHand"), TEXT("lowerarm_r") }; break;
        case ENWEquipmentSlot::Legs: Candidates = { TEXT("pelvis"), TEXT("Pelvis"), TEXT("thigh_r") }; break;
        case ENWEquipmentSlot::Boots: Candidates = { TEXT("foot_r"), TEXT("Foot_R"), TEXT("calf_r") }; break;
        default: break;
    }

    for (FName Candidate : Candidates)
    {
        if (Character->GetMesh()->DoesSocketExist(Candidate) || Character->GetMesh()->GetBoneIndex(Candidate) != INDEX_NONE) return Candidate;
    }
    return NAME_None;
}

FTransform ANWPlayerEquipmentVisualDirector::GetStaticArmorRelativeTransform(ENWEquipmentSlot Slot, UStaticMesh* Mesh) const
{
    float TargetDimension = 40.0f;
    switch (Slot)
    {
        case ENWEquipmentSlot::Head: TargetDimension = 40.0f; break;
        case ENWEquipmentSlot::Chest: TargetDimension = 78.0f; break;
        case ENWEquipmentSlot::Gloves: TargetDimension = 28.0f; break;
        case ENWEquipmentSlot::Legs: TargetDimension = 76.0f; break;
        case ENWEquipmentSlot::Boots: TargetDimension = 34.0f; break;
        default: break;
    }
    const float Scale = ComputeStaticScale(Mesh, TargetDimension);
    return FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(Scale));
}

FTransform ANWPlayerEquipmentVisualDirector::GetWeaponRelativeTransform(ENWWeaponType Type, bool bRight, UStaticMesh* Mesh) const
{
    if (!Mesh) { return FTransform::Identity; }

    const float Scale = ComputeStaticScale(Mesh, WeaponTargetDimension(Type, bRight));
    const FBoxSphereBounds B = Mesh->GetBounds();
    int32 LongAxis = 0;
    const float Extent = LongestExtent(B, LongAxis);

    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;

    // Se o artista colocou o pivot proximo ao centro, deslocamos a arma para o
    // punho ficar proximo de uma extremidade. Se o pivot ja esta no cabo, preserva.
    const float OriginAxis = LongAxis == 0 ? static_cast<float>(B.Origin.X)
        : (LongAxis == 1 ? static_cast<float>(B.Origin.Y) : static_cast<float>(B.Origin.Z));
    const bool bCenteredPivot = Extent > 1.0f && FMath::Abs(OriginAxis) <= Extent * 0.28f;

    if (bCenteredPivot && Type != ENWWeaponType::Bow && Type != ENWWeaponType::SwordShield)
    {
        const float Fraction = (Type == ENWWeaponType::Staff || Type == ENWWeaponType::Greatsword) ? 0.70f : 0.58f;
        const float Shift = -Extent * Scale * Fraction;
        if (LongAxis == 0) Location.X = Shift;
        else if (LongAxis == 1) Location.Y = Shift;
        else Location.Z = Shift;
    }

    // Hand sockets de rigs UE/Paragon ja carregam orientacao; evitamos rotacoes
    // absolutas agressivas. Apenas espelhamos levemente a segunda adaga/espada.
    if (!bRight && (Type == ENWWeaponType::Daggers || Type == ENWWeaponType::DualSwords))
    {
        Rotation.Roll = 180.0f;
    }

    return FTransform(Rotation, Location, FVector(Scale));
}

float ANWPlayerEquipmentVisualDirector::ComputeStaticScale(UStaticMesh* Mesh, float TargetDimension) const
{
    if (!Mesh) { return 1.0f; }
    const FBoxSphereBounds B = Mesh->GetBounds();
    const float MaxDim = FMath::Max3(
        static_cast<float>(B.BoxExtent.X * 2.0),
        static_cast<float>(B.BoxExtent.Y * 2.0),
        static_cast<float>(B.BoxExtent.Z * 2.0));
    return (!FMath::IsFinite(MaxDim) || MaxDim <= 1.0f)
        ? 1.0f
        : FMath::Clamp(TargetDimension / MaxDim, 0.025f, 3.0f);
}

float ANWPlayerEquipmentVisualDirector::WeaponTargetDimension(ENWWeaponType Type, bool bRight) const
{
    switch (Type)
    {
        case ENWWeaponType::Staff: return 188.0f;
        case ENWWeaponType::Greatsword: return 158.0f;
        case ENWWeaponType::DualSwords: return 108.0f;
        case ENWWeaponType::SwordShield: return bRight ? 108.0f : 78.0f;
        case ENWWeaponType::Daggers: return 54.0f;
        case ENWWeaponType::Bow: return 145.0f;
        case ENWWeaponType::Firearm: return 130.0f;
        default: return 110.0f;
    }
}

uint32 ANWPlayerEquipmentVisualDirector::BuildArmorSignature(const ANWCharacter* Character) const
{
    uint32 Signature = 0x56384752u;
    if (!Character) { return Signature; }
    for (const FNWGeneratedItem& Item : Character->GetEquippedItems())
    {
        Signature = HashCombine(Signature, GetTypeHash(Item.ItemSeed));
        Signature = HashCombine(Signature, GetTypeHash(Item.AppearanceSeed));
        Signature = HashCombine(Signature, GetTypeHash(Item.StyleId));
        Signature = HashCombine(Signature, GetTypeHash(static_cast<uint8>(Item.Slot)));
    }
    return Signature;
}

FName ANWPlayerEquipmentVisualDirector::GetActiveWeaponStyle(const ANWCharacter* Character) const
{
    if (!Character) { return NAME_None; }
    for (const FNWGeneratedItem& Item : Character->GetEquippedWeaponItems())
    {
        if (Item.Kind == ENWItemKind::Weapon && Item.WeaponType == Character->GetActiveWeapon()) return Item.StyleId;
    }
    return NAME_None;
}
