// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WindSystemRuntime : ModuleRules
{
	public WindSystemRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Access Renderer private/internal headers for Scene Extension API
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Private"));
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Internal"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"Renderer",
			"Projects",
		});
	}
}
