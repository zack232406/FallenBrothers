// Fallen Brothers - Evidence item.
//
// A clue the player can examine and collect. For now it shows a message and disappears.
// In Step 5 (case journal) collecting it will add it to the journal.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "FBEvidenceItem.generated.h"

class UStaticMeshComponent;
class AFBEvidenceItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEvidenceCollected, AFBEvidenceItem*, Evidence, AActor*, Collector);

UCLASS()
class FALLENBROTHERS_API AFBEvidenceItem : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBEvidenceItem();

	virtual void OnConstruction(const FTransform& Transform) override;

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override;
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	UFUNCTION(BlueprintPure, Category = "Evidence")
	FName GetEvidenceId() const { return EvidenceId; }

	UPROPERTY(BlueprintAssignable, Category = "Evidence")
	FOnEvidenceCollected OnCollected;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Unique id, e.g. "Case01_TornDiary". The journal will use this later. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	FName EvidenceId = TEXT("Evidence_Unnamed");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	FText DisplayName = NSLOCTEXT("FB", "EvidenceDefaultName", "Strange object");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence", meta = (MultiLine = true))
	FText Description;

	/** Clue ids this gives (unlock computer searches / theories), e.g. "Clue_Crane". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	TArray<FName> GrantsClues;

	/** A real model from /Game/FallenBrothers/Props/<ModelName> (Poly Haven / Sketchfab) instead of the grey cube. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	FName ModelName;

	/** The model's longest side (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	float ModelSize = 25.f;

	/** Remove the object from the world after collecting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	bool bDestroyOnCollect = true;

private:
	bool bCollected = false;
};
