// Fallen Brothers - Inventory component.

#include "Inventory/FBInventoryComponent.h"

UFBInventoryComponent::UFBInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Start with one spare battery.
	FFBInventoryItem SpareBattery;
	SpareBattery.Description = NSLOCTEXT("FB", "BatteryDesc", "Spare battery for the flashlight or UV light. Press R to replace.");
	StartingItems.Add(SpareBattery);

	// Hunter's kit.
	FFBInventoryItem Lighter;
	Lighter.ItemId = TEXT("Lighter");
	Lighter.DisplayName = NSLOCTEXT("FB", "ItemLighter", "Zippo lighter");
	Lighter.Description = NSLOCTEXT("FB", "LighterDesc", "Dad's old lighter. Still works.");
	Lighter.Category = EFBItemCategory::Misc;
	Lighter.bStackable = false;
	StartingItems.Add(Lighter);

	FFBInventoryItem Shells;
	Shells.ItemId = TEXT("SaltShell");
	Shells.DisplayName = NSLOCTEXT("FB", "ItemSaltShell", "Rock salt shells");
	Shells.Description = NSLOCTEXT("FB", "SaltShellDesc", "Shotgun shells packed with rock salt. Won't kill a person. Scatters a spirit.");
	Shells.Category = EFBItemCategory::Ammo;
	Shells.Quantity = 6;
	Shells.MaxStack = 24;
	StartingItems.Add(Shells);

	FFBInventoryItem Salt;
	Salt.ItemId = TEXT("Salt");
	Salt.DisplayName = NSLOCTEXT("FB", "ItemSalt", "Salt");
	Salt.Description = NSLOCTEXT("FB", "SaltDesc", "Pour a line (slot 4) - spirits can't cross it. Also for salting remains.");
	Salt.Category = EFBItemCategory::Ritual;
	Salt.Quantity = 3;
	StartingItems.Add(Salt);

	// The fake credentials every hunter carries: gets them into motels, morgues and crime scenes.
	FFBInventoryItem Badge;
	Badge.ItemId = TEXT("FBIBadge");
	Badge.DisplayName = NSLOCTEXT("FB", "ItemFBIBadge", "FBI badge (fake)");
	Badge.Description = NSLOCTEXT("FB", "FBIBadgeDesc", "Special Agent C. Harlan. Laminated at a copy shop in Sioux Falls. Looks real if nobody looks too hard.");
	Badge.Category = EFBItemCategory::Document;
	Badge.bStackable = false;
	StartingItems.Add(Badge);
}

void UFBInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	for (const FFBInventoryItem& Item : StartingItems)
	{
		AddItem(Item);
	}
}

int32 UFBInventoryComponent::AddItem(const FFBInventoryItem& Item)
{
	if (!Item.IsValid())
	{
		return 0;
	}

	int32 Remaining = Item.Quantity;
	const int32 StackLimit = Item.bStackable ? FMath::Max(1, Item.MaxStack) : 1;

	// 1) Fill existing stacks of the same item.
	if (Item.bStackable)
	{
		for (FFBInventoryItem& Existing : Items)
		{
			if (Remaining <= 0)
			{
				break;
			}
			if (Existing.ItemId == Item.ItemId && Existing.Quantity < StackLimit)
			{
				const int32 ToAdd = FMath::Min(Remaining, StackLimit - Existing.Quantity);
				Existing.Quantity += ToAdd;
				Remaining -= ToAdd;
			}
		}
	}

	// 2) New stacks while there are free slots.
	while (Remaining > 0 && Items.Num() < MaxSlots)
	{
		FFBInventoryItem NewStack = Item;
		NewStack.Quantity = FMath::Min(Remaining, StackLimit);
		Remaining -= NewStack.Quantity;
		Items.Add(NewStack);
	}

	const int32 Added = Item.Quantity - Remaining;
	if (Added > 0)
	{
		OnInventoryChanged.Broadcast();
	}
	return Added;
}

bool UFBInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	if (Count <= 0 || GetItemCount(ItemId) < Count)
	{
		return false;
	}

	// Take from the last stacks first.
	for (int32 i = Items.Num() - 1; i >= 0 && Count > 0; --i)
	{
		if (Items[i].ItemId != ItemId)
		{
			continue;
		}
		const int32 Take = FMath::Min(Count, Items[i].Quantity);
		Items[i].Quantity -= Take;
		Count -= Take;
		if (Items[i].Quantity <= 0)
		{
			Items.RemoveAt(i);
		}
	}

	OnInventoryChanged.Broadcast();
	return true;
}

int32 UFBInventoryComponent::GetItemCount(FName ItemId) const
{
	int32 Total = 0;
	for (const FFBInventoryItem& Item : Items)
	{
		if (Item.ItemId == ItemId)
		{
			Total += Item.Quantity;
		}
	}
	return Total;
}

bool UFBInventoryComponent::FindFirstOfCategory(EFBItemCategory Category, FFBInventoryItem& OutItem) const
{
	for (const FFBInventoryItem& Item : Items)
	{
		if (Item.Category == Category)
		{
			OutItem = Item;
			return true;
		}
	}
	return false;
}

FText UFBInventoryComponent::GetItemDisplayName(FName ItemId) const
{
	for (const FFBInventoryItem& Item : Items)
	{
		if (Item.ItemId == ItemId)
		{
			return Item.DisplayName;
		}
	}
	return FText::FromName(ItemId);
}
