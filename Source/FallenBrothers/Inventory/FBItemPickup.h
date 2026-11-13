// Fallen Brothers - Item pickup.
//
// Place it in the level, fill in "Item" in the Details panel, look at it and press F.
// Default: one Battery. The colour of the placeholder cube depends on the category.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "Inventory/FBInventoryTypes.h"
#include "FBItemPickup.generated.h"

class UStaticMeshComponent;

UCLASS()
class FALLENBROTHERS_API AFBItemPickup : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBItemPickup();

	virtual void OnConstruction(const FTransform& Transform) override;

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return Item.IsValid(); }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** What the player gets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	FFBInventoryItem Item;

private:
	void UpdateColor();
};
