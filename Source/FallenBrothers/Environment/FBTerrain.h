// Fallen Brothers - Rolling ground around the town: hills, valleys and a lake basin.
//
// One big procedural mesh (with exact collision) that replaces the flat ground plane. The town, the roads and
// the building pads you list stay perfectly flat (height 0); outside them the land rolls into hills that grow
// towards the mountains. A lake basin can be dug in; put a Water Body Lake on it (WaterLevel).
//
// Everything that is placed on the ground (trees, rocks, grass) asks GroundZ() for the height, so nothing floats.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBTerrain.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/** A strip that stays flat: a road (A to B, HalfWidth to each side). */
USTRUCT(BlueprintType)
struct FFBFlatStrip
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") FVector2D A = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") FVector2D B = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float HalfWidth = 1000.f;
};

UCLASS()
class FALLENBROTHERS_API AFBTerrain : public AActor
{
	GENERATED_BODY()

public:
	AFBTerrain();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Total size (cm), centred on the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	FVector2D Size = FVector2D(50000.f, 45000.f);

	/** Grid spacing (cm). Smaller = smoother hills, more triangles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "100", ClampMax = "2000"))
	float CellSize = 250.f;

	/** Highest hills (cm) near the edge of the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float HillHeight = 2600.f;

	/** Size of one hill (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float HillScale = 11000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	int32 Seed = 7;

	/** Areas that stay flat (world XY): the town, building yards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Flat")
	TArray<FBox2D> FlatAreas;

	/** Roads that stay flat (world XY). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Flat")
	TArray<FFBFlatStrip> FlatStrips;

	/** How far (cm) the ground takes to rise from a flat area to the full hills. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Flat")
	float FlatBlend = 4500.f;

	/** Hills fade out (to 0) this close to the edge ellipse, where the mountains start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	FVector2D EdgeRadius = FVector2D(23500.f, 21000.f);

	// ---- Lake
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Lake")
	bool bLake = false;

	/** World XY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Lake")
	FVector2D LakeCenter = FVector2D::ZeroVector;

	/** Shoreline radii (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Lake")
	FVector2D LakeRadius = FVector2D(4500.f, 3000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Lake")
	float LakeDepth = 650.f;

	/** Water surface height, relative to the actor (the shore sits just above it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Lake")
	float WaterLevel = -40.f;

	// ---- Look
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Look")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	/** Steep slopes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Look")
	TObjectPtr<UMaterialInterface> SlopeMaterial;

	/** Lake bed and shore. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Look")
	TObjectPtr<UMaterialInterface> ShoreMaterial;

	/** Ground height at a world XY, relative to the actor. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	float HeightAt(const FVector2D& World) const;

	/** True if the point is under the lake's water. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	bool IsUnderWater(const FVector2D& World, float Margin = 0.f) const;

	/** World Z of the ground at XY (the terrain in this world if there is one, else Default). */
	static float GroundZ(const UWorld* World, const FVector2D& XY, float Default);

	/** The terrain of this world (or null). */
	static AFBTerrain* Find(const UWorld* World);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Mesh;

private:
	void Build();
	float FlatDistance(const FVector2D& P) const;
	float LakeAlpha(const FVector2D& P) const; // 0 at the centre, 1 at the shore
};
