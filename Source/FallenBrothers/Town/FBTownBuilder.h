// Fallen Brothers - Town builder.
//
// Lays out a whole small American town from YOUR real models (Fab / Sketchfab / Megaplants):
//   - main street + side streets with asphalt, kerbs, sidewalks, centre lines, crosswalks
//   - rows of buildings facing the road, varied setbacks, gaps and empty lots
//   - street lamps (with real lights), utility poles, parked cars, bins, hydrants, mailboxes
//   - trees / sheds behind the houses
// Everything is instanced (cheap), rebuilds live in the editor, and "Seed" rerolls the town.
//
// Hand-placed story buildings (diner, library, church...) are kept free:
// give them the Actor Tag "TownReserved" and the builder won't build over them.

#pragma once

#include "CoreMinimal.h"
#include "Town/FBBlockoutActor.h"
#include "FBTownBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
struct FRandomStream;

USTRUCT(BlueprintType)
struct FFBTownBuildingType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Building")
	TObjectPtr<UStaticMesh> Mesh;

	/** Turn the model so its FRONT DOOR faces the red X arrow. Usually 0, 90, -90 or 180. */
	UPROPERTY(EditAnywhere, Category = "Building")
	float FrontYaw = 0.f;

	UPROPERTY(EditAnywhere, Category = "Building", meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	/** How often it's picked compared to the others. */
	UPROPERTY(EditAnywhere, Category = "Building", meta = (ClampMin = "0"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Building")
	bool bMainStreet = true;

	UPROPERTY(EditAnywhere, Category = "Building")
	bool bSideStreets = true;
};

UENUM(BlueprintType)
enum class EFBTownSpot : uint8
{
	Curb          UMETA(DisplayName = "Kerb (lamps, poles, hydrants)"),
	SidewalkBack  UMETA(DisplayName = "Back of sidewalk (bins, benches, mailboxes)"),
	ParkingLane   UMETA(DisplayName = "Parking lane (cars)"),
	FrontYard     UMETA(DisplayName = "Front yard (fences, bushes)"),
	BehindLots    UMETA(DisplayName = "Behind buildings (trees, sheds)")
};

USTRUCT(BlueprintType)
struct FFBTownPropType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Prop")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, Category = "Prop")
	EFBTownSpot Spot = EFBTownSpot::Curb;

	/** Average distance between two of them (cm). */
	UPROPERTY(EditAnywhere, Category = "Prop", meta = (ClampMin = "100"))
	float Spacing = 2000.f;

	/** 0-1: chance that a spot actually gets one. */
	UPROPERTY(EditAnywhere, Category = "Prop", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 1.f;

	/** Turn the model so its front faces the red X arrow. */
	UPROPERTY(EditAnywhere, Category = "Prop")
	float FrontYaw = 0.f;

	UPROPERTY(EditAnywhere, Category = "Prop", meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	/** Random size variation (0.1 = +-10%). */
	UPROPERTY(EditAnywhere, Category = "Prop", meta = (ClampMin = "0", ClampMax = "0.5"))
	float ScaleJitter = 0.1f;

	/** Random rotation (degrees). Trees: 180. Lamps: 0. */
	UPROPERTY(EditAnywhere, Category = "Prop")
	float RandomYaw = 0.f;

	/** Street lamps / porch lights: add a real light at LightOffset (relative to the model). */
	UPROPERTY(EditAnywhere, Category = "Prop|Light")
	bool bLight = false;

	UPROPERTY(EditAnywhere, Category = "Prop|Light", meta = (EditCondition = "bLight"))
	FVector LightOffset = FVector(150.f, 0.f, 460.f);

	UPROPERTY(EditAnywhere, Category = "Prop|Light", meta = (EditCondition = "bLight"))
	float LightIntensity = 300.f;

	UPROPERTY(EditAnywhere, Category = "Prop|Light", meta = (EditCondition = "bLight"))
	FLinearColor LightColor = FLinearColor(1.f, 0.62f, 0.3f);
};

UCLASS()
class FALLENBROTHERS_API AFBTownBuilder : public AFBBlockoutActor
{
	GENERATED_BODY()

public:
	AFBTownBuilder();

	/** Change for a different town (same models, new arrangement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town")
	int32 Seed = 1952;

	// ---------------------------------------------------------------- Streets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Streets", meta = (ClampMin = "2000"))
	float MainStreetLength = 14000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Streets", meta = (ClampMin = "0", ClampMax = "6"))
	int32 SideStreets = 2;

	/** How far each side street runs away from Main Street (each direction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Streets", meta = (ClampMin = "1000"))
	float SideStreetLength = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Streets")
	float RoadWidth = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Streets")
	float SidewalkWidth = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Streets")
	float CurbHeight = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Streets")
	TObjectPtr<UMaterialInterface> RoadMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Streets")
	TObjectPtr<UMaterialInterface> SidewalkMaterial;

	// ---------------------------------------------------------------- Lots
	/** Gap between sidewalk and building front (front yard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Lots")
	float Setback = 300.f;

	/** Random extra setback per building. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Lots")
	float SetbackJitter = 250.f;

	/** Gap between two buildings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Lots")
	float LotGap = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Lots")
	float LotGapJitter = 400.f;

	/** 0-1: chance of an empty lot (overgrown, abandoned feel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Lots", meta = (ClampMin = "0", ClampMax = "1"))
	float EmptyLotChance = 0.1f;

	/** Side-street lots start this far from Main Street (keeps corners clear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Lots")
	float SideLotStart = 2500.f;

	/** Pushes models slightly into the ground so no gaps show under them (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Lots")
	float SinkDepth = 5.f;

	/** Actors with this tag are kept free (hand-placed story buildings). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Lots")
	FName ReservedTag = TEXT("TownReserved");

	// ---------------------------------------------------------------- Content
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Content")
	TArray<FFBTownBuildingType> Buildings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Content")
	TArray<FFBTownPropType> Props;

	/** Real lights are expensive - lamps beyond this count stay dark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town|Content", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MaxLights = 30;

protected:
	virtual void Rebuild() override;
	virtual void ApplyColors() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Asphalt;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Sidewalks;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Paint;

private:
	/** One street side: a straight stretch with lots on one side. */
	struct FRow
	{
		FVector Origin;   // point on the road's centre line where s = 0
		FVector Dir;      // along the road (unit, X or Y)
		FVector Normal;   // from road towards the lots (unit, X or Y)
		float StartS = 0.f;
		float EndS = 0.f;
		bool bMainStreet = true;
	};

	void BuildStreets(TArray<FRow>& OutRows);
	void FillRow(const FRow& Row, FRandomStream& Random);
	void PlaceProps(const FRow& Row, FRandomStream& Random);

	const FFBTownBuildingType* PickBuilding(bool bMainStreet, FRandomStream& Random) const;
	UInstancedStaticMeshComponent* GetMeshISM(UStaticMesh* Mesh);
	void AddLight(const FVector& Location, const FFBTownPropType& Type);
	bool IsBlocked(const FBox& LocalBox) const;

	/** Places a mesh so its face is DistanceFromRoad away from the row's road centre line. */
	FBox PlaceMesh(UStaticMesh* Mesh, float Yaw, float Scale, const FRow& Row, float S, float DistanceFromRoad,
	               float GroundZ, bool bDryRun, FTransform& OutTransform) const;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> MeshISMs;

	TArray<FBox> Occupied;   // local-space boxes of everything placed (no overlaps)
	TArray<FBox> Reserved;   // local-space boxes of "TownReserved" actors
	TArray<float> CrossX;    // side street positions on Main Street
	int32 LightCount = 0;
};
