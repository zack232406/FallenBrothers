// Fallen Brothers - Equipment component.

#include "Equipment/FBEquipmentComponent.h"

#include "Equipment/FBEMFDetector.h"
#include "Equipment/FBEquipmentBase.h"
#include "Equipment/FBFlashlight.h"
#include "Equipment/FBUVLight.h"
#include "Equipment/FBShotgun.h"
#include "Equipment/FBSaltCanister.h"
#include "Equipment/FBSilverKnife.h"
#include "Inventory/FBInventoryComponent.h"
#include "UI/FBHUD.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UFBEquipmentComponent::UFBEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	FlashlightClass = AFBFlashlight::StaticClass();
	// Investigation tools in the jacket; the weapons wait in the car trunk (AFBCarTrunk -> AddEquipment).
	StartingEquipment = { AFBEMFDetector::StaticClass(), AFBUVLight::StaticClass() };
}

void UFBEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	UCameraComponent* Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("FBEquipmentComponent: owner has no camera."));
		return;
	}

	if (FlashlightClass)
	{
		Flashlight = Cast<AFBFlashlight>(SpawnEquipment(FlashlightClass, Camera));
		if (Flashlight)
		{
			Flashlight->SetBodyMounted(true);
			Flashlight->OnEquipped(); // Always mounted, just off.
		}
	}

	for (const TSubclassOf<AFBEquipmentBase>& Class : StartingEquipment)
	{
		if (AFBEquipmentBase* Item = SpawnEquipment(Class, Camera))
		{
			Item->OnUnequipped();
			Items.Add(Item);
		}
	}

	if (Items.IsValidIndex(StartingSlot))
	{
		SetActiveSlot(StartingSlot);
	}
}

void UFBEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Flashlight)
	{
		Flashlight->Destroy();
	}
	for (AFBEquipmentBase* Item : Items)
	{
		if (Item)
		{
			Item->Destroy();
		}
	}
	Items.Empty();

	Super::EndPlay(EndPlayReason);
}

AFBEquipmentBase* UFBEquipmentComponent::SpawnEquipment(TSubclassOf<AFBEquipmentBase> Class, USceneComponent* Holder)
{
	if (!Class)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AFBEquipmentBase* Item = GetWorld()->SpawnActor<AFBEquipmentBase>(Class, Holder->GetComponentTransform(), Params);
	if (Item)
	{
		Item->SetHolder(Holder);
	}
	return Item;
}

bool UFBEquipmentComponent::AddEquipment(TSubclassOf<AFBEquipmentBase> Class)
{
	if (!Class)
	{
		return false;
	}
	for (const AFBEquipmentBase* Item : Items)
	{
		if (Item && Item->GetClass() == Class)
		{
			return false;
		}
	}
	UCameraComponent* Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
	AFBEquipmentBase* Item = Camera ? SpawnEquipment(Class, Camera) : nullptr;
	if (!Item)
	{
		return false;
	}
	Item->OnUnequipped();
	Items.Add(Item);
	OnEquipmentChanged.Broadcast(GetActiveEquipment());
	return true;
}

AFBEquipmentBase* UFBEquipmentComponent::GetActiveEquipment() const
{
	return Items.IsValidIndex(ActiveIndex) ? Items[ActiveIndex].Get() : nullptr;
}

void UFBEquipmentComponent::ToggleFlashlight()
{
	if (Flashlight)
	{
		Flashlight->UseEquipment();
	}
}

void UFBEquipmentComponent::UseActiveEquipment()
{
	if (AFBEquipmentBase* Active = GetActiveEquipment())
	{
		Active->UseEquipment();
	}
}

void UFBEquipmentComponent::EquipSlot(int32 SlotIndex)
{
	if (!Items.IsValidIndex(SlotIndex))
	{
		return;
	}
	SetActiveSlot(SlotIndex == ActiveIndex ? INDEX_NONE : SlotIndex);
}

void UFBEquipmentComponent::CycleEquipment(int32 Direction)
{
	if (Items.Num() == 0 || Direction == 0)
	{
		return;
	}

	// Cycle through: none -> 0 -> 1 -> ... -> none
	const int32 Count = Items.Num() + 1;
	const int32 Current = ActiveIndex + 1; // none = 0
	const int32 Next = (Current + (Direction > 0 ? 1 : -1) + Count) % Count;
	SetActiveSlot(Next - 1);
}

void UFBEquipmentComponent::Holster()
{
	SetActiveSlot(INDEX_NONE);
}

bool UFBEquipmentComponent::TryReload(UFBInventoryComponent* Inventory)
{
	if (AFBEquipmentBase* Active = GetActiveEquipment())
	{
		if (Active->Reload(Inventory))
		{
			return true;
		}
	}
	return TryReplaceBattery(Inventory);
}

bool UFBEquipmentComponent::TryReplaceBattery(UFBInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return false;
	}

	// Light in hand first (UV light), otherwise the chest flashlight.
	AFBFlashlight* Target = Cast<AFBFlashlight>(GetActiveEquipment());
	if (!Target)
	{
		Target = Flashlight;
	}
	if (!Target)
	{
		return false;
	}

	if (Target->GetBattery() >= 0.98f)
	{
		AFBHUD::ShowNotificationFor(GetOwner(), FText::Format(NSLOCTEXT("FB", "BatteryFull", "{0}: battery is full."), Target->GetDisplayName()), 2.f);
		return false;
	}

	FFBInventoryItem Battery;
	if (!Inventory->FindFirstOfCategory(EFBItemCategory::Battery, Battery))
	{
		AFBHUD::ShowNotificationFor(GetOwner(), NSLOCTEXT("FB", "NoBattery", "No spare batteries."), 2.f);
		return false;
	}

	Inventory->RemoveItem(Battery.ItemId, 1);
	Target->AddBattery(1.f);
	AFBHUD::ShowNotificationFor(GetOwner(), FText::Format(NSLOCTEXT("FB", "BatteryReplaced", "{0}: battery replaced."), Target->GetDisplayName()), 2.f);
	return true;
}

void UFBEquipmentComponent::SetActiveSlot(int32 NewIndex)
{
	if (NewIndex == ActiveIndex)
	{
		return;
	}

	if (AFBEquipmentBase* Old = GetActiveEquipment())
	{
		Old->OnUnequipped();
	}

	ActiveIndex = Items.IsValidIndex(NewIndex) ? NewIndex : INDEX_NONE;

	AFBEquipmentBase* New = GetActiveEquipment();
	if (New)
	{
		New->OnEquipped();
	}
	OnEquipmentChanged.Broadcast(New);
}
