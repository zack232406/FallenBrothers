// Fallen Brothers - UV light.
//
// Hand-held purple light. Reveals AFBUVTrace actors that are inside its cone,
// in range and not blocked by walls.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FBFlashlight.h"
#include "FBUVLight.generated.h"

UCLASS()
class FALLENBROTHERS_API AFBUVLight : public AFBFlashlight
{
	GENERATED_BODY()

public:
	AFBUVLight();

	virtual void Tick(float DeltaSeconds) override;

private:
	void RevealTraces();
};
