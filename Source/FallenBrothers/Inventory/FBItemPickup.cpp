// Fallen Brothers - Item pickup.

#include "Inventory/FBItemPickup.h"

#include "Inventory/FBInventoryComponent.h"
#include "UI/FBHUD.h"
#include "Save/FBSaveSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBItemPickup::AFBItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMaterial.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.15f));
}

void AFBItemPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateColor();
}

void AFBItemPickup::UpdateColor()
{
	FLinearColor Color(0.6f, 0.6f, 0.6f);
	switch (Item.Category)
	{
	case EFBItemCategory::Key:      Color = FLinearColor(0.9f, 0.7f, 0.2f);  break; // gold
	case EFBItemCategory::Battery:  Color = FLinearColor(0.2f, 0.5f, 1.0f);  break; // blue
	case EFBItemCategory::Document: Color = FLinearColor(0.95f, 0.95f, 0.9f); break; // paper
	case EFBItemCategory::Medical:  Color = FLinearColor(0.9f, 0.1f, 0.1f);  break; // red
	case EFBItemCategory::Ammo:     Color = FLinearColor(0.4f, 0.35f, 0.2f); break; // brass
	case EFBItemCategory::Weapon:   Color = FLinearColor(0.25f, 0.25f, 0.28f); break; // dark iron
	case EFBItemCategory::Ritual:   Color = FLinearColor(1.f, 1.f, 1.f);     break; // salt white
	default: break;
	}

	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

FText AFBItemPickup::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	if (Item.Quantity > 1)
	{
		return FText::Format(NSLOCTEXT("FB", "PickupPromptN", "Pick up {0} (x{1})"), Item.DisplayName, Item.Quantity);
	}
	return FText::Format(NSLOCTEXT("FB", "PickupPrompt", "Pick up {0}"), Item.DisplayName);
}

void AFBItemPickup::Interact_Implementation(AActor* InteractingActor)
{
	UFBInventoryComponent* Inventory = InteractingActor ? InteractingActor->FindComponentByClass<UFBInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return;
	}

	const int32 Added = Inventory->AddItem(Item);
	if (Added <= 0)
	{
		AFBHUD::ShowNotificationFor(InteractingActor, NSLOCTEXT("FB", "InventoryFull", "Inventory full."));
		return;
	}

	const FText Msg = (Added > 1)
		? FText::Format(NSLOCTEXT("FB", "PickedUpN", "Picked up: {0} (x{1})"), Item.DisplayName, Added)
		: FText::Format(NSLOCTEXT("FB", "PickedUp", "Picked up: {0}"), Item.DisplayName);
	AFBHUD::ShowNotificationFor(InteractingActor, Msg);

	Item.Quantity -= Added;
	if (Item.Quantity <= 0)
	{
		UFBSaveSubsystem::NotifyActorRemoved(this);
		Destroy();
	}
}
