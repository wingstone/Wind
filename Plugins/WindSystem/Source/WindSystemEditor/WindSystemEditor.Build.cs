// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WindSystemEditor : ModuleRules
{
	public WindSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"WindSystemRuntime",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
		});
	}
}
