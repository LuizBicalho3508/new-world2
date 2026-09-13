#include "NWPremiumVFXDirector.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NWCharacter.h"
#include "NWCombatLibrary.h"
#include "TimerManager.h"

ANWPremiumVFXDirector::ANWPremiumVFXDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = TickInterval;
    bReplicates = false;
}

void ANWPremiumVFXDirector::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { return; }

    ScanAndCacheVFX();
    WarmupCachedSystems();

    UE_LOG(LogTemp, Warning, TEXT("[VFX-V3] diretor premium ativo | direct=%d area=%d heal=%d | camadas/cast=%d"),
        DirectDamageSystems.Num(), AreaDamageSystems.Num(), HealSystems.Num(), LayersPerCast);
}

void ANWPremiumVFXDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetNetMode() == NM_DedicatedServer || !GetWorld()) { return; }
    DetectCasts();
}

bool ANWPremiumVFXDirector::IsSafeVFXAsset(const FAssetData& Asset) const
{
    const FString Path = Asset.PackageName.ToString().ToLower();

    static const TCHAR* BlockedTokens[] = {
        TEXT("/demo/"),
        TEXT("/tutorial"),
        TEXT("animstarterpack"),
        TEXT("deformablesnowsystem/demo"),
        TEXT("free_magic/demo"),
        TEXT("smokepufflight"),
        TEXT("preview"),
        TEXT("benchmark"),
        TEXT("testmap"),
        TEXT("debug")
    };

    for (const TCHAR* Token : BlockedTokens)
    {
        if (Path.Contains(Token)) { return false; }
    }
    return true;
}

int32 ANWPremiumVFXDirector::ScoreVFXAsset(const FAssetData& Asset, const TArray<FString>& Keywords) const
{
    if (!IsSafeVFXAsset(Asset)) { return TNumericLimits<int32>::Lowest(); }

    const FString Searchable = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
    int32 Score = 0;
    bool bKeywordMatch = false;

    for (const FString& Keyword : Keywords)
    {
        if (Searchable.Contains(Keyword.ToLower()))
        {
            Score += 55;
            bKeywordMatch = true;
        }
    }
    if (!bKeywordMatch) { return TNumericLimits<int32>::Lowest(); }

    if (Searchable.Contains(TEXT("/fab/"))) { Score += 55; }
    if (Searchable.Contains(TEXT("vfx"))) { Score += 35; }
    if (Searchable.Contains(TEXT("magic"))) { Score += 28; }
    if (Searchable.Contains(TEXT("niagara"))) { Score += 22; }
    if (Searchable.Contains(TEXT("fx"))) { Score += 16; }
    if (Searchable.Contains(TEXT("impact")) || Searchable.Contains(TEXT("burst"))) { Score += 12; }
    if (Searchable.Contains(TEXT("loop")) || Searchable.Contains(TEXT("ambient"))) { Score -= 20; }
    if (Searchable.Contains(TEXT("smoke")) && !Searchable.Contains(TEXT("magic"))) { Score -= 22; }
    if (Searchable.Contains(TEXT("wood")) || Searchable.Contains(TEXT("debuff"))) { Score -= 80; }

    return Score;
}

void ANWPremiumVFXDirector::SelectSystems(
    const TArray<FAssetData>& Assets,
    const TArray<FString>& Keywords,
    TArray<TObjectPtr<UNiagaraSystem>>& OutSystems,
    int32 MaxSystems)
{
    struct FScoredAsset
    {
        int32 Score = 0;
        FAssetData Asset;
    };

    TArray<FScoredAsset> Scored;
    for (const FAssetData& Asset : Assets)
    {
        const int32 Score = ScoreVFXAsset(Asset, Keywords);
        if (Score == TNumericLimits<int32>::Lowest()) { continue; }
        FScoredAsset Entry;
        Entry.Score = Score;
        Entry.Asset = Asset;
        Scored.Add(MoveTemp(Entry));
    }

    Scored.Sort([](const FScoredAsset& A, const FScoredAsset& B)
    {
        if (A.Score != B.Score) { return A.Score > B.Score; }
        return A.Asset.PackageName.LexicalLess(B.Asset.PackageName);
    });

    OutSystems.Reset();
    TSet<FName> UsedPackages;
    for (const FScoredAsset& Entry : Scored)
    {
        if (OutSystems.Num() >= MaxSystems) { break; }
        if (UsedPackages.Contains(Entry.Asset.PackageName)) { continue; }

        UNiagaraSystem* System = Cast<UNiagaraSystem>(Entry.Asset.GetAsset());
        if (!System) { continue; }

        UsedPackages.Add(Entry.Asset.PackageName);
        OutSystems.Add(System);
        UE_LOG(LogTemp, Display, TEXT("[VFX-V3] cache -> %s (score=%d)"), *System->GetPathName(), Entry.Score);
    }
}

void ANWPremiumVFXDirector::ScanAndCacheVFX()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);

    SelectSystems(
        Assets,
        { TEXT("slash"), TEXT("sword"), TEXT("blade"), TEXT("melee"), TEXT("impact"), TEXT("hit"), TEXT("spark") },
        DirectDamageSystems,
        4);

    SelectSystems(
        Assets,
        { TEXT("shockwave"), TEXT("ground"), TEXT("earth"), TEXT("explosion"), TEXT("blast"), TEXT("burst"), TEXT("wave"), TEXT("impact") },
        AreaDamageSystems,
        4);

    SelectSystems(
        Assets,
        { TEXT("heal"), TEXT("holy"), TEXT("aura"), TEXT("life"), TEXT("restore"), TEXT("buff"), TEXT("energy") },
        HealSystems,
        4);

    UE_LOG(LogTemp, Warning, TEXT("[VFX-V3] catalogo Niagara=%d | seguros selecionados=%d"),
        Assets.Num(), DirectDamageSystems.Num() + AreaDamageSystems.Num() + HealSystems.Num());
}

void ANWPremiumVFXDirector::WarmupCachedSystems()
{
    if (bWarmupDone || !GetWorld()) { return; }
    bWarmupDone = true;

    TArray<UNiagaraSystem*> UniqueSystems;
    auto AppendUnique = [&UniqueSystems](const TArray<TObjectPtr<UNiagaraSystem>>& Systems)
    {
        for (UNiagaraSystem* System : Systems)
        {
            if (System && !UniqueSystems.Contains(System)) { UniqueSystems.Add(System); }
        }
    };
    AppendUnique(DirectDamageSystems);
    AppendUnique(AreaDamageSystems);
    AppendUnique(HealSystems);

    const FVector WarmupLocation(0.0f, 0.0f, -80000.0f);
    int32 Warmed = 0;
    for (UNiagaraSystem* System : UniqueSystems)
    {
        UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), System, WarmupLocation, FRotator::ZeroRotator, FVector(0.02f), true, true, ENCPoolMethod::AutoRelease, false);
        if (!Component) { continue; }

        ++Warmed;
        TWeakObjectPtr<UNiagaraComponent> WeakComponent(Component);
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakComponent]()
        {
            if (WeakComponent.IsValid()) { WeakComponent->Deactivate(); }
        }), 0.20f, false);
    }

    UE_LOG(LogTemp, Warning, TEXT("[VFX-V3] warm-up solicitado para %d sistemas Niagara antes do combate."), Warmed);
}

void ANWPremiumVFXDirector::DetectCasts()
{
    if (!GetWorld()) { return; }

    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character) || !Character->IsLocallyControlled()) { continue; }

        FPlayerVFXState& State = PlayerStates.FindOrAdd(Character);
        const ENWWeaponType Weapon = Character->GetActiveWeapon();

        if (!State.bInitialized || State.Weapon != Weapon || State.Cooldowns.Num() != 3)
        {
            State.Weapon = Weapon;
            State.Cooldowns.SetNumZeroed(3);
            for (int32 AbilityIndex = 0; AbilityIndex < 3; ++AbilityIndex)
            {
                State.Cooldowns[AbilityIndex] = Character->GetAbilityCooldownRemaining(AbilityIndex);
            }
            State.bInitialized = true;
            continue;
        }

        for (int32 AbilityIndex = 0; AbilityIndex < 3; ++AbilityIndex)
        {
            const float Current = Character->GetAbilityCooldownRemaining(AbilityIndex);
            const float Previous = State.Cooldowns[AbilityIndex];
            if (Current > Previous + 0.30f && Current > 0.40f)
            {
                PlayCastPresentation(Character, AbilityIndex);
            }
            State.Cooldowns[AbilityIndex] = Current;
        }
    }
}

void ANWPremiumVFXDirector::PlayCastPresentation(ANWCharacter* Character, int32 AbilityIndex)
{
    if (!Character || AbilityIndex < 0 || AbilityIndex > 2) { return; }

    const FNWWeaponDefinition Weapon = NWCombat::GetWeaponDefinition(Character->GetActiveWeapon());
    if (!Weapon.Abilities.IsValidIndex(AbilityIndex)) { return; }
    const FNWWeaponAbilityDefinition& Ability = Weapon.Abilities[AbilityIndex];

    PlayStableAbilityAnimation(Character, AbilityIndex);
    const FVector AimPoint = ResolveAimPoint(Character, Ability);
    SpawnLayeredVFX(AimPoint, Ability.Kind, Character->GetActiveWeapon(), AbilityIndex);

    UE_LOG(LogTemp, Warning, TEXT("[VFX-V3] %s/%s | slot=%d | ponto=(%.0f,%.0f,%.0f)"),
        *Weapon.Name, *Ability.Name, AbilityIndex + 1, AimPoint.X, AimPoint.Y, AimPoint.Z);
}

void ANWPremiumVFXDirector::PlayStableAbilityAnimation(ANWCharacter* Character, int32 AbilityIndex)
{
    if (!Character || !Character->GetMesh()) { return; }
    UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
    if (!AnimInstance) { return; }

    static const TCHAR* StableMontages[3] = {
        TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryB_Montage.Attack_PrimaryB_Montage"),
        TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryC_Montage.Attack_PrimaryC_Montage"),
        TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryA_Montage.Attack_PrimaryA_Montage")
    };
    static const float Rates[3] = { 0.92f, 0.78f, 0.70f };
    static TWeakObjectPtr<UAnimMontage> CachedMontages[3];

    const int32 Slot = FMath::Clamp(AbilityIndex, 0, 2);
    UAnimMontage* Montage = CachedMontages[Slot].Get();
    if (!Montage)
    {
        Montage = LoadObject<UAnimMontage>(nullptr, StableMontages[Slot]);
        CachedMontages[Slot] = Montage;
    }
    if (!Montage) { return; }

    // O Character pode ter iniciado uma sequence/montage antiga neste mesmo frame.
    // V3 assume ownership da apresentacao e faz blend curto para uma montage que ja
    // foi validada no ataque basico do Greystone.
    AnimInstance->Montage_Stop(0.055f);
    AnimInstance->Montage_Play(Montage, Rates[Slot]);
}

FVector ANWPremiumVFXDirector::ResolveAimPoint(ANWCharacter* Character, const FNWWeaponAbilityDefinition& Ability) const
{
    if (!Character || !GetWorld()) { return FVector::ZeroVector; }
    if (Ability.Kind == ENWAbilityKind::Heal)
    {
        return Character->GetActorLocation() + FVector(0.0f, 0.0f, 35.0f);
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const float TraceRange = FMath::Max(Ability.Range, 400.0f) + 2200.0f;
    const FVector End = ViewLocation + ViewRotation.Vector() * TraceRange;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(NWV3VFXAim), true, Character);
    Params.AddIgnoredActor(Character);

    FHitResult Hit;
    FVector Point = End;
    if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params) && Hit.bBlockingHit)
    {
        Point = Hit.ImpactPoint;
    }

    const bool bRanged = Character->GetActiveWeapon() == ENWWeaponType::Staff ||
        Character->GetActiveWeapon() == ENWWeaponType::Bow ||
        Character->GetActiveWeapon() == ENWWeaponType::Firearm;
    const float MaxRange = bRanged ? FMath::Max(Ability.Range, 650.0f) : FMath::Max(Ability.Range, 320.0f);

    FVector Delta = Point - Character->GetActorLocation();
    if (!bRanged) { Delta.Z = 0.0f; }
    if (Delta.Size() > MaxRange)
    {
        Delta = Delta.GetSafeNormal() * MaxRange;
    }
    Point = Character->GetActorLocation() + Delta;
    if (!bRanged) { Point.Z = Character->GetActorLocation().Z - 72.0f; }
    return Point;
}

void ANWPremiumVFXDirector::SpawnLayeredVFX(const FVector& Location, ENWAbilityKind Kind, ENWWeaponType Weapon, int32 AbilityIndex)
{
    if (!GetWorld()) { return; }

    const TArray<TObjectPtr<UNiagaraSystem>>* Systems = &DirectDamageSystems;
    if (Kind == ENWAbilityKind::AreaDamage) { Systems = &AreaDamageSystems; }
    else if (Kind == ENWAbilityKind::Heal) { Systems = &HealSystems; }

    if (!Systems || Systems->IsEmpty())
    {
        UE_LOG(LogTemp, Display, TEXT("[VFX-V3] sem Niagara curado para kind=%d; telegraph deterministico permanece ativo."), static_cast<int32>(Kind));
        return;
    }

    const int32 LayerCount = FMath::Min(LayersPerCast, Systems->Num());
    for (int32 Index = 0; Index < LayerCount; ++Index)
    {
        UNiagaraSystem* System = (*Systems)[Index];
        if (!System) { continue; }

        float ScaleValue = 1.0f + 0.16f * Index;
        if (Kind == ENWAbilityKind::AreaDamage) { ScaleValue += 0.22f; }
        if (Kind == ENWAbilityKind::Heal) { ScaleValue += 0.10f; }
        if (Weapon == ENWWeaponType::Greatsword && AbilityIndex == 1) { ScaleValue += 0.16f; }

        const FVector LayerLocation = Location + FVector(0.0f, 0.0f, 8.0f + Index * 5.0f);
        const FRotator Rotation(0.0f, static_cast<float>(Index * 67), 0.0f);
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), System, LayerLocation, Rotation, FVector(ScaleValue), true, true, ENCPoolMethod::AutoRelease, false);
    }
}
