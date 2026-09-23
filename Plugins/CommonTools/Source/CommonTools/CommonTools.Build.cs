// Copyright TADemo. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class CommonTools : ModuleRules
{
	public CommonTools(ReadOnlyTargetRules Target) : base(Target)
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
