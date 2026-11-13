// Fallen Brothers - Interactable door.
//
// Drop it into a level (Place Actors / Content Browser > C++ Classes). Uses the engine cube as a
// placeholder door panel, so no assets are needed. Swings away from the player, can be locked.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "FBDoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class FALLENBROTHERS_API AFBDoor : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBDoor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetOpen(bool bOpen, AActor* Opener = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetLocked(bool bNewLocked) { bLocked = bNewLocked; }

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsLocked() const { return bLocked; }

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override;
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Rotates around this point (the hinge edge of the door). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	// ---------------------------------------------------------------- Tuning
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenAngle = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenSpeed = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bStartOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FText LockedMessage = NSLOCTEXT("FB", "DoorLocked", "It's locked.");

	/** Item id of the key that unlocks this door (e.g. "Key_Motel12"). Empty = no key opens it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName RequiredKeyId;

	/** Remove the key from the inventory when used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bConsumeKey = false;

private:
	/** True if InteractingActor carries the key for this door. */
	bool HasKey(const AActor* InteractingActor) const;

	float CurrentYaw = 0.f;
	float TargetYaw = 0.f;
	bool bIsOpen = false;
};
