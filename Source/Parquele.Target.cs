using UnrealBuildTool;

public class ParqueleTarget : TargetRules
{
	public ParqueleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Parquele");
	}
}
