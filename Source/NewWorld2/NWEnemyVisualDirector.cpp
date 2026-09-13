#include "NWEnemyVisualDirector.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "NWCharacter.h"
#include "NWEnemy.h"

ANWEnemyVisualDirector::ANWEnemyVisualDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.40f;
    bReplicates = false;
}

void ANWEnemyVisualDirector::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { return; }
    ScanAssets();
    RefreshEnemyVisuals();
    StabilizePlayerWeaponVisuals();
}

void ANWEnemyVisualDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetNetMode() == NM_DedicatedServer) { return; }
    RefreshEnemyVisuals();
    StabilizePlayerWeaponVisuals();
}

void ANWEnemyVisualDirector::ScanAssets()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    FARFilter MeshFilter;
    MeshFilter.PackagePaths.Add(FName(TEXT("/Game")));
    MeshFilter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
    MeshFilter.bRecursivePaths = true;
    Registry.GetAssets(MeshFilter, SkeletalMeshAssets);

    FARFilter AnimFilter;
    AnimFilter.PackagePaths.Add(FName(TEXT("/Game")));
    AnimFilter.ClassPaths.Add(UAnimBlueprint::StaticClass()->GetClassPathName());
    AnimFilter.bRecursivePaths = true;
    Registry.GetAssets(AnimFilter, AnimBlueprintAssets);

    UE_LOG(LogTemp, Display, TEXT("[MOB-VISUAL] catalogo: skeletal=%d animbp=%d"), SkeletalMeshAssets.Num(), AnimBlueprintAssets.Num());
}

void ANWEnemyVisualDirector::RefreshEnemyVisuals()
{
    if (!GetWorld()) { return; }

    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }

        const int32 Signature = static_cast<int32>(Enemy->GetEnemyArchetype()) + (Enemy->IsWorldBoss() ? 100 : 0);
        const int32* Applied = AppliedSignatures.Find(Enemy);
        if (Applied && *Applied == Signature && Enemy->GetMesh() && Enemy->GetMesh()->GetSkeletalMeshAsset()) { continue; }

        if (ApplyVisual(Enemy))
        {
            AppliedSignatures.Add(Enemy, Signature);
        }
    }

    for (auto It = AppliedSignatures.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
}

void ANWEnemyVisualDirector::StabilizePlayerWeaponVisuals()
{
    if (!GetWorld()) { return; }

    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character) || !Character->GetMesh()) { continue; }

        const USkeletalMesh* PlayerMesh = Character->GetMesh()->GetSkeletalMeshAsset();
        const FString PlayerMeshPath = PlayerMesh ? PlayerMesh->GetPathName() : FString();
        const bool bGreystoneRig = PlayerMeshPath.Contains(TEXT("ParagonGreystone"), ESearchCase::IgnoreCase);

        TArray<UStaticMeshComponent*> Components;
        Character->GetComponents<UStaticMeshComponent>(Components);
        int32 Hidden = 0;
        for (UStaticMeshComponent* Component : Components)
        {
            if (!Component) { continue; }
            const FString ComponentName = Component->GetName();
            if (!ComponentName.Contains(TEXT("NW_WeaponVisual"), ESearchCase::IgnoreCase)) { continue; }

            const UStaticMesh* WeaponMesh = Component->GetStaticMesh();
            const FString WeaponPath = WeaponMesh ? WeaponMesh->GetPathName() : FString();
            const bool bEnginePrimitive = WeaponPath.Contains(TEXT("/Engine/BasicShapes/"), ESearchCase::IgnoreCase);

            // O Greystone usado neste vertical slice ja possui arma integrada ao visual/rig.
            // Acrescentar outra espada sobre a mao produz a duplicacao vista no playtest.
            // Para outros personagens, apenas placeholders de BasicShapes sao ocultados.
            if (bGreystoneRig || bEnginePrimitive)
            {
                Component->SetVisibility(false, true);
                Component->SetHiddenInGame(true, true);
                ++Hidden;
            }
        }

        if (Hidden > 0 && !LoggedWeaponSafetyCharacters.Contains(Character))
        {
            LoggedWeaponSafetyCharacters.Add(Character);
            UE_LOG(LogTemp, Warning, TEXT("[WEAPON-VISUAL] %s: %d visual(is) externo(s) duplicado/placeholder ocultado(s); gameplay da arma permanece ativo."),
                *Character->GetName(), Hidden);
        }
    }

    for (auto It = LoggedWeaponSafetyCharacters.CreateIterator(); It; ++It)
    {
        if (!It->IsValid()) { It.RemoveCurrent(); }
    }
}

bool ANWEnemyVisualDirector::ApplyVisual(ANWEnemy* Enemy)
{
    if (!Enemy || !Enemy->GetMesh()) { return false; }

    UClass* AnimClass = nullptr;
    USkeletalMesh* Mesh = FindBestMeshForEnemy(Enemy, AnimClass);
    if (!Mesh)
    {
        HideDebugMeshes(Enemy);
        Enemy->GetMesh()->SetVisibility(false, true);
        UE_LOG(LogTemp, Warning, TEXT("[MOB-VISUAL] nenhum skeletal mesh real encontrado para %s; placeholder geometrico foi ocultado."), *Enemy->GetName());
        return false;
    }

    Enemy->GetMesh()->SetSkeletalMeshAsset(Mesh);
    if (AnimClass)
    {
        Enemy->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        Enemy->GetMesh()->SetAnimInstanceClass(AnimClass);
    }

    const bool bBoss = Enemy->IsWorldBoss();
    float TargetHeight = bBoss ? 320.0f : 188.0f;
    if (!bBoss)
    {
        switch (Enemy->GetEnemyArchetype())
        {
            case ENWEnemyArchetype::Zombie: TargetHeight = 184.0f; break;
            case ENWEnemyArchetype::Ghost: TargetHeight = 196.0f; break;
            case ENWEnemyArchetype::Brute: TargetHeight = 218.0f; break;
            default: break;
        }
    }

    Enemy->GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, bBoss ? -118.0f : -92.0f));
    Enemy->GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    Enemy->GetMesh()->SetRelativeScale3D(FVector(ComputeScale(Mesh, TargetHeight)));
    Enemy->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Enemy->GetMesh()->SetVisibility(true, true);
    HideDebugMeshes(Enemy);

    UE_LOG(LogTemp, Warning, TEXT("[MOB-VISUAL] %s archetype=%d boss=%s -> %s | anim=%s"),
        *Enemy->GetName(),
        static_cast<int32>(Enemy->GetEnemyArchetype()),
        Enemy->IsWorldBoss() ? TEXT("sim") : TEXT("nao"),
        *Mesh->GetPathName(),
        AnimClass ? *AnimClass->GetName() : TEXT("sem AnimBP"));
    return true;
}

USkeletalMesh* ANWEnemyVisualDirector::FindBestMeshForEnemy(const ANWEnemy* Enemy, UClass*& OutAnimClass) const
{
    OutAnimClass = nullptr;
    if (!Enemy) { return nullptr; }

    if (Enemy->IsWorldBoss())
    {
        if (USkeletalMesh* Mesh = FindBestByKeywords(
            { TEXT("Boss"), TEXT("Demon"), TEXT("Monster"), TEXT("Warlord"), TEXT("Giant"), TEXT("Ogre"), TEXT("Troll") },
            { TEXT("Creature"), TEXT("Enemy"), TEXT("Undead"), TEXT("Dark"), TEXT("Realistic"), TEXT("PBR") },
            OutAnimClass))
        {
            return Mesh;
        }
    }

    switch (Enemy->GetEnemyArchetype())
    {
        case ENWEnemyArchetype::Zombie:
            if (USkeletalMesh* Mesh = FindBestByKeywords(
                { TEXT("Zombie"), TEXT("Undead"), TEXT("Ghoul"), TEXT("Skeleton"), TEXT("Corpse") },
                { TEXT("Enemy"), TEXT("Monster"), TEXT("Creature"), TEXT("Realistic"), TEXT("PBR") },
                OutAnimClass))
            {
                return Mesh;
            }
            break;

        case ENWEnemyArchetype::Ghost:
            if (USkeletalMesh* Mesh = FindBestByKeywords(
                { TEXT("Ghost"), TEXT("Wraith"), TEXT("Specter"), TEXT("Spectre"), TEXT("Spirit"), TEXT("Phantom") },
                { TEXT("Enemy"), TEXT("Monster"), TEXT("Undead"), TEXT("Dark"), TEXT("Realistic") },
                OutAnimClass))
            {
                return Mesh;
            }
            break;

        case ENWEnemyArchetype::Brute:
        default:
            if (USkeletalMesh* Mesh = FindBestByKeywords(
                { TEXT("Brute"), TEXT("Orc"), TEXT("Ogre"), TEXT("Troll"), TEXT("Warrior"), TEXT("Barbarian"), TEXT("Monster") },
                { TEXT("Enemy"), TEXT("Creature"), TEXT("Heavy"), TEXT("Realistic"), TEXT("PBR") },
                OutAnimClass))
            {
                return Mesh;
            }
            break;
    }

    return FindBestParagonFallback(Enemy, OutAnimClass);
}

USkeletalMesh* ANWEnemyVisualDirector::FindBestByKeywords(const TArray<FString>& Primary, const TArray<FString>& Preferred, UClass*& OutAnimClass) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* BestMesh = nullptr;
    UClass* BestAnim = nullptr;

    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        const int32 Score = ScoreAsset(Asset, Primary, Preferred, true);
        if (Score <= BestScore) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh || !Mesh->GetSkeleton()) { continue; }

        UClass* AnimClass = FindAnimClass(Mesh, Preferred);
        if (!AnimClass) { continue; }

        BestScore = Score;
        BestMesh = Mesh;
        BestAnim = AnimClass;
    }

    OutAnimClass = BestAnim;
    return BestMesh;
}

USkeletalMesh* ANWEnemyVisualDirector::FindBestParagonFallback(const ANWEnemy* Enemy, UClass*& OutAnimClass) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* BestMesh = nullptr;
    UClass* BestAnim = nullptr;

    TArray<FString> Preferred;
    if (Enemy && Enemy->IsWorldBoss())
    {
        Preferred = { TEXT("Rampage"), TEXT("Sevarog"), TEXT("Grux"), TEXT("Khaimera"), TEXT("Steel") };
    }
    else if (Enemy && Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Ghost)
    {
        Preferred = { TEXT("Sevarog"), TEXT("Countess"), TEXT("Wraith"), TEXT("Gideon"), TEXT("Aurora") };
    }
    else if (Enemy && Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Zombie)
    {
        Preferred = { TEXT("Revenant"), TEXT("Khaimera"), TEXT("Crunch"), TEXT("Greystone") };
    }
    else
    {
        Preferred = { TEXT("Grux"), TEXT("Steel"), TEXT("Greystone"), TEXT("Rampage") };
    }

    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (!Searchable.Contains(TEXT("paragon"))) { continue; }

        int32 Score = 20;
        for (const FString& Keyword : Preferred)
        {
            if (Searchable.Contains(Keyword.ToLower())) { Score += 80; }
        }
        if (Searchable.Contains(TEXT("hero"))) { Score += 12; }
        if (Searchable.Contains(TEXT("mesh"))) { Score += 5; }
        if (Searchable.Contains(TEXT("weapon")) || Searchable.Contains(TEXT("skin")) || Searchable.Contains(TEXT("preview"))) { Score -= 120; }
        if (Score <= BestScore) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!Mesh || !Mesh->GetSkeleton()) { continue; }
        UClass* AnimClass = FindAnimClass(Mesh, Preferred);
        if (!AnimClass) { continue; }

        BestScore = Score;
        BestMesh = Mesh;
        BestAnim = AnimClass;
    }

    OutAnimClass = BestAnim;
    return BestMesh;
}

UClass* ANWEnemyVisualDirector::FindAnimClass(USkeletalMesh* Mesh, const TArray<FString>& Preferred) const
{
    if (!Mesh || !Mesh->GetSkeleton()) { return nullptr; }

    int32 BestScore = TNumericLimits<int32>::Lowest();
    UClass* BestClass = nullptr;
    for (const FAssetData& Asset : AnimBlueprintAssets)
    {
        UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset.GetAsset());
        if (!AnimBlueprint || !AnimBlueprint->GeneratedClass || AnimBlueprint->TargetSkeleton != Mesh->GetSkeleton()) { continue; }

        const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        int32 Score = 10;
        for (const FString& Keyword : Preferred)
        {
            if (Searchable.Contains(Keyword.ToLower())) { Score += 20; }
        }
        if (Searchable.Contains(TEXT("animblueprint")) || Searchable.Contains(TEXT("anim_bp"))) { Score += 8; }
        if (Score > BestScore)
        {
            BestScore = Score;
            BestClass = AnimBlueprint->GeneratedClass;
        }
    }
    return BestClass;
}

int32 ANWEnemyVisualDirector::ScoreAsset(const FAssetData& Asset, const TArray<FString>& Primary, const TArray<FString>& Preferred, bool bRequirePrimary) const
{
    const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
    bool bPrimaryMatch = !bRequirePrimary;
    int32 Score = 0;

    for (const FString& Keyword : Primary)
    {
        if (Searchable.Contains(Keyword.ToLower()))
        {
            bPrimaryMatch = true;
            Score += 45;
        }
    }
    if (!bPrimaryMatch) { return TNumericLimits<int32>::Lowest(); }

    for (const FString& Keyword : Preferred)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 16; }
    }

    if (Searchable.Contains(TEXT("realistic")) || Searchable.Contains(TEXT("pbr"))) { Score += 35; }
    if (Searchable.Contains(TEXT("enemy")) || Searchable.Contains(TEXT("creature")) || Searchable.Contains(TEXT("monster"))) { Score += 25; }
    if (Searchable.Contains(TEXT("lowpoly")) || Searchable.Contains(TEXT("stylized")) || Searchable.Contains(TEXT("cartoon"))) { Score -= 400; }
    if (Searchable.Contains(TEXT("weapon")) || Searchable.Contains(TEXT("armor")) || Searchable.Contains(TEXT("armour"))) { Score -= 180; }

    return Score;
}

float ANWEnemyVisualDirector::ComputeScale(USkeletalMesh* Mesh, float TargetHeight) const
{
    if (!Mesh) { return 1.0f; }
    const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
    const float Height = static_cast<float>(Bounds.BoxExtent.Z * 2.0);
    if (!FMath::IsFinite(Height) || Height <= 1.0f) { return 1.0f; }
    return FMath::Clamp(TargetHeight / Height, 0.20f, 2.8f);
}

void ANWEnemyVisualDirector::HideDebugMeshes(ANWEnemy* Enemy) const
{
    if (!Enemy) { return; }
    TArray<UStaticMeshComponent*> Components;
    Enemy->GetComponents<UStaticMeshComponent>(Components);
    for (UStaticMeshComponent* Component : Components)
    {
        if (Component) { Component->SetVisibility(false, true); }
    }
}
