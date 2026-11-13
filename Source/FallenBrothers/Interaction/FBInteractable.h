// Fallen Brothers - Interactable interface.
//
// Any actor that the player can "use" (door, evidence, NPC, switch...) implements this.
// The player's UFBInteractionComponent finds it by looking at it and calls these functions.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FBInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UFBInteractable : public UInterface
{
	GENERATED_BODY()
};

class FALLENBROTHERS_API IFBInteractable
{
	GENERATED_BODY()

public:
	/** Can this be used right now? (false = prompt shown greyed out, interaction ignored) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* InteractingActor) const;

	/** Prompt text, e.g. "Open door", "Examine bloody note". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText(AActor* InteractingActor) const;

	/** Called when the player presses the interact key while looking at this. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* InteractingActor);
};
