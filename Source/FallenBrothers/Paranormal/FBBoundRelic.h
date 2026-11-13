// Fallen Brothers - Bound relic: the thing that still ties the dead to this world.
//
// Salt and fire on the bones is not always enough: if something of his is still above ground, he stays.
// For Elias Crane it is the foreman's key ring he was drowned with - hung back up in the mill like a trophy.
// It only appears in the case's evening part, in the heart of his haunt: you go into the dark for it, with the
// EMF reader screaming, and he does not want you to have it. Taking it enrages him. It burns with the bones.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "FBBoundRelic.generated.h"

class USphereComponent;
class UPointLightComponent;
class UFBParanormalSourceComponent;

UCLASS()
class FALLENBROTHERS_API AFBBoundRelic : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBBoundRelic();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relic")
	FText RelicName = NSLOCTEXT("FB", "RelicKeys", "Crane's key ring");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relic")
	FName GrantsClue = TEXT("Clue_KeyRing");

	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override;
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Touch;

	/** A faint cold glint so it can be found in the dark once you are close. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glint;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UFBParanormalSourceComponent> EMF;

private:
	float Time = 0.f;
};
