using UnrealBuildTool;

public class NewWorld2 : ModuleRules
{
    public NewWorld2(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
