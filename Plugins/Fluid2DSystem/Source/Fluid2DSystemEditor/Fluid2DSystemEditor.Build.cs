// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Fluid2DSystemEditor : ModuleRules
{
	public Fluid2DSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "Fluid2DSystemEditorPCH.h";

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Fluid2DSystemRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
