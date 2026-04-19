// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Wind : ModuleRules
{
	public Wind(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "WindPCH.h";

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Wind",
			"Wind/Variant_Platforming",
			"Wind/Variant_Platforming/Animation",
			"Wind/Variant_Combat",
			"Wind/Variant_Combat/AI",
			"Wind/Variant_Combat/Animation",
			"Wind/Variant_Combat/Gameplay",
			"Wind/Variant_Combat/Interfaces",
			"Wind/Variant_Combat/UI",
			"Wind/Variant_SideScrolling",
			"Wind/Variant_SideScrolling/AI",
			"Wind/Variant_SideScrolling/Gameplay",
			"Wind/Variant_SideScrolling/Interfaces",
			"Wind/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
