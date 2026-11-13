// Fallen Brothers - Footsteps: the right sound for what you step on (asphalt, sidewalk, grass, gravel, wood floors,
// tiles, metal), splashing when the street is wet from the rain, and in the lake. Used by the player and townspeople.

#pragma once

#include "CoreMinimal.h"

class AActor;

namespace FBFootsteps
{
	/** One step at Foot (roughly where the sole touches). Volume 0..1. Ignore = the walker. */
	FALLENBROTHERS_API void Play(const AActor* Walker, const FVector& Foot, float Volume);
}
