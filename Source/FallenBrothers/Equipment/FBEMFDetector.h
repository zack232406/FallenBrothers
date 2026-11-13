// Fallen Brothers - EMF detector.
//
// Hand-held reader. Left click switches it on/off. Shows a 5-LED meter on the HUD
// that rises near paranormal sources (UFBParanormalSourceComponent).

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FBEquipmentBase.h"
#include "FBEMFDetector.generated.h"

UCLASS()
class FALLENBROTHERS_API AFBEMFDetector : public AFBEquipmentBase
{
	GENERATED_BODY()

public:
	AFBEMFDetector();

	virtual void Tick(float DeltaSeconds) override;
	virtual void UseEquipment() override { bPoweredOn = !bPoweredOn; }
	virtual void DrawEquipmentHUD(AFBHUD* HUD, float UIScale) override;

	/** Current reading 0-5 (0 when switched off). */
	UFUNCTION(BlueprintPure, Category = "EMF")
	int32 GetDisplayedLevel() const { return bPoweredOn ? DisplayedLevel : 0; }

protected:
	/** How quickly the needle follows the real value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EMF")
	float ResponseSpeed = 6.f;

	/** Random noise so the reading never looks perfectly static. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EMF")
	float Jitter = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EMF")
	bool bPoweredOn = true;

private:
	float SmoothedLevel = 0.f;
	int32 DisplayedLevel = 0;
	float NoiseTime = 0.f;
};
