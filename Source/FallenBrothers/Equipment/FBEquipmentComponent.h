// Fallen Brothers - Equipment component.
//
// Lives on the player. Spawns the chest flashlight + the hand-held tools,
// and handles switching between them.
//   T          = chest flashlight on/off
//   1-5        = EMF, UV light, salt shotgun, salt, silver knife (press again to put it away)
//   Mouse wheel= cycle tools
//   Left mouse = use the tool in hand

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FBEquipmentComponent.generated.h"

class AFBEquipmentBase;
class AFBFlashlight;
class UFBInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentChanged, AFBEquipmentBase*, NewEquipment);

UCLASS(ClassGroup = (FallenBrothers), meta = (BlueprintSpawnableComponent))
class FALLENBROTHERS_API UFBEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFBEquipmentComponent();

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void ToggleFlashlight();

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UseActiveEquipment();

	/** Select slot (0-based). Selecting the active slot again puts it away. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void CycleEquipment(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void Holster();

	/**
	 * R: put a fresh battery from the inventory into the light in hand (UV),
	 * or into the chest flashlight if nothing battery-powered is in hand.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool TryReplaceBattery(UFBInventoryComponent* Inventory);

	/** R: reload the item in hand (shotgun), otherwise replace a battery. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool TryReload(UFBInventoryComponent* Inventory);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	AFBEquipmentBase* GetActiveEquipment() const;

	/** Gives Chris a new piece of equipment (e.g. from the car trunk). False if he already has one of that kind. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool AddEquipment(TSubclassOf<AFBEquipmentBase> Class);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	AFBFlashlight* GetFlashlight() const { return Flashlight; }

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquipmentChanged OnEquipmentChanged;

	/** Chest-mounted light, always available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<AFBFlashlight> FlashlightClass;

	/** Hand-held tools, in slot order (slot 1 = first). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TArray<TSubclassOf<AFBEquipmentBase>> StartingEquipment;

	/** Which slot is in hand at the start (-1 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	int32 StartingSlot = -1;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AFBEquipmentBase* SpawnEquipment(TSubclassOf<AFBEquipmentBase> Class, USceneComponent* Holder);
	void SetActiveSlot(int32 NewIndex);

	UPROPERTY(Transient)
	TObjectPtr<AFBFlashlight> Flashlight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AFBEquipmentBase>> Items;

	int32 ActiveIndex = INDEX_NONE;
};
