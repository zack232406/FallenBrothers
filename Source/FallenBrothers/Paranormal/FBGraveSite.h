// Fallen Brothers - Grave site (salt and burn).
//
// The final step of a ghost case. Look at it + F, three times:
//   1) Dig up the grave   -> the ghost gets furious and attacks (Enrage)
//   2) Salt the bones     -> needs a "Salt" item
//   3) Burn the bones     -> needs a "Lighter" item -> ghost banished, CASE SOLVED
// You must have discovered where the grave is (RequiredClue) before you can dig.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "FBGraveSite.generated.h"

class AFBGhost;
class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EFBGraveState : uint8
{
	Undug,
	Dug,
	Salted,
	Burning,
	Done
};

UCLASS()
class FALLENBROTHERS_API AFBGraveSite : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBGraveSite();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override;
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Hole;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Skull;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Bones;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Flames;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> FireLight;

	/** The ghost bound to these remains. Empty = the first ghost in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	TObjectPtr<AFBGhost> LinkedGhost;

	/** Player must know this clue to recognise the grave. Empty = always. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	FName RequiredClue = TEXT("Clue_CraneGrave");

	/** Item needed to dig (e.g. "Shovel"). Empty = no tool needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	FName DigItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	FName SaltItemId = TEXT("Salt");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	FName LighterItemId = TEXT("Lighter");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	FText CaseTitle = NSLOCTEXT("FB", "DemoCaseTitle", "The Weeping Mill");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	float BurnDuration = 30.f;

	/** Shovelfuls to open the grave (one per press; he comes for you while you dig). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	int32 DigSteps = 7;

	/** The bones only burn once you hold this clue (the bound relic). None = no relic needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grave")
	FName RelicClue = TEXT("Clue_KeyRing");

private:
	AFBGhost* GetGhost() const;
	bool PlayerKnowsGrave(const AActor* Player) const;
	void SetCompColor(UStaticMeshComponent* Comp, const FLinearColor& Color) const;

	EFBGraveState State = EFBGraveState::Undug;
	float BurnStartTime = 0.f;
	int32 DigDone = 0;
	float NextDigTime = 0.f;
	float FlickerTime = 0.f;
};
