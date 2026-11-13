// Fallen Brothers - Inventory component.
//
// Lives on the player. Holds stacks of items (keys, batteries, salt, iron...).
// Tab shows the list on the HUD.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FBInventoryTypes.h"
#include "FBInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup = (FallenBrothers), meta = (BlueprintSpawnableComponent))
class FALLENBROTHERS_API UFBInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFBInventoryComponent();

	/** Adds as much as fits. Returns how many were actually added. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(const FFBInventoryItem& Item);

	/** Removes Count of ItemId. Fails (and removes nothing) if there aren't enough. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(FName ItemId, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(FName ItemId, int32 Count = 1) const { return GetItemCount(ItemId) >= Count; }

	/** First item of a category (e.g. any Battery). Returns false if none. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool FindFirstOfCategory(EFBItemCategory Category, FFBInventoryItem& OutItem) const;

	/** Display name for an id (for messages like "Unlocked with Rusty key"). */
	FText GetItemDisplayName(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FFBInventoryItem>& GetItems() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMaxSlots() const { return MaxSlots; }

	/** Replace everything (used when loading a save). */
	void SetItems(const TArray<FFBInventoryItem>& NewItems) { Items = NewItems; OnInventoryChanged.Broadcast(); }

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	/** How many different stacks can be carried. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "1"))
	int32 MaxSlots = 12;

	/** Items the player starts with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FFBInventoryItem> StartingItems;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<FFBInventoryItem> Items;
};
