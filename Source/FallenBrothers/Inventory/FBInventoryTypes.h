// Fallen Brothers - Inventory types.

#pragma once

#include "CoreMinimal.h"
#include "FBInventoryTypes.generated.h"

/** What kind of item this is. Some systems look for a category (e.g. R uses any Battery). */
UENUM(BlueprintType)
enum class EFBItemCategory : uint8
{
	Misc,
	Key,
	Battery,
	Document,
	Medical,
	Ammo,
	Weapon,     // Iron pipe, crowbar... (melee comes in step 5/8)
	Ritual      // Salt, holy water, herbs... (weakness system comes in step 8)
};

/** One stack of items in the inventory. Also used by pickups to describe what they give. */
USTRUCT(BlueprintType)
struct FFBInventoryItem
{
	GENERATED_BODY()

	/** Unique id. Doors check this for keys, e.g. "Key_Motel12". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemId = TEXT("Battery");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FText DisplayName = NSLOCTEXT("FB", "ItemBattery", "Battery");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EFBItemCategory Category = EFBItemCategory::Battery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	bool bStackable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1", EditCondition = "bStackable"))
	int32 MaxStack = 10;

	bool IsValid() const { return !ItemId.IsNone() && Quantity > 0; }
};
