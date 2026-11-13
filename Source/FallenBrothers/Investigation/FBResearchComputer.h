// Fallen Brothers - Research computer.
//
// A real antique desk with an old computer, a lamp and a chair (falls back to placeholder shapes), with a faint screen glow.
// Look at it and press F to open the town archive terminal.
// The searchable content comes from the AFBCaseFile in the level.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "FBResearchComputer.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class FALLENBROTHERS_API AFBResearchComputer : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBResearchComputer();

	virtual void OnConstruction(const FTransform& Transform) override;

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return true; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	/** Title shown at the top of the terminal. */
	UFUNCTION(BlueprintPure, Category = "Computer")
	FText GetTerminalName() const { return TerminalName; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Computer")
	FText TerminalName = NSLOCTEXT("FB", "TerminalDefault", "PUBLIC LIBRARY - TOWN ARCHIVE TERMINAL");

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Desk;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Monitor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> ScreenGlow;

	/** Extra turn for the real props, if a model faces the wrong way (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Computer")
	float ComputerYaw = 0.f;

	/** Builds the real desk / computer / lamp / chair from the imported props (Props/prop_*). */
	void Dress();


};
