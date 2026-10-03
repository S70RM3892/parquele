using UnrealBuildTool;

public class ParqueleEditorTarget : TargetRules
{
	public ParqueleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Parquele");
	}
}
