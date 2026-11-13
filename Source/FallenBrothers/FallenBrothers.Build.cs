// Fallen Brothers - game module build rules.

using UnrealBuildTool;

public class FallenBrothers : ModuleRules
{
	public FallenBrothers(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",

			// Creature AI: AI controllers, pathfinding on the navmesh.
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",

			// Project Settings page (Project Settings > Game > Fallen Brothers).
			"DeveloperSettings",

			// Deformable snow mesh (Environment/FBSnowField).
			"ProceduralMeshComponent",

			// Physical materials (slippery ice in Environment/FBWeatherZones).
			"PhysicsCore",

			// The lake (Unreal's Water plugin: Water Body Lake, Water Zone).
			"Water",

			// Finding the MetaHuman looks for the pedestrians (Town/FBStreetNetwork).
			"AssetRegistry"

			// Added in later steps:
			// "UMG", "Slate", "SlateCore"   (UI)
		});

		// Lets every file include headers by feature folder, e.g.
		// #include "Player/FBPlayerCharacter.h"
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
