// Copyright TADemo. All Rights Reserved.

using UnrealBuildTool;

public class WeatherSystemEditor : ModuleRules
{
	public WeatherSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"WeatherSystem",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AppFramework",
			"AssetTools",
			"EditorFramework",
			"InputCore",
			"Projects",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd",
			"GameplayTags",
		});
	}
}
