// Fallen Brothers - Noise subsystem.
//
// Anything that makes a sound the creatures could hear reports it here:
// footsteps, gunshots, objects crashing to the floor...
// Loudness 1.0 = heard from a creature's full HearingRange.
//   crouch-walk ~0.1, walk ~0.4, sprint ~1.0, thrown object ~0.3-0.8, shotgun 2.0

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FBNoiseSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFBNoise, const FVector& /*Location*/, float /*Loudness*/, AActor* /*Instigator*/);

UCLASS()
class FALLENBROTHERS_API UFBNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Report a noise to every listening creature. */
	static void ReportNoise(const UObject* WorldContext, const FVector& Location, float Loudness, AActor* Instigator);

	FOnFBNoise OnNoise;
};
