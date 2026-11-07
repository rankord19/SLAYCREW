// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SLAYCREW : ModuleRules
{
	public SLAYCREW(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
		if (Target.bBuildEditor) { PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "LevelEditor" }); }

		PublicIncludePaths.AddRange(new string[] {
			"SLAYCREW",
			"SLAYCREW/Variant_Platforming",
			"SLAYCREW/Variant_Platforming/Animation",
			"SLAYCREW/Variant_Combat",
			"SLAYCREW/Variant_Combat/AI",
			"SLAYCREW/Variant_Combat/Animation",
			"SLAYCREW/Variant_Combat/Gameplay",
			"SLAYCREW/Variant_Combat/Interfaces",
			"SLAYCREW/Variant_Combat/UI",
			"SLAYCREW/Variant_SideScrolling",
			"SLAYCREW/Variant_SideScrolling/AI",
			"SLAYCREW/Variant_SideScrolling/Gameplay",
			"SLAYCREW/Variant_SideScrolling/Interfaces",
			"SLAYCREW/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
