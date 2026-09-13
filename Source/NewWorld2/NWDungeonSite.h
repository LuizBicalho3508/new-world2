#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWWorldTypes.h"
#include "NWDungeonSite.generated.h"

class ANWDungeonGuardian;
class ANWEnemy;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UTextRenderComponent;

UCLASS()
class NEWORLD2_API ANWDungeonSite : public AActor
{
    GENERATED_BODY()

public:
    ANWDungeonSite();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void ConfigureDungeon(ENWDungeonType InType, int32 InSeed, int32 InTier);

    UFUNCTION(BlueprintPure, Category="Dungeon")
    ENWDungeonType GetDungeonType() const { return DungeonType; }

    UFUNCTION(BlueprintPure, Category="Dungeon")
    int32 GetDungeonTier() const { return DungeonTier; }

protected:
    virtual void BeginPlay() override;

private:
    void BuildDungeonGeometry();
    void BuildDarkCastle(FRandomStream& Random);
    void BuildAncientCave(FRandomStream& Random);
    void SpawnDungeonPopulation();
    UStaticMesh* FindInstalledMesh(const TArray<FString>& Keywords) const;
    FName GetDungeonThemeName() const;

    UFUNCTION()
    void OnRep_DungeonConfiguration();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PrimaryStructures;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SecondaryStructures;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UTextRenderComponent> DungeonLabel;

    UPROPERTY(ReplicatedUsing=OnRep_DungeonConfiguration, EditAnywhere, Category="Dungeon")
    ENWDungeonType DungeonType = ENWDungeonType::DarkCastle;

    UPROPERTY(ReplicatedUsing=OnRep_DungeonConfiguration, EditAnywhere, Category="Dungeon")
    int32 DungeonSeed = 41001;

    UPROPERTY(ReplicatedUsing=OnRep_DungeonConfiguration, EditAnywhere, Category="Dungeon", meta=(ClampMin="1", ClampMax="10"))
    int32 DungeonTier = 2;

    // Packs Fab de arquitetura sao modulares e usam escalas/pivots distintos. Ate
    // existir adapter por pack, as dungeons usam primitives de 100 cm, para as quais
    // os transforms abaixo foram realmente autorados.
    UPROPERTY(EditAnywhere, Category="Dungeon|Visual")
    bool bUseInstalledDungeonMeshes = false;

    // A formula atual soma Tier*2. Base 3 deixa cada dungeon tier 3 com 9 mobs,
    // mantendo as duas dungeons, roaming, bosses e encontro inicial dentro do teto
    // de 42 sem criar 77 atores para imediatamente destruir dezenas deles.
    UPROPERTY(EditAnywhere, Category="Dungeon", meta=(ClampMin="1", ClampMax="12"))
    int32 RegularEnemyCount = 3;

    UPROPERTY(EditAnywhere, Category="Dungeon", meta=(ClampMin="1", ClampMax="4"))
    int32 GuardianCount = 1;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ANWEnemy>> SpawnedEnemies;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ANWDungeonGuardian>> SpawnedGuardians;
};
