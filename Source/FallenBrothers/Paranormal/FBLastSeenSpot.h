// Fallen Brothers - Last-seen spot: where one of the missing was taken.
//
// By day there is nothing to see - only the EMF reader climbs as you get close. Stand on the spot with the
// reader and take the reading (F): the journal gets what happened here. The spots are spread wide over the map
// (the mill, the road, the woods by the cemetery); together they draw a line to where the ghost is buried.
// At night the spots go quiet - by then the thing that made them is awake.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "FBLastSeenSpot.generated.h"

class USphereComponent;
class UFBParanormalSourceComponent;

UCLASS()
class FALLENBROTHERS_API AFBLastSeenSpot : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBLastSeenSpot();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Last Seen")
	FName SpotId = TEXT("Spot_Unnamed");

	/** Who vanished here ("Ruth Ann Decker"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Last Seen")
	FText Person;

	/** What the reading / the ground tells you (journal text). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Last Seen", meta = (MultiLine = true))
	FText Reading;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Last Seen")
	TArray<FName> GrantsClues;

	/** A spot of the night level (where one of the taken lies): stays active after dark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Last Seen")
	bool bNightSpot = false;

	/** Prompt and messages for a night spot ("Search the shallow grave"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Last Seen")
	FText PromptOverride;

	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override;
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	void SetQuiet(bool bQuiet);
	bool IsRead() const { return bRead; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Touch;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UFBParanormalSourceComponent> EMF;

private:
	bool bRead = false;
};
