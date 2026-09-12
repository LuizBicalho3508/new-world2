using UnrealBuildTool;

public class NewWorld2 : ModuleRules
{
    public NewWorld2(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "NewWorld2PCH.h";

        // Compatibilidade temporaria: o codigo legado usa NEWORLD2_API,
        // mas o UnrealBuildTool gera NEWWORLD2_API para o modulo NewWorld2.
        // Mantemos o alias centralizado para destravar a compilacao em UE 5.8.2
        // sem precisar alterar dezenas de headers de uma vez.
        PublicDefinitions.Add("NEWORLD2_API=NEWWORLD2_API");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "ProceduralMeshComponent",
            "UMG",
            "AssetRegistry",
            "PCG",
            "NavigationSystem",
            "AIModule",
            "Niagara"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore"
        });
    }
}
