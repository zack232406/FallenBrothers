// Fallen Brothers - Equipment base class.
//
// Anything the player carries: flashlight, EMF reader, UV light, later weapons.
// The equipment follows the bodycam camera with a slight lag (feels hand-held),
// and can draw its own little HUD element (battery, EMF LEDs...).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBEquipmentBase.generated.h"

class AFBHUD;
class UFont;

UCLASS(Abstract)
class FALLENBROTHERS_API AFBEquipmentBase : public AActor
{
	GENERATED_BODY()

public:
	AFBEquipmentBase();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** The scene component (camera) this equipment follows. */
	void SetHolder(USceneComponent* InHolder);

	virtual void OnEquipped();
	virtual void OnUnequipped();

	/** Primary action (left mouse): toggle a light, switch a detector on/off, fire later... */
	virtual void UseEquipment() {}

	/** R key. Return true if this equipment handled it (e.g. shotgun reload). */
	virtual bool Reload(class UFBInventoryComponent* Inventory) { return false; }

	/** Draw this equipment's HUD. Default: its name in the bottom-right corner. */
	virtual void DrawEquipmentHUD(AFBHUD* HUD, float UIScale);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsEquipped() const { return bEquipped; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FText GetDisplayName() const { return DisplayName; }

protected:
	/** Helper: right-aligned text in the bottom-right HUD area. Returns the text height. */
	float DrawRightAlignedText(AFBHUD* HUD, const FString& Text, float RightX, float Y, const FLinearColor& Color, float Scale) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FText DisplayName = NSLOCTEXT("FB", "EquipmentDefault", "Equipment");

	/** Position relative to the camera (X forward, Y right, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FVector HoldOffset = FVector(20.f, 15.f, -15.f);

	/** How fast the aim catches up with the camera. Lower = more hand-held lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	float AimLagSpeed = 14.f;

	/** Real model in /Game/FallenBrothers/Items/<ModelName> (downloaded). Empty or missing = keep the grey shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Model")
	FString ModelName;

	/** Size of the real model's longest side (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Model")
	float ModelLength = 25.f;

	/** Extra turn for the real model (on top of "longest side forward"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Model")
	FRotator ModelRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Model")
	FVector ModelOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Holder;

	bool bEquipped = false;

private:
	FRotator SmoothedRotation = FRotator::ZeroRotator;
	bool bHasSmoothedRotation = false;
};
