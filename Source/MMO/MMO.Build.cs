// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MMO : ModuleRules
{
	public MMO(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore",
			"NavigationSystem",
			"Niagara",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MMO",
			"MMO/Variant_Platforming",
			"MMO/Variant_Platforming/Animation",
			"MMO/Variant_Combat",
			"MMO/Variant_Combat/AI",
			"MMO/Variant_Combat/Animation",
			"MMO/Variant_Combat/Gameplay",
			"MMO/Variant_Combat/Interfaces",
			"MMO/Variant_Combat/UI",
			"MMO/Variant_SideScrolling",
			"MMO/Variant_SideScrolling/AI",
			"MMO/Variant_SideScrolling/Gameplay",
			"MMO/Variant_SideScrolling/Interfaces",
			"MMO/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
