using UnrealBuildTool;
using System.Collections.Generic;

public class NewWorld2EditorTarget : TargetRules
{
    public NewWorld2EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new string[] { "NewWorld2" });
    }
}
