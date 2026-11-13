// Fallen Brothers - Flashlight.
//
// Chest-mounted torch (toggle with T). Has a battery, dims and flickers when the
// battery is low, and flickers on its own near strong paranormal activity.
// AFBUVLight is a child of this class.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FBEquipmentBase.h"
#include "FBFlashlight.generated.h"

class USpotLightComponent;

UCLASS()
class FALLENBROTHERS_API AFBFlashlight : public AFBEquipmentBase
{
	GENERATED_BODY()

public:
	AFBFlashlight();

	virtual void Tick(float DeltaSeconds) override;
	virtual void UseEquipment() override { SetLightOn(!bLightOn); }
	virtual void OnUnequipped() override;
	virtual void DrawEquipmentHUD(AFBHUD* HUD, float UIScale) override;

	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void SetLightOn(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Flashlight")
	bool IsLightOn() const { return bLightOn; }

	/** 0-1 */
	UFUNCTION(BlueprintPure, Category = "Flashlight")
	float GetBattery() const { return Battery; }

	/** Add battery charge (0-1). For battery pickups later. */
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void AddBattery(float Amount) { Battery = FMath::Clamp(Battery + Amount, 0.f, 1.f); }

	/** Make the light flicker for a while (e.g. when a creature is near). Amount 0-1. */
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void AddInterference(float Amount, float Duration);

	/** Is this world point inside the lit beam (on, in range, in the cone, not behind a wall)? */
	bool IsPointLit(const FVector& Point, const AActor* IgnoreActor = nullptr) const;

	/** Body-mounted lights draw their battery bottom-left, hand-held ones bottom-right. */
	void SetBodyMounted(bool bInBodyMounted) { bBodyMounted = bInBodyMounted; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> Light;

	// ---------------------------------------------------------------- Light
	/** Brightness in candelas. Tuned for night scenes (a real ~300 lumen torch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float LightIntensity = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	FLinearColor LightColor = FLinearColor(1.f, 0.95f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float Range = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float InnerConeAngle = 7.f;   // bright hotspot

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float OuterConeAngle = 28.f;  // soft spill around it

	// ---------------------------------------------------------------- Battery
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Battery")
	bool bUsesBattery = true;

	/** Seconds of light from a full battery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Battery")
	float BatteryLifeSeconds = 600.f;

	/** Below this (0-1) the light starts dimming and flickering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Battery")
	float LowBatteryThreshold = 0.15f;

	// ---------------------------------------------------------------- Paranormal
	/** Flicker automatically when the EMF level here is high. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Paranormal")
	bool bReactsToParanormal = true;

	/** EMF level (0-5) at which flickering starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Paranormal")
	float ParanormalFlickerEMF = 3.5f;

	bool bLightOn = false;

private:
	void UpdateLight(float DeltaSeconds);

	float Battery = 1.f;
	float InterferenceAmount = 0.f;
	float InterferenceEndTime = 0.f;
	float NoiseTime = 0.f;
	bool bBodyMounted = false;
};
