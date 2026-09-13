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
    UE_LOG(LogTemp, Warning, TEXT("[VFX-V6] diretor tematico ativo | 7 armas x 3 skills | %d camadas por cast | Spline/DistanceField bloqueados."), LayersPerCast);
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
    static const TCHAR* Blocked[] = {
        TEXT("/demo/"), TEXT("/tutorial"), TEXT("animstarterpack"), TEXT("deformablesnowsystem/demo"),
        TEXT("free_magic/demo"), TEXT("smokepufflight"), TEXT("preview"), TEXT("benchmark"), TEXT("testmap"),
        TEXT("debug"), TEXT("_splinevfx"), TEXT("distancefield"), TEXT("distance_field"), TEXT("grid3d"),
        TEXT("fluid"), TEXT("skeletalmeshbones"), TEXT("skeletalmeshtris"), TEXT("bubble_burst")
    };
    for (const TCHAR* Token : Blocked) if (Path.Contains(Token)) { return false; }
    return true;
}

int32 ANWPremiumVFXDirector::ScoreVFXAsset(const FAssetData& Asset, const TArray<FString>& Keywords) const
{
    if (!IsSafeVFXAsset(Asset)) { return TNumericLimits<int32>::Lowest(); }
    const FString Search = (Asset.PackageName.ToString() + TEXT("/") + Asset.AssetName.ToString()).ToLower();
    bool bMatch = false;
    int32 Score = 0;
    for (const FString& Keyword : Keywords)
    {
        if (Search.Contains(Keyword.ToLower())) { bMatch = true; Score += 58; }
    }
    if (!bMatch) { return TNumericLimits<int32>::Lowest(); }
    if (Search.Contains(TEXT("/fab/"))) Score += 70;
    if (Search.Contains(TEXT("vfx"))) Score += 34;
    if (Search.Contains(TEXT("magic"))) Score += 32;
    if (Search.Contains(TEXT("niagara"))) Score += 24;
    if (Search.Contains(TEXT("impact")) || Search.Contains(TEXT("burst")) || Search.Contains(TEXT("hit"))) Score += 18;
    if (Search.Contains(TEXT("loop")) || Search.Contains(TEXT("ambient"))) Score -= 18;
    if (Search.Contains(TEXT("wood")) || Search.Contains(TEXT("debuff"))) Score -= 90;
    return Score;
}

void ANWPremiumVFXDirector::SelectSystems(const TArray<FAssetData>& Assets, const TArray<FString>& Keywords, TArray<TObjectPtr<UNiagaraSystem>>& OutSystems, int32 MaxSystems)
{
    struct FScored { int32 Score = 0; FAssetData Asset; };
    TArray<FScored> Scored;
    for (const FAssetData& Asset : Assets)
    {
        const int32 Score = ScoreVFXAsset(Asset, Keywords);
        if (Score == TNumericLimits<int32>::Lowest()) { continue; }
        Scored.Add({ Score, Asset });
    }
    Scored.Sort([](const FScored& A, const FScored& B)
    {
        if (A.Score != B.Score) return A.Score > B.Score;
        return A.Asset.PackageName.LexicalLess(B.Asset.PackageName);
    });

    OutSystems.Reset();
    for (const FScored& Entry : Scored)
    {
        if (OutSystems.Num() >= MaxSystems) break;
        if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Entry.Asset.GetAsset()))
        {
            OutSystems.Add(System);
            UE_LOG(LogTemp, Display, TEXT("[VFX-V6] cache -> %s (score=%d)"), *System->GetPathName(), Entry.Score);
        }
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

    // Apenas um sistema por tema precisa ser compilado; a riqueza vem de camadas,
    // escala, rotacao e tint diferentes. Isso reduz hitches sem empobrecer o cast.
    SelectSystems(Assets, { TEXT("fire"), TEXT("flame"), TEXT("ember"), TEXT("burn"), TEXT("inferno") }, FireSystems, 1);
    SelectSystems(Assets, { TEXT("frost"), TEXT("ice"), TEXT("snow"), TEXT("freeze"), TEXT("cold") }, FrostSystems, 1);
    SelectSystems(Assets, { TEXT("lightning"), TEXT("electric"), TEXT("shock"), TEXT("thunder"), TEXT("storm") }, LightningSystems, 1);
    SelectSystems(Assets, { TEXT("earth"), TEXT("ground"), TEXT("rock"), TEXT("stone"), TEXT("shockwave") }, EarthSystems, 1);
    SelectSystems(Assets, { TEXT("slash"), TEXT("sword"), TEXT("blade"), TEXT("melee"), TEXT("hit") }, BladeSystems, 1);
    SelectSystems(Assets, { TEXT("shadow"), TEXT("dark"), TEXT("poison"), TEXT("venom"), TEXT("toxic") }, ShadowSystems, 1);
    SelectSystems(Assets, { TEXT("blood"), TEXT("vamp"), TEXT("siphon"), TEXT("life") }, BloodSystems, 1);
    SelectSystems(Assets, { TEXT("holy"), TEXT("heal"), TEXT("aura"), TEXT("restore"), TEXT("buff") }, HolySystems, 1);
    SelectSystems(Assets, { TEXT("arrow"), TEXT("projectile"), TEXT("trail"), TEXT("bolt") }, ProjectileSystems, 1);
    SelectSystems(Assets, { TEXT("muzzle"), TEXT("gun"), TEXT("bullet"), TEXT("explosion"), TEXT("blast") }, GunpowderSystems, 1);
    SelectSystems(Assets, { TEXT("hit"), TEXT("slash"), TEXT("burst"), TEXT("impact"), TEXT("magic") }, GenericSystems, 3);

    UE_LOG(LogTemp, Warning, TEXT("[VFX-V6] catalogo=%d | fire=%d frost=%d lightning=%d earth=%d blade=%d shadow=%d blood=%d holy=%d projectile=%d gun=%d generic=%d"),
        Assets.Num(), FireSystems.Num(), FrostSystems.Num(), LightningSystems.Num(), EarthSystems.Num(), BladeSystems.Num(), ShadowSystems.Num(),
        BloodSystems.Num(), HolySystems.Num(), ProjectileSystems.Num(), GunpowderSystems.Num(), GenericSystems.Num());
}

const TArray<TObjectPtr<UNiagaraSystem>>& ANWPremiumVFXDirector::GetSystemsForTheme(NWPremiumV6::ESkillTheme Theme) const
{
    const TArray<TObjectPtr<UNiagaraSystem>>* Result = &GenericSystems;
    switch (Theme)
    {
        case NWPremiumV6::ESkillTheme::Fire: Result = &FireSystems; break;
        case NWPremiumV6::ESkillTheme::Frost: Result = &FrostSystems; break;
        case NWPremiumV6::ESkillTheme::Lightning: Result = &LightningSystems; break;
        case NWPremiumV6::ESkillTheme::Earth: Result = &EarthSystems; break;
        case NWPremiumV6::ESkillTheme::Blade: Result = &BladeSystems; break;
        case NWPremiumV6::ESkillTheme::Shadow: Result = &ShadowSystems; break;
        case NWPremiumV6::ESkillTheme::Blood: Result = &BloodSystems; break;
        case NWPremiumV6::ESkillTheme::Holy: Result = &HolySystems; break;
        case NWPremiumV6::ESkillTheme::Projectile: Result = &ProjectileSystems; break;
        case NWPremiumV6::ESkillTheme::Gunpowder: Result = &GunpowderSystems; break;
        default: break;
    }
    return Result->IsEmpty() ? GenericSystems : *Result;
}

void ANWPremiumVFXDirector::WarmupCachedSystems()
{
    if (bWarmupDone || !GetWorld()) { return; }
    bWarmupDone = true;
    TArray<UNiagaraSystem*> Unique;
    auto Append = [&Unique](const TArray<TObjectPtr<UNiagaraSystem>>& Systems)
    {
        for (UNiagaraSystem* S : Systems) if (S && !Unique.Contains(S)) Unique.Add(S);
    };
    Append(FireSystems); Append(FrostSystems); Append(LightningSystems); Append(EarthSystems); Append(BladeSystems);
    Append(ShadowSystems); Append(BloodSystems); Append(HolySystems); Append(ProjectileSystems); Append(GunpowderSystems); Append(GenericSystems);

    const FVector WarmupLocation(0.0f, 0.0f, -80000.0f);
    int32 Warmed = 0;
    for (UNiagaraSystem* System : Unique)
    {
        UNiagaraComponent* C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, WarmupLocation, FRotator::ZeroRotator,
            FVector(0.01f), true, true, ENCPoolMethod::AutoRelease, false);
        if (!C) continue;
        ++Warmed;
        TWeakObjectPtr<UNiagaraComponent> Weak(C);
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) Weak->Deactivate(); }), 0.22f, false);
    }
    UE_LOG(LogTemp, Warning, TEXT("[VFX-V6] warm-up solicitado para %d Niagara seguros; compilacao pesada concentrada no boot."), Warmed);
}

void ANWPremiumVFXDirector::DetectCasts()
{
    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character) || !Character->IsLocallyControlled()) continue;
        FPlayerVFXState& State = PlayerStates.FindOrAdd(Character);
        const ENWWeaponType Weapon = Character->GetActiveWeapon();
        if (!State.bInitialized || State.Weapon != Weapon || State.Cooldowns.Num() != 3)
        {
            State.Weapon = Weapon;
            State.Cooldowns.SetNumZeroed(3);
            for (int32 I = 0; I < 3; ++I) State.Cooldowns[I] = Character->GetAbilityCooldownRemaining(I);
            State.bInitialized = true;
            continue;
        }
        for (int32 I = 0; I < 3; ++I)
        {
            const float Current = Character->GetAbilityCooldownRemaining(I);
            if (Current > State.Cooldowns[I] + 0.28f && Current > 0.40f) PlayCastPresentation(Character, I);
            State.Cooldowns[I] = Current;
        }
    }
}

void ANWPremiumVFXDirector::PlayCastPresentation(ANWCharacter* Character, int32 AbilityIndex)
{
    if (!Character || AbilityIndex < 0 || AbilityIndex > 2) return;
    const FNWWeaponDefinition WeaponDef = NWCombat::GetWeaponDefinition(Character->GetActiveWeapon());
    if (!WeaponDef.Abilities.IsValidIndex(AbilityIndex)) return;
    PlayStableAbilityAnimation(Character, AbilityIndex);
    const FVector Aim = ResolveAimPoint(Character, WeaponDef.Abilities[AbilityIndex]);
    SpawnLayeredVFX(Aim, Character->GetActiveWeapon(), AbilityIndex, Character->GetArrowElement());
    UE_LOG(LogTemp, Warning, TEXT("[VFX-V6] %s | slot=%d | tema=%d | ponto=(%.0f,%.0f,%.0f)"),
        *NWPremiumV6::AbilityName(Character->GetActiveWeapon(), AbilityIndex), AbilityIndex + 1,
        static_cast<int32>(NWPremiumV6::Theme(Character->GetActiveWeapon(), AbilityIndex, Character->GetArrowElement())), Aim.X, Aim.Y, Aim.Z);
}

void ANWPremiumVFXDirector::PlayStableAbilityAnimation(ANWCharacter* Character, int32 AbilityIndex)
{
    if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetAnimInstance()) return;
    static const TCHAR* Montages[3] = {
        TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryA_Montage.Attack_PrimaryA_Montage"),
        TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryB_Montage.Attack_PrimaryB_Montage"),
        TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryC_Montage.Attack_PrimaryC_Montage")
    };
    static TWeakObjectPtr<UAnimMontage> Cache[3];
    const int32 WeaponOffset = static_cast<int32>(Character->GetActiveWeapon()) % 3;
    const int32 MontageIndex = (AbilityIndex + WeaponOffset) % 3;
    UAnimMontage* Montage = Cache[MontageIndex].Get();
    if (!Montage) { Montage = LoadObject<UAnimMontage>(nullptr, Montages[MontageIndex]); Cache[MontageIndex] = Montage; }
    if (!Montage) return;
    float Rate = 1.0f;
    if (Character->GetActiveWeapon() == ENWWeaponType::Greatsword) Rate = AbilityIndex == 1 ? 0.72f : 0.84f;
    else if (Character->GetActiveWeapon() == ENWWeaponType::Daggers || Character->GetActiveWeapon() == ENWWeaponType::DualSwords) Rate = 1.28f;
    else if (Character->GetActiveWeapon() == ENWWeaponType::Staff) Rate = 0.92f + 0.08f * AbilityIndex;
    UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
    Anim->Montage_Stop(0.045f);
    Anim->Montage_Play(Montage, Rate);
}

FVector ANWPremiumVFXDirector::ResolveAimPoint(ANWCharacter* Character, const FNWWeaponAbilityDefinition& Ability) const
{
    if (!Character || !GetWorld()) return FVector::ZeroVector;
    if (Ability.Kind == ENWAbilityKind::Heal) return Character->GetActorLocation() + FVector(0, 0, 35);
    FVector ViewLocation; FRotator ViewRotation;
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController())) PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    else Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    const FVector End = ViewLocation + ViewRotation.Vector() * (FMath::Max(Ability.Range, 500.0f) + 1900.0f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(NWV6VFXAim), true, Character);
    Params.AddIgnoredActor(Character);
    FHitResult Hit;
    FVector Point = (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params) && Hit.bBlockingHit) ? Hit.ImpactPoint : End;
    const bool bRanged = Character->GetActiveWeapon() == ENWWeaponType::Staff || Character->GetActiveWeapon() == ENWWeaponType::Bow || Character->GetActiveWeapon() == ENWWeaponType::Firearm;
    const float MaxRange = bRanged ? FMath::Max(Ability.Range, 700.0f) : FMath::Max(Ability.Range, 360.0f);
    FVector Delta = Point - Character->GetActorLocation();
    if (!bRanged) Delta.Z = 0.0f;
    if (Delta.Size() > MaxRange) Delta = Delta.GetSafeNormal() * MaxRange;
    Point = Character->GetActorLocation() + Delta;
    if (!bRanged) Point.Z = Character->GetActorLocation().Z - 72.0f;
    return Point;
}

void ANWPremiumVFXDirector::SpawnLayeredVFX(const FVector& Location, ENWWeaponType Weapon, int32 AbilityIndex, ENWArrowElement ArrowElement)
{
    if (!GetWorld()) return;
    const NWPremiumV6::ESkillTheme Theme = NWPremiumV6::Theme(Weapon, AbilityIndex, ArrowElement);
    const FLinearColor Tint = NWPremiumV6::ThemeColor(Theme);
    const TArray<TObjectPtr<UNiagaraSystem>>& Systems = GetSystemsForTheme(Theme);
    if (Systems.IsEmpty()) return;

    for (int32 Layer = 0; Layer < LayersPerCast; ++Layer)
    {
        UNiagaraSystem* System = Systems[Layer % Systems.Num()];
        if (!System) continue;
        float Scale = 0.92f + 0.22f * Layer;
        if (AbilityIndex == 1) Scale += 0.28f;
        if (Weapon == ENWWeaponType::Greatsword || Weapon == ENWWeaponType::Firearm) Scale += 0.16f;
        const FVector P = Location + FVector(0, 0, 7.0f + Layer * 7.0f);
        UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, P,
            FRotator(0, Layer * 61.0f, 0), FVector(Scale), true, true, ENCPoolMethod::AutoRelease, false);
        if (Component)
        {
            Component->SetVariableLinearColor(TEXT("User.Color"), Tint);
            Component->SetVariableLinearColor(TEXT("User.Tint"), Tint);
            Component->SetVariableLinearColor(TEXT("User.BaseColor"), Tint);
        }
    }
}
