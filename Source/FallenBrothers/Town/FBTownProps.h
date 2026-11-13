// Fallen Brothers - Town blockout pieces: road, fence, cemetery, forest.
//
// All of them rebuild live in the editor when you change their values in Details.

#pragma once

#include "CoreMinimal.h"
#include "Town/FBBlockoutActor.h"
#include "FBTownProps.generated.h"

class UInstancedStaticMeshComponent;
class USkinnedMeshComponent;
class USkeletalMesh;

/** Straight road along the actor's forward arrow, with sidewalks and a dashed centre line. */
UCLASS()
class FALLENBROTHERS_API AFBRoad : public AFBBlockoutActor
{
	GENERATED_BODY()

public:
	AFBRoad();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road", meta = (ClampMin = "100"))
	float Length = 6000.f;

	/** Asphalt width (two lanes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road", meta = (ClampMin = "200"))
	float RoadWidth = 800.f;

	/** 0 = no sidewalks (country road). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road", meta = (ClampMin = "0"))
	float SidewalkWidth = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	bool bCenterLine = true;

	/** Travel lanes in each direction (1 = two-lane street, 2 = four-lane main street). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Lanes", meta = (ClampMin = "1", ClampMax = "3"))
	int32 LanesPerDirection = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Lanes", meta = (ClampMin = "250"))
	float LaneWidth = 350.f;

	/** Parking lane along each curb (0 = none). Lies between the travel lanes and the sidewalk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Lanes", meta = (ClampMin = "0"))
	float ParkingWidth = 0.f;

	/** Real US road paint: double yellow centre line, dashed white lane lines, solid white edge lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Lanes")
	bool bRealMarkings = false;

	/** Zebra crosswalk + stop line at the start / end of this piece (where it meets an intersection). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Lanes")
	bool bCrosswalkAtStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Lanes")
	bool bCrosswalkAtEnd = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FLinearColor AsphaltColor = FLinearColor(0.06f, 0.06f, 0.065f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FLinearColor SidewalkColor = FLinearColor(0.32f, 0.31f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TObjectPtr<UMaterialInterface> AsphaltMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TObjectPtr<UMaterialInterface> SidewalkMaterial;

protected:
	virtual void Rebuild() override;
	virtual void ApplyColors() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Asphalt;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Sidewalks;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Paint;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> WhitePaint;
};

/** Crossing of two streets: an asphalt square plus the four sidewalk corners (with kerb) around it. */
UCLASS()
class FALLENBROTHERS_API AFBIntersection : public AFBBlockoutActor
{
	GENERATED_BODY()

public:
	AFBIntersection();

	/** Asphalt square size: X = width of the street running along Y, Y = width of the street running along X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intersection")
	FVector2D AsphaltSize = FVector2D(800.f, 800.f);

	/** Sidewalk widths of the two streets (corner pieces). X = sidewalk of the street running along Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intersection")
	FVector2D Sidewalk = FVector2D(250.f, 250.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intersection")
	TObjectPtr<UMaterialInterface> AsphaltMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intersection")
	TObjectPtr<UMaterialInterface> SidewalkMaterial;

protected:
	virtual void Rebuild() override;
	virtual void ApplyColors() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Asphalt;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Corners;
};

/** Straight fence along the actor's forward arrow. Optional gate gap. */
UCLASS()
class FALLENBROTHERS_API AFBFence : public AFBBlockoutActor
{
	GENERATED_BODY()

public:
	AFBFence();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence", meta = (ClampMin = "100"))
	float Length = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence", meta = (ClampMin = "50"))
	float Height = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence", meta = (ClampMin = "50"))
	float PostSpacing = 250.f;

	/** Where the gate is (0 = start, 1 = end). Negative = no gate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
	float GatePosition = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
	float GateWidth = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
	FLinearColor Color = FLinearColor(0.2f, 0.15f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
	TObjectPtr<UMaterialInterface> Material;

protected:
	virtual void Rebuild() override;
	virtual void ApplyColors() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Posts;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Rails;
};

/** Fenced graveyard with rows of headstones. The gate is on the front (forward arrow) side. */
UCLASS()
class FALLENBROTHERS_API AFBCemetery : public AFBBlockoutActor
{
	GENERATED_BODY()

public:
	AFBCemetery();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery", meta = (ClampMin = "1", ClampMax = "30"))
	int32 Rows = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery", meta = (ClampMin = "1", ClampMax = "30"))
	int32 GravesPerRow = 8;

	/** Front-back distance between rows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	float RowSpacing = 350.f;

	/** Left-right distance between graves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	float GraveSpacing = 220.f;

	/** Change for a different random layout (missing / tilted stones). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	int32 Seed = 1952;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	bool bFence = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	FLinearColor StoneColor = FLinearColor(0.35f, 0.35f, 0.34f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	FLinearColor GroundColor = FLinearColor(0.08f, 0.11f, 0.06f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	FLinearColor FenceColor = FLinearColor(0.08f, 0.08f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	TObjectPtr<UMaterialInterface> GroundMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	TObjectPtr<UMaterialInterface> StoneMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cemetery")
	TObjectPtr<UMaterialInterface> FenceMaterial;

protected:
	virtual void Rebuild() override;
	virtual void ApplyColors() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Ground;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Stones;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Posts;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Rails;
};

/** Random pine trees inside a rectangle (the Gaunt's woods). Trunks block, needles don't. */
UCLASS()
class FALLENBROTHERS_API AFBForest : public AFBBlockoutActor
{
	GENERATED_BODY()

public:
	AFBForest();

	/** Rectangle size (cm), centred on the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FVector2D Size = FVector2D(8000.f, 8000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "1", ClampMax = "2000"))
	int32 TreeCount = 180;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float MinHeight = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float MaxHeight = 1800.f;

	/** Trees never closer than this (cm) - keeps paths walkable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float MinDistance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	int32 Seed = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FLinearColor TrunkColor = FLinearColor(0.12f, 0.08f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FLinearColor NeedleColor = FLinearColor(0.03f, 0.08f, 0.04f);

	/** Your real tree models (e.g. Megaplants). If set, these are used instead of the grey cones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Real Trees")
	TArray<TObjectPtr<UStaticMesh>> TreeMeshes;

	/** Megaplants trees (SK_... Skeletal Meshes). Used as they are - full Nanite detail, no "Make Static Mesh". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Real Trees")
	TArray<TObjectPtr<USkeletalMesh>> SkeletalTreeMeshes;

	/** Invisible collision cylinder per real tree (so you can't walk through trunks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Real Trees")
	float TrunkCollisionDiameter = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Real Trees", meta = (ClampMin = "0.05"))
	float TreeScaleMin = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Real Trees", meta = (ClampMin = "0.05"))
	float TreeScaleMax = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	TObjectPtr<UMaterialInterface> TrunkMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	TObjectPtr<UMaterialInterface> NeedleMaterial;

protected:
	virtual void Rebuild() override;
	virtual void ApplyColors() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Trunks;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Needles;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> TrunkBlockers;

private:
	/** One instanced skinned component per Megaplants tree model, remade every rebuild.
	 *  (Made through reflection: the engine header doesn't compile with the VS 2026 compiler.) */
	USkinnedMeshComponent* GetSkinnedTrees(USkeletalMesh* Mesh);
	static void AddSkinnedInstance(USkinnedMeshComponent* Comp, const FTransform& Transform);

	UPROPERTY(Transient)
	TMap<TObjectPtr<USkeletalMesh>, TObjectPtr<USkinnedMeshComponent>> SkinnedTrees;
};
