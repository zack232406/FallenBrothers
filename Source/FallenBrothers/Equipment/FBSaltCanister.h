// Fallen Brothers - Salt canister.
//
// Left click starts / stops pouring a line of salt on the floor in front of you.
// Walk while pouring to draw the line. Each "Salt" item in the inventory = SaltPerItem cm of line.
// Ghosts can't cross the line.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FBEquipmentBase.h"
#include "FBSaltCanister.generated.h"

class AFBSaltLine;
class UFBInventoryComponent;
class UStaticMeshComponent;

UCLASS()
class FALLENBROTHERS_API AFBSaltCanister : public AFBEquipmentBase
{
	GENERATED_BODY()

public:
	AFBSaltCanister();

	virtual void Tick(float DeltaSeconds) override;
	virtual void UseEquipment() override;
	virtual void OnUnequipped() override;
	virtual void DrawEquipmentHUD(AFBHUD* HUD, float UIScale) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Canister;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt")
	FName SaltItemId = TEXT("Salt");

	/** Centimetres of line per salt item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt")
	float SaltPerItem = 400.f;

	/** How far in front of you the salt lands (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt")
	float PourDistance = 90.f;

	/** A new point is added every this many cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt")
	float PointSpacing = 20.f;

private:
	void StopPouring();
	bool FindFloorPoint(FVector& OutPoint) const;
	UFBInventoryComponent* GetInventory() const;

	bool bPouring = false;
	float RemainingInCurrentItem = 0.f; // cm left from the salt item being used
	FVector LastPoint = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<AFBSaltLine> CurrentLine;
};
