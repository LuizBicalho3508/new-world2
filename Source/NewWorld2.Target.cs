using UnrealBuildTool;
using System.Collections.Generic;

public class NewWorld2Target : TargetRules
{
    public NewWorld2Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new string[] { "NewWorld2" });
    }
}
