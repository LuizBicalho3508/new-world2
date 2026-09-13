#include "NWEnemyVisualDirector.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "NWCharacter.h"
#include "NWEnemy.h"

namespace
{
    bool IsUnsafeRuntimeAssetPath(const FString& InPath)
    {
        const FString Path = InPath.ToLower();
        static const TCHAR* Blocked[] = {
            TEXT("animstarterpack"),
            TEXT("free_magic/demo"),
            TEXT("deformablesnowsystem/demo"),
            TEXT("/demo/"),
            TEXT("/preview"),
            TEXT("/tutorial"),
            TEXT("/test/"),
            TEXT("/fx/skeletalmeshes/"),
            TEXT("_proto"),
            TEXT("/buff/"),
            TEXT("/skins/")
        };
        for (const TCHAR* Token : Blocked)
        {
            if (Path.Contains(Token)) { return true; }
        }
        return false;
    }

    bool IsSafeRuntimeMesh(USkeletalMesh* Mesh)
    {
        return Mesh && Mesh->GetSkeleton() && !Mesh->HasActiveClothingAssets();
    }

    int32 ArchetypeSalt(const ANWEnemy* Enemy)
    {
        if (!Enemy) { return 0; }
        return static_cast<int32>(Enemy->GetEnemyArchetype()) * 97 + (Enemy->IsWorldBoss() ? 997 : 0);
    }
}

ANWEnemyVisualDirector::ANWEnemyVisualDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.35f;
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

    int32 SafePathMeshes = 0;
    int32 CreatureCandidates = 0;
    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        const FString Path = Asset.PackageName.ToString().ToLower();
        if (IsUnsafeRuntimeAssetPath(Path)) { continue; }
        ++SafePathMeshes;
        if (Path.Contains(TEXT("minion")) || Path.Contains(TEXT("monster")) || Path.Contains(TEXT("creature")) ||
            Path.Contains(TEXT("undead")) || Path.Contains(TEXT("zombie")) || Path.Contains(TEXT("orc")) ||
            Path.Contains(TEXT("paragongrux")) || Path.Contains(TEXT("paragonkhaimera")) ||
            Path.Contains(TEXT("paragonrampage")) || Path.Contains(TEXT("paragonsevarog")) ||
            Path.Contains(TEXT("paragonrevenant")) || Path.Contains(TEXT("paragoncountess")))
        {
            ++CreatureCandidates;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[MOB-VISUAL-V7] catalogo skeletal=%d | caminhos seguros=%d | candidatos criatura=%d | AnimBP nao e requisito"),
        SkeletalMeshAssets.Num(), SafePathMeshes, CreatureCandidates);
}

void ANWEnemyVisualDirector::RefreshEnemyVisuals()
{
    if (!GetWorld()) { return; }

    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy)) { continue; }

        const int32 Signature = ArchetypeSalt(Enemy);
        const int32* Applied = AppliedSignatures.Find(Enemy);
        if (Applied && *Applied == Signature && Enemy->GetMesh() && Enemy->GetMesh()->GetSkeletalMeshAsset()) { continue; }

        if (ApplyVisual(Enemy)) { AppliedSignatures.Add(Enemy, Signature); }
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
            if (WeaponPath.Contains(TEXT("/Engine/BasicShapes/"), ESearchCase::IgnoreCase))
            {
                Component->SetVisibility(false, true);
                Component->SetHiddenInGame(true, true);
                ++Hidden;
            }
        }

        if (Hidden > 0 && !LoggedWeaponSafetyCharacters.Contains(Character))
        {
            LoggedWeaponSafetyCharacters.Add(Character);
            UE_LOG(LogTemp, Warning, TEXT("[WEAPON-VISUAL-V7] %s: %d placeholder(s) legado(s) ocultado(s)."),
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

    USkeletalMesh* Mesh = FindBestMeshForEnemy(Enemy);
    if (!Mesh)
    {
        HideDebugMeshes(Enemy);
        Enemy->GetMesh()->SetVisibility(false, true);
        UE_LOG(LogTemp, Warning, TEXT("[MOB-VISUAL-V7] nenhum skeletal mesh seguro encontrado para %s."), *Enemy->GetName());
        return false;
    }

    if (!IsSafeRuntimeMesh(Mesh))
    {
        UE_LOG(LogTemp, Error, TEXT("[MOB-VISUAL-V7] mesh insegura bloqueada: %s"), *Mesh->GetPathName());
        return false;
    }

    Enemy->GetMesh()->SetSkeletalMeshAsset(Mesh);
    Enemy->GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Enemy->GetMesh()->SetAnimInstanceClass(nullptr);

    const bool bBoss = Enemy->IsWorldBoss();
    float TargetHeight = bBoss ? 340.0f : 190.0f;
    if (!bBoss)
    {
        switch (Enemy->GetEnemyArchetype())
        {
            case ENWEnemyArchetype::Zombie: TargetHeight = 188.0f; break;
            case ENWEnemyArchetype::Ghost: TargetHeight = 202.0f; break;
            case ENWEnemyArchetype::Brute: TargetHeight = 226.0f; break;
            default: break;
        }
    }

    const float SizeVariation = 0.95f + static_cast<float>((Enemy->GetUniqueID() + ArchetypeSalt(Enemy)) % 7u) * 0.018f;
    Enemy->GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, bBoss ? -124.0f : -94.0f));
    Enemy->GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    Enemy->GetMesh()->SetRelativeScale3D(FVector(ComputeScale(Mesh, TargetHeight) * SizeVariation));
    Enemy->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Enemy->GetMesh()->SetVisibility(true, true);

    FLinearColor ArchetypeTint = FLinearColor::White;
    if (Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Zombie) { ArchetypeTint = FLinearColor(0.68f, 0.78f, 0.58f, 1.0f); }
    else if (Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Ghost) { ArchetypeTint = FLinearColor(0.52f, 0.72f, 1.0f, 1.0f); }
    else { ArchetypeTint = FLinearColor(1.0f, 0.74f, 0.62f, 1.0f); }
    Enemy->GetMesh()->SetColorParameterValueOnMaterials(TEXT("Color"), ArchetypeTint);
    Enemy->GetMesh()->SetColorParameterValueOnMaterials(TEXT("BaseColor"), ArchetypeTint);

    HideDebugMeshes(Enemy);
    UE_LOG(LogTemp, Warning, TEXT("[MOB-VISUAL-V7] %s archetype=%d boss=%s -> %s | sequence-driven | hp=%.0f"),
        *Enemy->GetName(), static_cast<int32>(Enemy->GetEnemyArchetype()), Enemy->IsWorldBoss() ? TEXT("sim") : TEXT("nao"),
        *Mesh->GetPathName(), Enemy->GetMaxHealth());
    return true;
}

USkeletalMesh* ANWEnemyVisualDirector::FindBestMeshForEnemy(const ANWEnemy* Enemy) const
{
    if (!Enemy) { return nullptr; }

    if (Enemy->IsWorldBoss())
    {
        if (USkeletalMesh* Mesh = FindBestByKeywords(
            { TEXT("Boss"), TEXT("Demon"), TEXT("Monster"), TEXT("Warlord"), TEXT("Giant"), TEXT("Ogre"), TEXT("Troll"), TEXT("Rampage"), TEXT("Sevarog") },
            { TEXT("Rampage"), TEXT("Sevarog"), TEXT("Grux"), TEXT("Khaimera"), TEXT("Creature"), TEXT("Enemy"), TEXT("Dark") }))
        {
            return Mesh;
        }
    }

    switch (Enemy->GetEnemyArchetype())
    {
        case ENWEnemyArchetype::Zombie:
            if (USkeletalMesh* Mesh = FindBestByKeywords(
                { TEXT("Zombie"), TEXT("Undead"), TEXT("Ghoul"), TEXT("Skeleton"), TEXT("Corpse"), TEXT("Revenant"), TEXT("Minion") },
                { TEXT("Revenant"), TEXT("Khaimera"), TEXT("Minion"), TEXT("Enemy"), TEXT("Monster"), TEXT("Creature") }))
            {
                return Mesh;
            }
            break;

        case ENWEnemyArchetype::Ghost:
            if (USkeletalMesh* Mesh = FindBestByKeywords(
                { TEXT("Ghost"), TEXT("Wraith"), TEXT("Specter"), TEXT("Spectre"), TEXT("Spirit"), TEXT("Phantom"), TEXT("Sevarog"), TEXT("Countess") },
                { TEXT("Sevarog"), TEXT("Countess"), TEXT("Dark"), TEXT("Undead"), TEXT("Enemy") }))
            {
                return Mesh;
            }
            break;

        case ENWEnemyArchetype::Brute:
        default:
            if (USkeletalMesh* Mesh = FindBestByKeywords(
                { TEXT("Brute"), TEXT("Orc"), TEXT("Ogre"), TEXT("Troll"), TEXT("Grux"), TEXT("Rampage"), TEXT("Khaimera"), TEXT("Minion") },
                { TEXT("Grux"), TEXT("Rampage"), TEXT("Khaimera"), TEXT("Minion"), TEXT("Heavy"), TEXT("Creature"), TEXT("Enemy") }))
            {
                return Mesh;
            }
            break;
    }

    return FindBestParagonFallback(Enemy);
}

USkeletalMesh* ANWEnemyVisualDirector::FindBestByKeywords(const TArray<FString>& Primary, const TArray<FString>& Preferred) const
{
    int32 BestScore = TNumericLimits<int32>::Lowest();
    USkeletalMesh* BestMesh = nullptr;

    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        const int32 Score = ScoreAsset(Asset, Primary, Preferred, true);
        if (Score <= BestScore || IsUnsafeRuntimeAssetPath(Asset.PackageName.ToString())) { continue; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!IsSafeRuntimeMesh(Mesh)) { continue; }
        BestScore = Score;
        BestMesh = Mesh;
    }
    return BestMesh;
}

USkeletalMesh* ANWEnemyVisualDirector::FindBestParagonFallback(const ANWEnemy* Enemy) const
{
    struct FCandidate
    {
        int32 Score = 0;
        USkeletalMesh* Mesh = nullptr;
        FString Path;
    };

    TArray<FString> Preferred;
    if (Enemy && Enemy->IsWorldBoss())
    {
        Preferred = { TEXT("Rampage"), TEXT("Sevarog"), TEXT("Grux"), TEXT("Khaimera"), TEXT("Terra") };
    }
    else if (Enemy && Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Ghost)
    {
        Preferred = { TEXT("Sevarog"), TEXT("Countess"), TEXT("Revenant") };
    }
    else if (Enemy && Enemy->GetEnemyArchetype() == ENWEnemyArchetype::Zombie)
    {
        Preferred = { TEXT("Revenant"), TEXT("Khaimera"), TEXT("Minion") };
    }
    else
    {
        Preferred = { TEXT("Grux"), TEXT("Rampage"), TEXT("Khaimera"), TEXT("Minion"), TEXT("Terra") };
    }

    TArray<FCandidate> Candidates;
    for (const FAssetData& Asset : SkeletalMeshAssets)
    {
        const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
        if (!Searchable.Contains(TEXT("paragon")) || IsUnsafeRuntimeAssetPath(Searchable)) { continue; }
        if (Searchable.Contains(TEXT("weapon")) || Searchable.Contains(TEXT("preview"))) { continue; }

        int32 Score = 15;
        for (const FString& Keyword : Preferred)
        {
            if (Searchable.Contains(Keyword.ToLower())) { Score += 110; }
        }
        if (Searchable.Contains(TEXT("minion"))) { Score += 55; }
        if (Searchable.Contains(TEXT("hero"))) { Score += 8; }
        if (Searchable.Contains(TEXT("greystone"))) { Score -= 600; }
        if (Searchable.Contains(TEXT("sparrow")) || Searchable.Contains(TEXT("serath"))) { Score -= 90; }

        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
        if (!IsSafeRuntimeMesh(Mesh)) { continue; }
        Candidates.Add({ Score, Mesh, Mesh->GetPathName() });
    }

    Candidates.Sort([](const FCandidate& A, const FCandidate& B)
    {
        if (A.Score != B.Score) { return A.Score > B.Score; }
        return A.Path < B.Path;
    });

    if (Candidates.IsEmpty()) { return nullptr; }
    const int32 PoolSize = FMath::Min(4, Candidates.Num());
    const uint32 Seed = Enemy ? Enemy->GetUniqueID() + static_cast<uint32>(ArchetypeSalt(Enemy)) : 0u;
    return Candidates[static_cast<int32>(Seed % static_cast<uint32>(PoolSize))].Mesh;
}

int32 ANWEnemyVisualDirector::ScoreAsset(const FAssetData& Asset, const TArray<FString>& Primary, const TArray<FString>& Preferred, bool bRequirePrimary) const
{
    const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
    if (IsUnsafeRuntimeAssetPath(Searchable)) { return TNumericLimits<int32>::Lowest(); }

    bool bPrimaryMatch = !bRequirePrimary;
    int32 Score = 0;
    for (const FString& Keyword : Primary)
    {
        if (Searchable.Contains(Keyword.ToLower()))
        {
            bPrimaryMatch = true;
            Score += 55;
        }
    }
    if (!bPrimaryMatch) { return TNumericLimits<int32>::Lowest(); }

    for (const FString& Keyword : Preferred)
    {
        if (Searchable.Contains(Keyword.ToLower())) { Score += 24; }
    }

    if (Searchable.Contains(TEXT("paragonminions"))) { Score += 100; }
    if (Searchable.Contains(TEXT("paragongrux")) || Searchable.Contains(TEXT("paragonkhaimera")) ||
        Searchable.Contains(TEXT("paragonrampage")) || Searchable.Contains(TEXT("paragonsevarog")) ||
        Searchable.Contains(TEXT("paragonrevenant")) || Searchable.Contains(TEXT("paragoncountess"))) { Score += 80; }
    if (Searchable.Contains(TEXT("realistic")) || Searchable.Contains(TEXT("pbr")) || Searchable.Contains(TEXT("fab"))) { Score += 38; }
    if (Searchable.Contains(TEXT("enemy")) || Searchable.Contains(TEXT("creature")) || Searchable.Contains(TEXT("monster"))) { Score += 28; }
    if (Searchable.Contains(TEXT("greystone"))) { Score -= 650; }
    if (Searchable.Contains(TEXT("lowpoly")) || Searchable.Contains(TEXT("stylized")) || Searchable.Contains(TEXT("cartoon")) || Searchable.Contains(TEXT("voxel"))) { Score -= 500; }
    if (Searchable.Contains(TEXT("weapon")) || Searchable.Contains(TEXT("armor")) || Searchable.Contains(TEXT("armour"))) { Score -= 220; }
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
