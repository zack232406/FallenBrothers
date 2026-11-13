// Fallen Brothers - Grab component.
//
// Lets the player pick up physics objects with a physics handle (Amnesia-style):
//   Right mouse  = grab / drop
//   Left mouse   = swing (melee props like the iron pipe) or throw
//   G            = throw
// Heavy objects lag behind and sway; objects heavier than MaxGrabMassKg can't be lifted.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "FBGrabComponent.generated.h"

class AFBPhysicsProp;

UCLASS(ClassGroup = (FallenBrothers), meta = (BlueprintSpawnableComponent))
class FALLENBROTHERS_API UFBGrabComponent : public UPhysicsHandleComponent
{
	GENERATED_BODY()

public:
	UFBGrabComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Try to pick up what we're looking at. */
	UFUNCTION(BlueprintCallable, Category = "Grab")
	bool TryGrab();

	UFUNCTION(BlueprintCallable, Category = "Grab")
	void Drop();

	UFUNCTION(BlueprintCallable, Category = "Grab")
	void Throw();

	/** Left mouse while holding: swing melee props, throw everything else. */
	UFUNCTION(BlueprintCallable, Category = "Grab")
	void PrimaryAction();

	UFUNCTION(BlueprintPure, Category = "Grab")
	bool IsHolding() const { return GetGrabbedComponent() != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Grab")
	bool IsHoldingMelee() const { return IsHolding() && bHoldingMelee; }

	/** HUD text: "Grab Iron pipe" when looking at something grabbable, or the controls while holding. */
	bool GetPrompt(FText& OutText) const;

	// ---------------------------------------------------------------- Tuning
	/** How far away things can be grabbed (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float GrabReach = 220.f;

	/** Heavier than this can't be lifted (kg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float MaxGrabMassKg = 40.f;

	/** Preferred distance in front of the camera when holding (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float HoldDistance = 110.f;

	/** If the object gets stuck this far from the hands, it's dropped (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float BreakDistance = 140.f;

	/** Throw strength (kg*cm/s). Light things fly fast, heavy things barely move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float ThrowImpulse = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float MaxThrowSpeed = 1400.f;

	/** Seconds for one melee swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Melee")
	float SwingDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Melee")
	float SwingCooldown = 0.25f;

	/** Hand position for melee weapons, relative to the camera (forward, right, up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Melee")
	FVector MeleeHoldOffset = FVector(70.f, 25.f, -15.f);

protected:
	virtual void BeginPlay() override;

private:
	bool GetView(FVector& OutLocation, FRotator& OutRotation) const;
	UPrimitiveComponent* FindGrabbable(FHitResult& OutHit) const;
	void ComputeTarget(FVector& OutLocation, FRotator& OutRotation, float DeltaTime);
	void ApplyHandleStrength(float MassKg, bool bSwinging);

	TWeakObjectPtr<UPrimitiveComponent> FocusComponent;

	float CurrentHoldDistance = 110.f;
	FQuat HoldRotationOffset = FQuat::Identity;
	float HeldMassKg = 1.f;
	bool bHoldingMelee = false;

	float SwingTime = -1.f;     // < 0 = not swinging
	float LastSwingEnd = -100.f;

	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};
