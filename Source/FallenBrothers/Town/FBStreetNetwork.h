// Fallen Brothers - The town's street network, read from the generated roads when play starts.
//
// Sidewalks: every road piece with a sidewalk gives two straight walkable strips (one each side). Walkers follow a
// strip to its end, then turn the corner, carry on across the side street, or cross the road to the other side.
// Lanes: every road piece gives its driving lanes per direction (US style: drive on the right) for the traffic.
// It also fills the town with people when play starts (the crowd) and sits some of them on the benches.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Town/FBTownsperson.h"
#include "FBStreetNetwork.generated.h"


/** One straight walkable strip (sidewalk centre line). */
struct FFBWalkStrip
{
	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;
	float Width = 250.f;
};

/** One generated road piece: start (centre line), direction, sizes. */
struct FFBRoadPiece
{
	FVector Start = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;   // along the road
	FVector Right = FVector::RightVector;   // +Y of the road actor
	float Length = 0.f;
	float HalfAsphalt = 400.f;
	float LaneWidth = 350.f;
	int32 Lanes = 1;                        // per direction
	float Sidewalk = 0.f;
	bool bTown = false;                     // has sidewalks (in town)
	bool bCrossStart = false;
	bool bCrossEnd = false;
};

UCLASS()
class FALLENBROTHERS_API UFBStreetNetwork : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UFBStreetNetwork* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Reads the roads (once). */
	void EnsureBuilt();

	const TArray<FFBWalkStrip>& Strips() const { return WalkStrips; }
	const TArray<FFBRoadPiece>& Roads() const { return RoadPieces; }

	/** Nearest strip to P (within MaxDist), INDEX_NONE if none. OutT = 0..1 along it. */
	int32 NearestStrip(const FVector& P, float MaxDist, float* OutT = nullptr) const;

	/**
	 * Where to go on from the end End of strip From: another strip starting nearby (round the corner, straight on
	 * across the side street, or across the road). INDEX_NONE = dead end. OutFromA: start at its A end.
	 */
	int32 NextStrip(int32 From, const FVector& End, FRandomStream& Random, bool& bOutFromA) const;

	/** Everybody walking around (for keeping out of each other's way, and for the cars). */
	void Register(AFBTownsperson* P) { Walkers.AddUnique(P); }
	void Unregister(AFBTownsperson* P) { Walkers.Remove(P); }
	const TArray<TWeakObjectPtr<AFBTownsperson>>& GetWalkers() const { return Walkers; }

	/** Ground height under a point (sidewalk, road, terrain...), ignoring people. */
	static bool GroundAt(const UWorld* World, const FVector& P, float& OutZ, const AActor* Ignore = nullptr);

	/** After dark (from the atmosphere's time of day): the town empties out. */
	static bool IsNight(const UWorld* World);

private:
	void SpawnCrowd();
	void SpawnPedestrians();
	void SeatPeople();

	bool bBuilt = false;
	bool bNight = false;
	TArray<FFBWalkStrip> WalkStrips;
	TArray<FFBRoadPiece> RoadPieces;
	TArray<TWeakObjectPtr<AFBTownsperson>> Walkers;
};
