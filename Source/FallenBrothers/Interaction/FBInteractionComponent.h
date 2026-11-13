// Fallen Brothers - Interaction component.
//
// Lives on the player. Every frame it traces from the camera to find what the player
// is looking at. If that actor implements IFBInteractable it becomes the "focused" actor,
// and TryInteract() uses it.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "FBInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionFocusChanged, AActor*, NewFocusedActor);

UCLASS(ClassGroup = (FallenBrothers), meta = (BlueprintSpawnableComponent))
class FALLENBROTHERS_API UFBInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFBInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Interact with the focused actor. Returns true if something was used. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/** Prompt info for the HUD. Returns false if nothing is focused. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool GetFocusedPrompt(FText& OutText, bool& bOutCanInteract) const;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionFocusChanged OnFocusChanged;

	// ---------------------------------------------------------------- Tuning
	/** How far the player can reach (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionDistance = 220.f;

	/** Small sphere instead of a thin line, so small objects are easier to target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float TraceRadius = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Draw the trace in the world (for debugging). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bDrawDebug = false;

private:
	void UpdateFocus();
	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	TWeakObjectPtr<AActor> FocusedActor;
};
