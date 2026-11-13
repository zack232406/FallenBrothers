// Fallen Brothers - Map generator for case 1 (Harlow Falls).
//
// Put ONE in an empty level at (0,0,0), select it, press "Generate Map" in Details.
// It builds the whole playable map as normal actors you can then move / tweak / replace:
//   town (main street, 2 side streets, ~25 buildings with doors), church + cemetery,
//   country road to the mill and the barn, 6 forest blocks with a trail to a hunter's cabin,
//   street lamps, fences, physics props, atmosphere, and every gameplay actor of the demo case
//   (Zack, Martha, research computer, evidence, key, ghost, chandelier, grave, the Gaunt...).
// "Clear Generated" removes everything it made. Change "Seed" for a different layout of houses.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Town/FBTownBuilder.h"
#include "FBMapGenerator.generated.h"

class AFBBuilding;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;

UCLASS()
class FALLENBROTHERS_API AFBMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	AFBMapGenerator();

	/** Builds the whole map. Press it again to rebuild (clears the old one first). */
	UFUNCTION(CallInEditor, Category = "Generator")
	void GenerateMap();

	/** Removes every actor this generator made. */
	UFUNCTION(CallInEditor, Category = "Generator")
	void ClearGenerated();

	UPROPERTY(EditAnywhere, Category = "Generator")
	int32 Seed = 1952;

	/** Also place Zack, Martha, the ghost, evidence, keys... (turn off for scenery only). */
	UPROPERTY(EditAnywhere, Category = "Generator")
	bool bGameplayActors = true;

	/** Your real house models. If empty, grey-box houses (with our ambientCG materials) are used. */
	UPROPERTY(EditAnywhere, Category = "Generator|Your Models")
	TArray<FFBTownBuildingType> HouseModels;

	/** 0-1: how many houses use a real model when HouseModels is filled. */
	UPROPERTY(EditAnywhere, Category = "Generator|Your Models", meta = (ClampMin = "0", ClampMax = "1"))
	float RealHouseChance = 0.8f;

	/** Your real tree models (Megaplants...). If empty, grey cone trees are used. */
	UPROPERTY(EditAnywhere, Category = "Generator|Your Models")
	TArray<TObjectPtr<UStaticMesh>> TreeModels;

	/** Megaplants trees as they come (SK_... Skeletal Meshes). Mixed at random with TreeModels. */
	UPROPERTY(EditAnywhere, Category = "Generator|Your Models")
	TArray<TObjectPtr<USkeletalMesh>> SkeletalTreeModels;

	/** Rolling hills around the town (the town and the roads stay flat). */
	UPROPERTY(EditAnywhere, Category = "Generator|Land")
	bool bHills = true;

	/** A lake in the north-east woods (put a Water Body Lake on it: FB_Water.py does that). */
	UPROPERTY(EditAnywhere, Category = "Generator|Land")
	bool bLake = true;

	/** Mountain range around the whole map (also stops the player walking off the world). */
	UPROPERTY(EditAnywhere, Category = "Generator|Mountains")
	bool bMountains = true;

	/** Empty = uses /Game/FallenBrothers/Materials/MI_Rock (or MI_Ground if you don't have it yet). */
	UPROPERTY(EditAnywhere, Category = "Generator|Mountains")
	TObjectPtr<UMaterialInterface> MountainRockMaterial;

	/** Empty = uses /Game/FallenBrothers/Materials/MI_Snow if it exists (no snow caps otherwise). */
	UPROPERTY(EditAnywhere, Category = "Generator|Mountains")
	TObjectPtr<UMaterialInterface> MountainSnowMaterial;

	UPROPERTY(EditAnywhere, Category = "Generator|Mountains", meta = (ClampMin = "1000"))
	float MountainHeight = 45000.f;

	/** Trees per square km of forest. RTX 5060: 4000-8000 is fine with Nanite trees. */
	UPROPERTY(EditAnywhere, Category = "Generator|Your Models", meta = (ClampMin = "100"))
	float TreesPerKm2 = 5000.f;

private:
	struct FMaterials
	{
		UMaterialInterface* Asphalt = nullptr;
		UMaterialInterface* Paving = nullptr;
		UMaterialInterface* Grass = nullptr;
		UMaterialInterface* Ground = nullptr;
		UMaterialInterface* ForestGround = nullptr;
		UMaterialInterface* WoodSiding = nullptr;
		UMaterialInterface* Bricks = nullptr;
		UMaterialInterface* Planks = nullptr;
		UMaterialInterface* Roofing = nullptr;
		UMaterialInterface* WoodFloor = nullptr;
		UMaterialInterface* Rock = nullptr;
		UMaterialInterface* Snow = nullptr;
	};

	void LoadMaterials();

	template <class T>
	T* Spawn(const FVector& Location, float Yaw, const TCHAR* Folder, TFunctionRef<void(T*)> Setup);

	void BuildGround();
	void BuildRoads();
	void BuildTown(FRandomStream& Random);
	void BuildOutskirts();
	void BuildForests();
	void BuildLamps();
	void BuildMountains();
	void BuildGameplay();

	AFBBuilding* SpawnHouse(const FVector& Center, float Yaw, FRandomStream& Random);
	/** Places a downloaded house facing the road. bTestOnly: just works out its footprint (Base-relative). */
	bool SpawnModelHouse(const FFBTownBuildingType& Type, const FVector& RoadPoint, const FVector& ToRoad, float FrontDistance, bool bTestOnly = false, FBox2D* OutFootprint = nullptr);

	/** World Z of the ground (the rolling terrain) at a point. */
	float GroundAt(const FVector& World) const;
	void SpawnDoor(AFBBuilding* Building, bool bLocked = false, FName KeyId = NAME_None);
	void SpawnForest(const FVector& Center, const FVector2D& Size, int32 SeedOffset);

	/** Point inside a building, in the building's own space (+X = front door side). */
	static FVector InBuilding(const AActor* Building, const FVector& Local);
	/**
	 * A free spot on the floor inside a building for something HalfSize big (closest to the wanted Local spot):
	 * floor under it, roof over it, nothing in the way. Falls back to InBuilding(Local).
	 */
	static FVector FreeSpotInside(const AActor* Building, const FVector& Local, const FVector& HalfSize, float* OutYaw = nullptr);
	/** The same inside any box (world): zero vector when nothing is free. */
	static FVector FreeSpotInBox(UWorld* World, const FBox& Box, float Floor0, float BaseYaw, const FVector& HalfSize, float* OutYaw, const AActor* Ignore);

	FMaterials Mats;
	FVector Base = FVector::ZeroVector; // the generator's location: everything is relative to it

	// Key buildings, remembered for the gameplay pass.
	TWeakObjectPtr<AFBBuilding> Motel, Diner, Sheriff, Library, Church, Mill, Barn, Cabin;
	TWeakObjectPtr<AActor> CemeteryActor;
	TWeakObjectPtr<class AFBTerrain> Terrain;
	int32 SpawnedCount = 0;
};
