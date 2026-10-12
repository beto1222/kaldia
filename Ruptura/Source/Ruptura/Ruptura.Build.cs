using UnrealBuildTool;

public class Ruptura : ModuleRules
{
	public Ruptura(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"PhysicsCore"
		});

		// Permite includes como "Combat/WeaponComponent.h" a partir da raiz do módulo.
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
