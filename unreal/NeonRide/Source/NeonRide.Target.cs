using UnrealBuildTool;

public class NeonRideTarget : TargetRules
{
    public NeonRideTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("NeonRide");
    }
}
