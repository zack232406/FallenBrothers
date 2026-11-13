// Fallen Brothers - Chandelier trap.
//
// A heavy iron chandelier hanging from a chain. It falls when:
//   - the player uses the rope tie on the wall (look at it + F), or
//   - something thrown hits the chain hard enough.
// Falling iron disperses ghosts it passes through.
//
// Place the actor's origin at the CEILING. Move the "RopeTie" component to a wall the player can reach.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/FBDamageTypes.h"
#include "Interaction/FBInteractable.h"
#include "FBChandelier.generated.h"

class UStaticMeshComponent;

UCLASS()
class FALLENBROTHERS_API AFBChandelier : public AActor, public IFBInteractable, public IFBImpactSource
{
	GENERATED_BODY()

public:
	AFBChandelier();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Chandelier")
	void Release();

	UFUNCTION(BlueprintPure, Category = "Chandelier")
	bool IsReleased() const { return bReleased; }

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return !bReleased; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override { Release(); }

	// IFBImpactSource
	virtual EFBDamageKind GetImpactKind() const override { return EFBDamageKind::Iron; }
	virtual float GetImpactMassKg() const override { return MassKg; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnChainHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Ceiling anchor point. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Anchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Chain;

	/** The heavy part that falls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Where the rope is tied on the wall. Move it in the viewport. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RopeTie;

	/** Distance from the ceiling to the top of the chandelier (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chandelier", meta = (ClampMin = "10.0"))
	float ChainLength = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chandelier", meta = (ClampMin = "1.0"))
	float MassKg = 80.f;

	/** Speed (cm/s) a thrown object must hit the chain with to snap it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chandelier")
	float ChainBreakSpeed = 350.f;

private:
	bool bReleased = false;
};
