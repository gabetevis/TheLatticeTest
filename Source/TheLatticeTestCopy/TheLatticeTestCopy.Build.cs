// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class TheLatticeTestCopy : ModuleRules
{
	public TheLatticeTestCopy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets feature folders include each other from the module root, e.g. "VTT/VTTComponent.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",       // player input
			"UMG",                 // VTT overlay widget
			"DeveloperSettings"    // Project Settings > Game > VTT
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",               // VTT overlay drawing
			"SlateCore",
			"CableComponent"       // grapple rope
		});
	}
}
