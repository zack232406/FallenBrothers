// Fallen Brothers - Street lamp.
//
// Sodium-orange light hanging over the road (the arm points along the forward arrow).
// Flickers when something paranormal is close (EMF), so lamps warn you before the EMF reader does.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBStreetLamp.generated.h"

class UStaticMeshComponent;
class USpotLightComponent;

UCLASS()
class FALLENBROTHERS_API AFBStreetLamp : public AActor
{
	GENERATED_BODY()

public:
	AFBStreetLamp();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bOn = true;

	/** Candelas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float Intensity = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	FLinearColor LightColor = FLinearColor(1.f, 0.62f, 0.3f);

	/** Old bulb: flickers now and then even without ghosts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bFaulty = false;

	/** Switches itself on at dusk and off at dawn (follows the atmosphere's day / night). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bAutoNight = true;

	/** Grey pole / arm / head. Off when a real lamp model stands in its place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bShowBody = true;

	/** Height of the bulb (cm) and how far it hangs out over the street. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float LightHeight = 465.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float ArmReach = 150.f;

	/** EMF level (0-5) where the lamp starts to flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float EMFFlickerThreshold = 1.5f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Pole;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Arm;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USpotLightComponent> Light;

private:
	void ApplyLook();

	float FlickerTimer = 0.f;
	float NextFaultTime = 0.f;
	float FaultEndTime = 0.f;
	float NightCheckTimer = 0.f;
	bool bLitByNight = true;
};
