// Copyright TADemo. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class CommonToolsEditor : ModuleRules
{
	public CommonToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"Niagara",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"DeveloperSettings",
		});
	}
}
