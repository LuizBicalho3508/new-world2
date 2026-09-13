#include "NWDungeonSite.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "NWDungeonGuardian.h"
#include "NWEnemy.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    void NormalizeDungeonMesh(UHierarchicalInstancedStaticMeshComponent* Component)
    {
        if (!Component) { return; }
        UStaticMesh* Mesh = Component->GetStaticMesh();
        if (!Mesh)
        {
            Component->SetRelativeScale3D(FVector::OneVector);
            return;
        }

        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const float MaxDimension = FMath::Max3(
            static_cast<float>(Bounds.BoxExtent.X * 2.0),
            static_cast<float>(Bounds.BoxExtent.Y * 2.0),
            static_cast<float>(Bounds.BoxExtent.Z * 2.0));
        const float NormalizedScale = (!FMath::IsFinite(MaxDimension) || MaxDimension <= 1.0f)
            ? 1.0f
            : FMath::Clamp(100.0f / MaxDimension, 0.01f, 12.0f);
        Component->SetRelativeScale3D(FVector(NormalizedScale));
    }

    bool IsUnsafeDungeonAssetPath(const FString& InPath)
    {
        const FString Path = InPath.ToLower();
        static const TCHAR* Blocked[] = {
            TEXT("/character"), TEXT("/weapon"), TEXT("/armor"), TEXT("/armour"),
            TEXT("/animation"), TEXT("/vfx"), TEXT("/fx/"), TEXT("/ui/"),
            TEXT("preview"), TEXT("tutorial"), TEXT("collision"), TEXT("proxy"),
            TEXT("lowpoly"), TEXT("stylized"), TEXT("cartoon")
        };
        for (const TCHAR* Token : Blocked)
        {
            if (Path.Contains(Token)) { return true; }
        }
        return false;
    }
}

ANWDungeonSite::ANWDungeonSite()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PrimaryStructures = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PrimaryStructures"));
    PrimaryStructures->SetupAttachment(SceneRoot);
    PrimaryStructures->SetCollisionProfileName(TEXT("BlockAll"));

    SecondaryStructures = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SecondaryStructures"));
    SecondaryStructures->SetupAttachment(SceneRoot);
    SecondaryStructures->SetCollisionProfileName(TEXT("BlockAll"));

    DungeonLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DungeonLabel"));
    DungeonLabel->SetupAttachment(SceneRoot);
    DungeonLabel->SetHorizontalAlignment(EHTA_Center);
    DungeonLabel->SetWorldSize(62.0f);
    DungeonLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 760.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (CubeMesh.Succeeded()) { PrimaryStructures->SetStaticMesh(CubeMesh.Object); }
    if (SphereMesh.Succeeded()) { SecondaryStructures->SetStaticMesh(SphereMesh.Object); }
}

void ANWDungeonSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ANWDungeonSite, DungeonType);
    DOREPLIFETIME(ANWDungeonSite, DungeonSeed);
    DOREPLIFETIME(ANWDungeonSite, DungeonTier);
}

void ANWDungeonSite::ConfigureDungeon(ENWDungeonType InType, int32 InSeed, int32 InTier)
{
    DungeonType = InType;
    DungeonSeed = InSeed;
    DungeonTier = FMath::Clamp(InTier, 1, 10);

    if (HasAuthority()) { ForceNetUpdate(); }

    if (HasActorBegunPlay())
    {
        BuildDungeonGeometry();
        if (HasAuthority()) { SpawnDungeonPopulation(); }
    }
}

void ANWDungeonSite::OnRep_DungeonConfiguration()
{
    if (HasActorBegunPlay()) { BuildDungeonGeometry(); }
}

void ANWDungeonSite::BeginPlay()
{
    Super::BeginPlay();
    BuildDungeonGeometry();
    if (HasAuthority())
    {
        SpawnDungeonPopulation();
    }
}

FName ANWDungeonSite::GetDungeonThemeName() const
{
    return DungeonType == ENWDungeonType::DarkCastle ? FName(TEXT("DarkCastle")) : FName(TEXT("AncientCave"));
}

void ANWDungeonSite::BuildDungeonGeometry()
{
    PrimaryStructures->ClearInstances();
    SecondaryStructures->ClearInstances();

    if (bUseInstalledDungeonMeshes)
    {
        if (DungeonType == ENWDungeonType::DarkCastle)
        {
            if (UStaticMesh* CastleMesh = FindInstalledMesh({ TEXT("Gothic"), TEXT("Castle"), TEXT("Fortress"), TEXT("Wall"), TEXT("Arch") }))
            {
                PrimaryStructures->SetStaticMesh(CastleMesh);
            }
            if (UStaticMesh* PropMesh = FindInstalledMesh({ TEXT("Pillar"), TEXT("Statue"), TEXT("Gargoyle"), TEXT("Ruins"), TEXT("Gate") }))
            {
                SecondaryStructures->SetStaticMesh(PropMesh);
            }
        }
        else
        {
            if (UStaticMesh* CaveMesh = FindInstalledMesh({ TEXT("Cave"), TEXT("Rock"), TEXT("Boulder"), TEXT("Cliff") }))
            {
                PrimaryStructures->SetStaticMesh(CaveMesh);
            }
            if (UStaticMesh* CrystalMesh = FindInstalledMesh({ TEXT("Crystal"), TEXT("Stalag"), TEXT("Mushroom"), TEXT("CavePlant") }))
            {
                SecondaryStructures->SetStaticMesh(CrystalMesh);
            }
        }

        NormalizeDungeonMesh(PrimaryStructures);
        NormalizeDungeonMesh(SecondaryStructures);
        UE_LOG(LogTemp, Warning, TEXT("[DUNGEON-V7] %s usa meshes instalados normalizados | primary=%s | secondary=%s"),
            *GetDungeonThemeName().ToString(),
            PrimaryStructures->GetStaticMesh() ? *PrimaryStructures->GetStaticMesh()->GetPathName() : TEXT("none"),
            SecondaryStructures->GetStaticMesh() ? *SecondaryStructures->GetStaticMesh()->GetPathName() : TEXT("none"));
    }
    else
    {
        PrimaryStructures->SetRelativeScale3D(FVector::OneVector);
        SecondaryStructures->SetRelativeScale3D(FVector::OneVector);
        UE_LOG(LogTemp, Display, TEXT("[DUNGEON-VISUAL] modo seguro: geometria usa primitives autorados para escala previsivel."));
    }

    FRandomStream Random(DungeonSeed);
    if (DungeonType == ENWDungeonType::DarkCastle) { BuildDarkCastle(Random); }
    else { BuildAncientCave(Random); }

    const FString Label = DungeonType == ENWDungeonType::DarkCastle
        ? FString::Printf(TEXT("CASTELO SOMBRIO  |  TIER %d  |  GUARDIOES LENDARIOS"), DungeonTier)
        : FString::Printf(TEXT("CAVERNA ANCESTRAL  |  TIER %d  |  TESOURO LENDARIO"), DungeonTier);
    DungeonLabel->SetText(FText::FromString(Label));
    DungeonLabel->SetTextRenderColor(DungeonType == ENWDungeonType::DarkCastle ? FColor(180, 30, 210) : FColor(35, 185, 255));
}

void ANWDungeonSite::BuildDarkCastle(FRandomStream& Random)
{
    const float HalfExtent = 1450.0f;
    const int32 SegmentsPerSide = 10;
    const float Step = (HalfExtent * 2.0f) / SegmentsPerSide;

    for (int32 Side = 0; Side < 4; ++Side)
    {
        for (int32 Segment = 0; Segment <= SegmentsPerSide; ++Segment)
        {
            const float Along = -HalfExtent + Segment * Step;
            FVector Location;
            FRotator Rotation;
            if (Side == 0) { Location = FVector(Along, HalfExtent, 180.0f); Rotation = FRotator::ZeroRotator; }
            else if (Side == 1) { Location = FVector(Along, -HalfExtent, 180.0f); Rotation = FRotator::ZeroRotator; }
            else if (Side == 2) { Location = FVector(HalfExtent, Along, 180.0f); Rotation = FRotator(0.0f, 90.0f, 0.0f); }
            else { Location = FVector(-HalfExtent, Along, 180.0f); Rotation = FRotator(0.0f, 90.0f, 0.0f); }

            PrimaryStructures->AddInstance(FTransform(Rotation, Location, FVector(3.0f, 0.55f, 3.6f)));
        }
    }

    const FVector Corners[] = {
        FVector(HalfExtent, HalfExtent, 370.0f), FVector(HalfExtent, -HalfExtent, 370.0f),
        FVector(-HalfExtent, HalfExtent, 370.0f), FVector(-HalfExtent, -HalfExtent, 370.0f)
    };
    for (const FVector& Corner : Corners)
    {
        SecondaryStructures->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), Corner, FVector(3.4f, 3.4f, 7.4f)));
    }

    for (int32 Keep = 0; Keep < 5; ++Keep)
    {
        const float Angle = 2.0f * PI * Keep / 5.0f;
        const FVector Location(FMath::Cos(Angle) * 460.0f, FMath::Sin(Angle) * 460.0f, 320.0f);
        PrimaryStructures->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f), Location, FVector(4.8f, 4.8f, 6.5f)));
    }
}

void ANWDungeonSite::BuildAncientCave(FRandomStream& Random)
{
    const FVector ChamberCenters[] = {
        FVector(-900.0f, 0.0f, 120.0f), FVector(650.0f, 720.0f, 120.0f), FVector(850.0f, -780.0f, 120.0f)
    };

    for (const FVector& Center : ChamberCenters)
    {
        for (int32 Index = 0; Index < 18; ++Index)
        {
            const float Angle = 2.0f * PI * Index / 18.0f + Random.FRandRange(-0.10f, 0.10f);
            const float Radius = Random.FRandRange(620.0f, 900.0f);
            const FVector Location = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Random.FRandRange(60.0f, 260.0f));
            const FVector Scale(Random.FRandRange(1.3f, 3.8f), Random.FRandRange(1.3f, 3.8f), Random.FRandRange(1.7f, 5.2f));
            PrimaryStructures->AddInstance(FTransform(FRotator(Random.FRandRange(-20.0f, 20.0f), Random.FRandRange(0.0f, 360.0f), Random.FRandRange(-15.0f, 15.0f)), Location, Scale));
        }

        for (int32 Crystal = 0; Crystal < 9; ++Crystal)
        {
            const FVector Location = Center + FVector(Random.FRandRange(-420.0f, 420.0f), Random.FRandRange(-420.0f, 420.0f), Random.FRandRange(45.0f, 190.0f));
            const float Scale = Random.FRandRange(0.6f, 1.8f);
            SecondaryStructures->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), Location, FVector(Scale, Scale, Scale * Random.FRandRange(1.5f, 3.0f))));
        }
    }
}

void ANWDungeonSite::SpawnDungeonPopulation()
{
    if (!GetWorld() || !HasAuthority()) { return; }

    for (ANWEnemy* Enemy : SpawnedEnemies) { if (IsValid(Enemy)) { Enemy->Destroy(); } }
    for (ANWDungeonGuardian* Guardian : SpawnedGuardians) { if (IsValid(Guardian)) { Guardian->Destroy(); } }
    SpawnedEnemies.Reset();
    SpawnedGuardians.Reset();

    FRandomStream Random(DungeonSeed + DungeonTier * 913);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 Index = 0; Index < RegularEnemyCount + DungeonTier * 2; ++Index)
    {
        const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
        const float Radius = Random.FRandRange(300.0f, 1280.0f);
        const FVector Local(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 130.0f);
        ANWEnemy* Enemy = GetWorld()->SpawnActor<ANWEnemy>(ANWEnemy::StaticClass(), GetActorLocation() + Local, FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), Params);
        if (Enemy) { SpawnedEnemies.Add(Enemy); }
    }

    for (int32 Index = 0; Index < GuardianCount; ++Index)
    {
        const float Angle = 2.0f * PI * Index / FMath::Max(1, GuardianCount);
        const FVector Local(FMath::Cos(Angle) * 420.0f, FMath::Sin(Angle) * 420.0f, 150.0f);
        ANWDungeonGuardian* Guardian = GetWorld()->SpawnActor<ANWDungeonGuardian>(ANWDungeonGuardian::StaticClass(), GetActorLocation() + Local, FRotator(0.0f, FMath::RadiansToDegrees(Angle + PI), 0.0f), Params);
        if (Guardian)
        {
            Guardian->ConfigureGuardian(GetDungeonThemeName(), DungeonTier + Index);
            SpawnedGuardians.Add(Guardian);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DUNGEON] %s gerado: tier=%d, mobs=%d, guardioes=%d"), *GetDungeonThemeName().ToString(), DungeonTier, SpawnedEnemies.Num(), SpawnedGuardians.Num());
}

UStaticMesh* ANWDungeonSite::FindInstalledMesh(const TArray<FString>& Keywords) const
{
    if (!bUseInstalledDungeonMeshes)
    {
        return nullptr;
    }

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);

    int32 BestScore = 0;
    FAssetData BestAsset;
    for (const FAssetData& Asset : Assets)
    {
        const FString Searchable = Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString();
        if (IsUnsafeDungeonAssetPath(Searchable)) { continue; }

        int32 Score = 0;
        for (const FString& Keyword : Keywords)
        {
            if (Searchable.Contains(Keyword, ESearchCase::IgnoreCase)) { Score += 12; }
        }

        if (Searchable.Contains(TEXT("Fab"), ESearchCase::IgnoreCase) || Searchable.Contains(TEXT("Megascans"), ESearchCase::IgnoreCase)) { Score += 12; }
        if (Searchable.Contains(TEXT("PBR"), ESearchCase::IgnoreCase) || Searchable.Contains(TEXT("Realistic"), ESearchCase::IgnoreCase)) { Score += 10; }
        if (Searchable.Contains(TEXT("Ruins"), ESearchCase::IgnoreCase) || Searchable.Contains(TEXT("Medieval"), ESearchCase::IgnoreCase)) { Score += 12; }

        if (DungeonType == ENWDungeonType::DarkCastle)
        {
            if (Searchable.Contains(TEXT("Gothic"), ESearchCase::IgnoreCase)) { Score += 30; }
            if (Searchable.Contains(TEXT("Wall"), ESearchCase::IgnoreCase) || Searchable.Contains(TEXT("Arch"), ESearchCase::IgnoreCase) || Searchable.Contains(TEXT("Pillar"), ESearchCase::IgnoreCase)) { Score += 18; }
            if (Searchable.Contains(TEXT("Castle"), ESearchCase::IgnoreCase) || Searchable.Contains(TEXT("Dark"), ESearchCase::IgnoreCase)) { Score += 8; }
        }
        else
        {
            if (Searchable.Contains(TEXT("Soul"), ESearchCase::IgnoreCase) && Searchable.Contains(TEXT("Cave"), ESearchCase::IgnoreCase)) { Score += 34; }
            if (Searchable.Contains(TEXT("Dungeon"), ESearchCase::IgnoreCase) || Searchable.Contains(TEXT("Cave"), ESearchCase::IgnoreCase)) { Score += 16; }
            if (Searchable.Contains(TEXT("Rock"), ESearchCase::IgnoreCase) || Searchable.Contains(TEXT("Stalag"), ESearchCase::IgnoreCase)) { Score += 8; }
        }

        if (Searchable.Contains(TEXT("KiteDemo"), ESearchCase::IgnoreCase)) { Score -= 15; }
        if (Score > BestScore)
        {
            BestScore = Score;
            BestAsset = Asset;
        }
    }

    return BestScore > 0 ? Cast<UStaticMesh>(BestAsset.GetAsset()) : nullptr;
}
