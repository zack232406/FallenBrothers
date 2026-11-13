// Fallen Brothers - The brothers' car trunk.
//
// Stand at the back of the car and press F: the trunk opens (a warm light comes on inside),
// the first time Chris takes the hunting gear out of it: weapons go to the equipment slots, ammo and supplies to the inventory.
// Press F again to close it. Placed by the map generator at the rear bumper of the brothers' black sedan.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "Inventory/FBInventoryTypes.h"
#include "FBCarTrunk.generated.h"

class UBoxComponent;
class UPointLightComponent;
class USoundBase;
class AFBEquipmentBase;

UCLASS()
class FALLENBROTHERS_API AFBCarTrunk : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBCarTrunk();

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return true; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	bool IsOpen() const { return bOpen; }

protected:
	virtual void BeginPlay() override;

	/** Actors tagged with this are the car's body: painted gloss black at start (any imported car model works). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk")
	FName CarTag = TEXT("FBBrothersCar");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk")
	FLinearColor PaintColor = FLinearColor(0.012f, 0.012f, 0.014f);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Box;

	/** Light inside the trunk (on while it's open). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> TrunkLight;

	/** Weapons in the trunk: added to Chris's equipment slots the first time it's opened. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk")
	TArray<TSubclassOf<AFBEquipmentBase>> Weapons;

	/** Ammo and supplies: added to the inventory the first time it's opened. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk")
	TArray<FFBInventoryItem> Supplies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk")
	TObjectPtr<USoundBase> CloseSound;

private:
	bool bOpen = false;
	bool bLooted = false;
};
