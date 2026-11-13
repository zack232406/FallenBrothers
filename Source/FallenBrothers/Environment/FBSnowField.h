// Fallen Brothers - Deformable snow (RDR2-style tracks).
//
// Put ONE in a snowy level (on the ground), set Area Size to cover the walkable snow and pick
// the snow material (M_FB_Snow). Then, while playing:
//   - Chris, Zack, NPCs and creatures leave real footprints + a trench in deep snow,
//   - thrown / rolling physics props carve the snow,
//   - everyone walks a bit slower in snow, and feet sink in (the snow is a visual layer above the ground),
//   - buildings, roads and actors tagged "NoSnow" stay clear (plowed / indoors),
//   - fresh snow slowly fills old tracks (Refill Rate).
//
// How it works: footprints are drawn into a "memory" texture covering the area. A dense mesh that
// follows the player reads it in the material and pushes the snow down where something walked.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBSnowField.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;

UCLASS()
class FALLENBROTHERS_API AFBSnowField : public AActor
{
	GENERATED_BODY()

public:
	AFBSnowField();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Size of the snowy area (cm), centred on this actor. Tracks are remembered inside it. */
	UPROPERTY(EditAnywhere, Category = "Snow")
	FVector2D AreaSize = FVector2D(30000.f, 30000.f);

	/** M_FB_Snow (or an instance of it). */
	UPROPERTY(EditAnywhere, Category = "Snow")
	TObjectPtr<UMaterialInterface> SnowMaterial;

	/** How deep the snow is (cm). Feet sink this much. */
	UPROPERTY(EditAnywhere, Category = "Snow", meta = (ClampMin = "2", ClampMax = "80"))
	float SnowDepth = 22.f;

	/** Walking speed in snow (1 = normal). */
	UPROPERTY(EditAnywhere, Category = "Snow", meta = (ClampMin = "0.2", ClampMax = "1"))
	float SpeedMultiplier = 0.8f;

	/** Fresh snow fills old tracks: fraction per minute (0 = tracks stay forever). */
	UPROPERTY(EditAnywhere, Category = "Snow", meta = (ClampMin = "0", ClampMax = "1"))
	float RefillPerMinute = 0.1f;

	/** Buildings and roads stay free of snow (plowed / indoors). Actors tagged "NoSnow" too. */
	UPROPERTY(EditAnywhere, Category = "Snow")
	bool bClearUnderBuildingsAndRoads = true;

	/** Shows what the snow sees on screen while playing (for testing). */
	UPROPERTY(EditAnywhere, Category = "Snow")
	bool bDebug = false;

	// ---- Footprints

	UPROPERTY(EditAnywhere, Category = "Snow|Footprints")
	float StrideLength = 65.f;

	/** Length x width of one boot print (cm). Scaled up for big creatures. */
	UPROPERTY(EditAnywhere, Category = "Snow|Footprints")
	FVector2D FootprintSize = FVector2D(34.f, 16.f);

	/** Deep snow: legs plough a continuous trench, not only prints. */
	UPROPERTY(EditAnywhere, Category = "Snow|Footprints")
	bool bTrench = true;

	UPROPERTY(EditAnywhere, Category = "Snow|Footprints", meta = (EditCondition = "bTrench"))
	float TrenchWidth = 55.f;

	/** 0-1: how much the trench pushes down (prints always go to the bottom). */
	UPROPERTY(EditAnywhere, Category = "Snow|Footprints", meta = (EditCondition = "bTrench", ClampMin = "0", ClampMax = "1"))
	float TrenchStrength = 0.35f;

	/** Thrown / rolling physics props (pipes, crates...) carve the snow. */
	UPROPERTY(EditAnywhere, Category = "Snow|Footprints")
	bool bPropsCarve = true;

	// ---- Quality (RTX 5060: defaults are fine)

	/** Track texture size. 4096 on a 300 m area = ~7 cm per pixel. */
	UPROPERTY(EditAnywhere, Category = "Snow|Quality", meta = (ClampMin = "512", ClampMax = "8192"))
	int32 TrailResolution = 4096;

	/** Radius of the detailed snow mesh around the player (cm). Beyond it the ground material shows. */
	UPROPERTY(EditAnywhere, Category = "Snow|Quality", meta = (ClampMin = "500", ClampMax = "5000"))
	float DetailRadius = 2000.f;

	/** Distance between snow mesh points (cm). Smaller = sharper prints, heavier. */
	UPROPERTY(EditAnywhere, Category = "Snow|Quality", meta = (ClampMin = "4", ClampMax = "50"))
	float VertexSpacing = 10.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> SnowMesh;

private:
	struct FStamp
	{
		FVector2D Center;   // pixels
		FVector2D Size;     // pixels
		float Yaw = 0.f;
		float Strength = 1.f;
		bool bSolid = false; // white rectangle instead of the soft brush
	};

	struct FWalker
	{
		FVector LastStep = FVector::ZeroVector;
		FVector LastTrench = FVector::ZeroVector;
		bool bLeftFoot = false;
		bool bValid = false;
	};

	void BuildGrid();
	void CreateTextures();
	void UpdateMaterial();
	void CollectClearAreas();

	void UpdateWalkers();
	void UpdateProps();
	void FollowPlayer();
	void Flush(float DeltaSeconds);

	/** Queue a print at a world point. Size in cm, yaw in degrees. */
	void Stamp(const FVector& World, const FVector2D& SizeCm, float Yaw, float Strength, bool bSolid = false);

	bool IsInside(const FVector& World) const;
	FVector2D AreaMin() const;
	float GroundZ() const { return GetActorLocation().Z; }

	TArray<FStamp> Pending;
	TArray<FBox> ClearAreas;
	TMap<TWeakObjectPtr<AActor>, FWalker> Walkers;
	TMap<TWeakObjectPtr<AActor>, FVector> PropLast;
	float RefillTimer = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> TrailTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BrushTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WhiteTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SnowMID;
};
