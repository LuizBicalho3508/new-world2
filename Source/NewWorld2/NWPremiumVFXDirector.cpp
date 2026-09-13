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
    UE_LOG(LogTemp, Warning, TEXT("[VFX-V10] modo estavel: whitelist Free_Magic/ArrowTrail | sem NiagaraExamples | sem spawn de warm-up."));
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

    // The V9 log showed NiagaraExamples firing editor metadata warnings and
    // compiling unrelated fireworks/rocket effects for more than a minute.
    // V10 deliberately keeps the current game on the two small packs already
    // proven by playtests: Free_Magic and ArrowTrail.
    if (!Path.Contains(TEXT("/free_magic/")) && !Path.Contains(TEXT("/arrowtrail/"))) { return false; }

    static const TCHAR* Blocked[] = {
        TEXT("/demo/"), TEXT("/tutorial"), TEXT("preview"), TEXT("benchmark"), TEXT("testmap"),
        TEXT("debug"), TEXT("_splinevfx"), TEXT("distancefield"), TEXT("distance_field"), TEXT("grid3d"),
        TEXT("fluid"), TEXT("skeletalmeshbones"), TEXT("skeletalmeshtris"), TEXT("bubble_burst"),
        TEXT("_base"), TEXT("looping")
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
    if (Search.Contains(TEXT("free_magic"))) Score += 80;
    if (Search.Contains(TEXT("arrowtrail"))) Score += 75;
    if (Search.Contains(TEXT("hit")) || Search.Contains(TEXT("slash")) || Search.Contains(TEXT("trail"))) Score += 24;
    if (Search.Contains(TEXT("projectile"))) Score -= 20;
    if (Search.Contains(TEXT("aura"))) Score -= 12;
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
            UE_LOG(LogTemp, Display, TEXT("[VFX-V10] cache -> %s"), *System->GetPathName());
        }
    }
}

void ANWPremiumVFXDirector::ScanAndCacheVFX()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game/Free_Magic")));
    Filter.PackagePaths.Add(FName(TEXT("/Game/ArrowTrail")));
    Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);

    // Four systems maximum for the entire vertical slice. Themes without a
    // dedicated effect intentionally reuse GenericSystems instead of compiling
    // another vendor graph during gameplay.
    SelectSystems(Assets, { TEXT("fire") }, FireSystems, 1);
    SelectSystems(Assets, { TEXT("ice"), TEXT("frost") }, FrostSystems, 1);
    LightningSystems.Reset();
    EarthSystems.Reset();
    BladeSystems.Reset();
    ShadowSystems.Reset();
    BloodSystems.Reset();
    HolySystems.Reset();
    SelectSystems(Assets, { TEXT("arrowtrail_magic"), TEXT("trail_magic"), TEXT("magic") }, ProjectileSystems, 1);
    GunpowderSystems.Reset();
    SelectSystems(Assets, { TEXT("hit1"), TEXT("hit2"), TEXT("slash") }, GenericSystems, 1);

    UE_LOG(LogTemp, Warning, TEXT("[VFX-V10] assets permitidos=%d | fire=%d frost=%d projectile=%d generic=%d | demais temas reutilizam generic"),
        Assets.Num(), FireSystems.Num(), FrostSystems.Num(), ProjectileSystems.Num(), GenericSystems.Num());
}

const TArray<TObjectPtr<UNiagaraSystem>>& ANWPremiumVFXDirector::GetSystemsForTheme(NWPremiumV6::ESkillTheme Theme) const
{
    const TArray<TObjectPtr<UNiagaraSystem>>* Result = &GenericSystems;
    switch (Theme)
    {
        case NWPremiumV6::ESkillTheme::Fire: if (!FireSystems.IsEmpty()) Result = &FireSystems; break;
        case NWPremiumV6::ESkillTheme::Frost: if (!FrostSystems.IsEmpty()) Result = &FrostSystems; break;
        case NWPremiumV6::ESkillTheme::Lightning: if (!LightningSystems.IsEmpty()) Result = &LightningSystems; break;
        case NWPremiumV6::ESkillTheme::Earth: if (!EarthSystems.IsEmpty()) Result = &EarthSystems; break;
        case NWPremiumV6::ESkillTheme::Blade: if (!BladeSystems.IsEmpty()) Result = &BladeSystems; break;
        case NWPremiumV6::ESkillTheme::Shadow: if (!ShadowSystems.IsEmpty()) Result = &ShadowSystems; break;
        case NWPremiumV6::ESkillTheme::Blood: if (!BloodSystems.IsEmpty()) Result = &BloodSystems; break;
        case NWPremiumV6::ESkillTheme::Holy: if (!HolySystems.IsEmpty()) Result = &HolySystems; break;
        case NWPremiumV6::ESkillTheme::Projectile: if (!ProjectileSystems.IsEmpty()) Result = &ProjectileSystems; break;
        case NWPremiumV6::ESkillTheme::Gunpowder: if (!GunpowderSystems.IsEmpty()) Result = &GunpowderSystems; break;
        default: break;
    }
    return *Result;
}

void ANWPremiumVFXDirector::WarmupCachedSystems()
{
    if (bWarmupDone) { return; }
    bWarmupDone = true;
    // V9 spawned every cached effect under the map, which forced shader/texture
    // work while the map was still loading. V10 leaves compilation lazy for the
    // tiny four-system whitelist and lets the PSO cache learn normal gameplay.
    UE_LOG(LogTemp, Display, TEXT("[VFX-V10] warm-up por spawn desativado para evitar compilacao forçada no boot."));
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

    // One system with two light layers is enough for readability in the stable
    // baseline and avoids multiplying PSO creation on first cast.
    const int32 SafeLayers = FMath::Clamp(LayersPerCast, 1, 2);
    for (int32 Layer = 0; Layer < SafeLayers; ++Layer)
    {
        UNiagaraSystem* System = Systems[Layer % Systems.Num()];
        if (!System) continue;
        float Scale = 0.96f + 0.20f * Layer;
        if (AbilityIndex == 1) Scale += 0.18f;
        const FVector P = Location + FVector(0, 0, 7.0f + Layer * 8.0f);
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
