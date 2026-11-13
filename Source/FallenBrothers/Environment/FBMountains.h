// Fallen Brothers - Mountain range around a map.
//
// A ring of real-looking mountains (ridged noise, like erosion ridges) around the playable area:
// foothills start at the edge of the map, peaks rise behind the forests, snow caps on top.
// It also closes the map: the slopes get too steep to climb, so nobody walks off the world.
//
// Put it at the centre of the map, on the ground (Z = ground height). The FBMapGenerator places one
// automatically. Change Seed for a different skyline. Everything rebuilds live in the editor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBMountains.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class USkeletalMesh;
class USkinnedMeshComponent;

UCLASS()
class FALLENBROTHERS_API AFBMountains : public AActor
{
	GENERATED_BODY()

public:
	AFBMountains();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Where the foothills start (cm, X and Y radius of the ellipse). Put this a little INSIDE your ground. */
	UPROPERTY(EditAnywhere, Category = "Mountains")
	FVector2D InnerRadius = FVector2D(23000.f, 21000.f);

	/** How far away the last ridges are (cm). 250000 = 2.5 km. */
	UPROPERTY(EditAnywhere, Category = "Mountains", meta = (ClampMin = "10000"))
	float OuterDistance = 250000.f;

	/** Highest peaks (cm). 45000 = 450 m. */
	UPROPERTY(EditAnywhere, Category = "Mountains", meta = (ClampMin = "1000"))
	float PeakHeight = 45000.f;

	/** How far from the foothills the mountains reach full height (0-1 of the depth). */
	UPROPERTY(EditAnywhere, Category = "Mountains", meta = (ClampMin = "0.02", ClampMax = "1"))
	float RiseDistance = 0.12f;

	/** Size of the big shapes (cm). Bigger = wider mountains. */
	UPROPERTY(EditAnywhere, Category = "Mountains", meta = (ClampMin = "1000"))
	float NoiseScale = 60000.f;

	/** 0 = soft hills, 1 = sharp alpine ridges. */
	UPROPERTY(EditAnywhere, Category = "Mountains", meta = (ClampMin = "0", ClampMax = "1"))
	float Sharpness = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Mountains")
	int32 Seed = 1952;

	/** Snow above this height (cm). Set very high for no snow. */
	UPROPERTY(EditAnywhere, Category = "Mountains")
	float SnowLine = 28000.f;

	/** Rock / forest-ground material (world-aligned, e.g. MI_Rock). */
	UPROPERTY(EditAnywhere, Category = "Mountains")
	TObjectPtr<UMaterialInterface> RockMaterial;

	/** Material for the snow caps (e.g. MI_Snow). Empty = rock everywhere. */
	UPROPERTY(EditAnywhere, Category = "Mountains")
	TObjectPtr<UMaterialInterface> SnowMaterial;

	/** Trees on the lower slopes (Megaplants SK_ trees). Empty = bare mountains. */
	UPROPERTY(EditAnywhere, Category = "Mountains|Trees")
	TArray<TObjectPtr<USkeletalMesh>> TreeMeshes;

	UPROPERTY(EditAnywhere, Category = "Mountains|Trees", meta = (ClampMin = "0", ClampMax = "40000"))
	int32 TreeCount = 9000;

	/** No trees above this height (cm). */
	UPROPERTY(EditAnywhere, Category = "Mountains|Trees")
	float TreeLine = 10000.f;

	/** How far back the forest goes (0-1 of the mountains' depth). */
	UPROPERTY(EditAnywhere, Category = "Mountains|Trees", meta = (ClampMin = "0.01", ClampMax = "1"))
	float TreeDepth = 0.35f;

	/** Detail: points around the ring and from front to back. RTX 5060: defaults are fine. */
	UPROPERTY(EditAnywhere, Category = "Mountains|Quality", meta = (ClampMin = "32", ClampMax = "1024"))
	int32 SegmentsAround = 384;

	UPROPERTY(EditAnywhere, Category = "Mountains|Quality", meta = (ClampMin = "8", ClampMax = "256"))
	int32 SegmentsDeep = 96;

	/** Height (cm) at a world point. Usable for placing things on the slopes. */
	float HeightAt(const FVector2D& World, float DepthAlpha) const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Mesh;

private:
	void Build();
	float Ridged(const FVector2D& P) const;
};
