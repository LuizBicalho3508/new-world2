#include "NWPremiumV6CombatDirector.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NWCharacter.h"
#include "NWEnemy.h"
#include "NWPremiumV6AbilityLibrary.h"
#include "TimerManager.h"

ANWPremiumV6CombatDirector::ANWPremiumV6CombatDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.06f;
    bReplicates = false;
}

void ANWPremiumV6CombatDirector::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("[ABILITY-V6] 21 identidades de habilidade ativas; free aim preservado."));
}

void ANWPremiumV6CombatDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()) { return; }
    DetectCasts();
}

void ANWPremiumV6CombatDirector::DetectCasts()
{
    for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
    {
        ANWCharacter* Character = *It;
        if (!IsValid(Character) || !Character->HasAuthority()) { continue; }

        FCastState& State = States.FindOrAdd(Character);
        const ENWWeaponType Weapon = Character->GetActiveWeapon();
        if (!State.bInitialized || State.Weapon != Weapon || State.Cooldowns.Num() != 3)
        {
            State.Weapon = Weapon;
            State.Cooldowns.SetNumZeroed(3);
            for (int32 Slot = 0; Slot < 3; ++Slot) { State.Cooldowns[Slot] = Character->GetAbilityCooldownRemaining(Slot); }
            State.bInitialized = true;
            continue;
        }

        for (int32 Slot = 0; Slot < 3; ++Slot)
        {
            const float Current = Character->GetAbilityCooldownRemaining(Slot);
            const float Previous = State.Cooldowns[Slot];
            if (Current > Previous + 0.28f && Current > 0.40f)
            {
                ApplyWeaponSignature(Character, Weapon, Slot);
            }
            State.Cooldowns[Slot] = Current;
        }
    }
}

FVector ANWPremiumV6CombatDirector::ResolveAimPoint(ANWCharacter* Character, float MaxDistance) const
{
    if (!Character || !GetWorld()) { return FVector::ZeroVector; }
    FVector ViewLocation;
    FRotator ViewRotation;
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController())) { PC->GetPlayerViewPoint(ViewLocation, ViewRotation); }
    else { Character->GetActorEyesViewPoint(ViewLocation, ViewRotation); }

    const FVector End = ViewLocation + ViewRotation.Vector() * MaxDistance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(NWV6Aim), true, Character);
    Params.AddIgnoredActor(Character);
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params) && Hit.bBlockingHit) { return Hit.ImpactPoint; }
    return End;
}

ANWEnemy* ANWPremiumV6CombatDirector::FindBestEnemyNear(const FVector& Point, float Radius) const
{
    ANWEnemy* Best = nullptr;
    float BestDistanceSq = FMath::Square(Radius);
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy) || Enemy->GetHealth() <= 0.0f) { continue; }
        const float D = FVector::DistSquared(Enemy->GetActorLocation(), Point);
        if (D < BestDistanceSq) { BestDistanceSq = D; Best = Enemy; }
    }
    return Best;
}

TArray<ANWEnemy*> ANWPremiumV6CombatDirector::FindEnemiesNear(const FVector& Point, float Radius, int32 MaxCount) const
{
    TArray<TPair<float, ANWEnemy*>> Sorted;
    const float RadiusSq = FMath::Square(Radius);
    for (TActorIterator<ANWEnemy> It(GetWorld()); It; ++It)
    {
        ANWEnemy* Enemy = *It;
        if (!IsValid(Enemy) || Enemy->GetHealth() <= 0.0f) { continue; }
        const float D = FVector::DistSquared(Enemy->GetActorLocation(), Point);
        if (D <= RadiusSq) { Sorted.Emplace(D, Enemy); }
    }
    Sorted.Sort([](const TPair<float, ANWEnemy*>& A, const TPair<float, ANWEnemy*>& B) { return A.Key < B.Key; });
    TArray<ANWEnemy*> Result;
    for (int32 Index = 0; Index < Sorted.Num() && Index < MaxCount; ++Index) { Result.Add(Sorted[Index].Value); }
    return Result;
}

void ANWPremiumV6CombatDirector::ApplyDamage(ANWCharacter* Character, ANWEnemy* Enemy, float Damage) const
{
    if (!Character || !Enemy || Damage <= 0.0f) { return; }
    UGameplayStatics::ApplyDamage(Enemy, Damage, Character->GetController(), Character, UDamageType::StaticClass());
}

void ANWPremiumV6CombatDirector::ApplyDot(ANWCharacter* Character, ANWEnemy* Enemy, float DamagePerTick, int32 Ticks, float Interval)
{
    if (!Character || !Enemy || !GetWorld() || Ticks <= 0) { return; }
    TWeakObjectPtr<ANWCharacter> WeakCharacter(Character);
    TWeakObjectPtr<ANWEnemy> WeakEnemy(Enemy);
    TSharedRef<int32> Remaining = MakeShared<int32>(Ticks);
    TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
    GetWorldTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WeakCharacter, WeakEnemy, Remaining, Handle, DamagePerTick]()
    {
        if (!WeakCharacter.IsValid() || !WeakEnemy.IsValid() || WeakEnemy->GetHealth() <= 0.0f || *Remaining <= 0)
        {
            if (WeakCharacter.IsValid()) { WeakCharacter->GetWorldTimerManager().ClearTimer(*Handle); }
            return;
        }
        UGameplayStatics::ApplyDamage(WeakEnemy.Get(), DamagePerTick, WeakCharacter->GetController(), WeakCharacter.Get(), UDamageType::StaticClass());
        --(*Remaining);
        if (*Remaining <= 0) { WeakCharacter->GetWorldTimerManager().ClearTimer(*Handle); }
    }), Interval, true, Interval);
}

void ANWPremiumV6CombatDirector::ApplySlow(ANWEnemy* Enemy, float SpeedMultiplier, float Duration)
{
    if (!Enemy || !Enemy->GetCharacterMovement() || !GetWorld()) { return; }
    UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
    const float OriginalSpeed = Movement->MaxWalkSpeed;
    Movement->MaxWalkSpeed = OriginalSpeed * FMath::Clamp(SpeedMultiplier, 0.25f, 0.90f);
    TWeakObjectPtr<ANWEnemy> WeakEnemy(Enemy);
    FTimerHandle Handle;
    GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakEnemy, OriginalSpeed]()
    {
        if (WeakEnemy.IsValid() && WeakEnemy->GetCharacterMovement()) { WeakEnemy->GetCharacterMovement()->MaxWalkSpeed = OriginalSpeed; }
    }), Duration, false);
}

void ANWPremiumV6CombatDirector::PulseDamage(ANWCharacter* Character, const FVector& Point, float Radius, float Damage, int32 Pulses, float Interval)
{
    if (!Character || !GetWorld() || Pulses <= 0) { return; }
    TWeakObjectPtr<ANWCharacter> WeakCharacter(Character);
    TSharedRef<int32> Remaining = MakeShared<int32>(Pulses);
    TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
    auto Pulse = [this, WeakCharacter, Point, Radius, Damage, Remaining, Handle]()
    {
        if (!WeakCharacter.IsValid() || *Remaining <= 0)
        {
            if (WeakCharacter.IsValid()) { WeakCharacter->GetWorldTimerManager().ClearTimer(*Handle); }
            return;
        }
        for (ANWEnemy* Enemy : FindEnemiesNear(Point, Radius, 8)) { ApplyDamage(WeakCharacter.Get(), Enemy, Damage); }
        --(*Remaining);
        if (*Remaining <= 0) { WeakCharacter->GetWorldTimerManager().ClearTimer(*Handle); }
    };
    Pulse();
    if (*Remaining > 0) { GetWorldTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda(Pulse), Interval, true, Interval); }
}

void ANWPremiumV6CombatDirector::ApplyWeaponSignature(ANWCharacter* Character, ENWWeaponType Weapon, int32 Slot)
{
    if (!Character || !GetWorld()) { return; }
    const FVector Aim = ResolveAimPoint(Character, 2200.0f);
    ANWEnemy* Target = FindBestEnemyNear(Aim, Weapon == ENWWeaponType::Staff || Weapon == ENWWeaponType::Bow || Weapon == ENWWeaponType::Firearm ? 420.0f : 500.0f);

    switch (Weapon)
    {
        case ENWWeaponType::Staff:
            if (Slot == 0 && Target) { ApplyDamage(Character, Target, 8.0f); ApplyDot(Character, Target, 4.0f, 4, 0.55f); }
            else if (Slot == 1) { for (ANWEnemy* E : FindEnemiesNear(Aim, 420.0f, 7)) { ApplyDamage(Character, E, 6.0f); ApplySlow(E, 0.48f, 2.2f); } }
            else { for (ANWEnemy* E : FindEnemiesNear(Aim, 650.0f, 5)) { ApplyDamage(Character, E, 9.0f); E->ApplyStagger(0.22f); } }
            break;

        case ENWWeaponType::Greatsword:
            if (Slot == 0 && Target) { ApplyDamage(Character, Target, 10.0f); Target->ApplyStagger(0.72f); }
            else if (Slot == 1) { for (ANWEnemy* E : FindEnemiesNear(Aim, 480.0f, 8)) { ApplyDamage(Character, E, 8.0f); E->ApplyStagger(0.48f); } }
            else { PulseDamage(Character, Character->GetActorLocation(), 360.0f, 5.5f, 3, 0.18f); }
            break;

        case ENWWeaponType::DualSwords:
            if (Slot == 0 && Target) { ApplyDamage(Character, Target, 7.5f); Character->LaunchCharacter(Character->GetActorForwardVector() * 260.0f, true, false); }
            else if (Slot == 1) { PulseDamage(Character, Character->GetActorLocation(), 330.0f, 4.5f, 3, 0.14f); }
            else { PulseDamage(Character, Character->GetActorLocation(), 280.0f, 4.0f, 3, 0.20f); }
            break;

        case ENWWeaponType::SwordShield:
            if (Slot == 0 && Target) { ApplyDamage(Character, Target, 6.0f); Target->ApplyStagger(1.0f); }
            else if (Slot == 1) { for (ANWEnemy* E : FindEnemiesNear(Character->GetActorLocation(), 360.0f, 8)) { ApplyDamage(Character, E, 5.0f); E->ApplyStagger(0.45f); } }
            else { for (ANWEnemy* E : FindEnemiesNear(Character->GetActorLocation(), 250.0f, 4)) { E->ApplyStagger(0.30f); } }
            break;

        case ENWWeaponType::Daggers:
            if (Slot == 0 && Target) { Character->LaunchCharacter((Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D() * 310.0f, true, false); ApplyDamage(Character, Target, 7.0f); ApplyDot(Character, Target, 3.5f, 5, 0.46f); }
            else if (Slot == 1) { PulseDamage(Character, Character->GetActorLocation() + Character->GetActorForwardVector() * 145.0f, 330.0f, 4.0f, 3, 0.12f); }
            else { for (ANWEnemy* E : FindEnemiesNear(Character->GetActorLocation(), 300.0f, 6)) { ApplyDot(Character, E, 3.0f, 4, 0.50f); } }
            break;

        case ENWWeaponType::Bow:
            if (Slot == 0 && Target) { ApplyDamage(Character, Target, 10.0f); }
            else if (Slot == 1) { PulseDamage(Character, Aim, 470.0f, 4.5f, 4, 0.24f); }
            else
            {
                for (ANWEnemy* E : FindEnemiesNear(Aim, 500.0f, 6))
                {
                    ApplyDamage(Character, E, 5.0f);
                    if (Character->GetArrowElement() == ENWArrowElement::Fire) ApplyDot(Character, E, 3.5f, 4, 0.50f);
                    else if (Character->GetArrowElement() == ENWArrowElement::Frost) ApplySlow(E, 0.55f, 1.8f);
                    else if (Character->GetArrowElement() == ENWArrowElement::Lightning) E->ApplyStagger(0.30f);
                    else if (Character->GetArrowElement() == ENWArrowElement::Poison) ApplyDot(Character, E, 2.8f, 5, 0.48f);
                }
            }
            break;

        case ENWWeaponType::Firearm:
            if (Slot == 0 && Target) { ApplyDamage(Character, Target, 11.0f); Target->ApplyStagger(0.42f); }
            else if (Slot == 1) { for (ANWEnemy* E : FindEnemiesNear(Aim, 350.0f, 6)) { ApplyDamage(Character, E, 6.0f); } }
            else { PulseDamage(Character, Aim, 430.0f, 10.0f, 2, 0.18f); }
            break;

        default:
            break;
    }

    UE_LOG(LogTemp, Warning, TEXT("[ABILITY-V6] %s | slot=%d | %s | alvo=%s"),
        *NWPremiumV6::AbilityName(Weapon, Slot), Slot + 1,
        *NWPremiumV6::AbilityDescription(Weapon, Slot), Target ? *Target->GetDisplayName() : TEXT("free-cast"));
}
