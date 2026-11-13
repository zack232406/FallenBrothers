// Fallen Brothers - Interactable door.

#include "Interaction/FBDoor.h"

#include "UI/FBHUD.h"
#include "Save/FBSaveSubsystem.h"
#include "Inventory/FBInventoryComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AFBDoor::AFBDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Only ticks while swinging.

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	// The navmesh ignores doors (creatures open them), so doorways stay walkable.
	DoorMesh->SetCanEverAffectNavigation(false);

	// Placeholder: engine cube (100x100x100) scaled to a 10 x 100 x 210 cm door panel.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DoorMesh->SetStaticMesh(CubeMesh.Object);
	}
	DoorMesh->SetRelativeScale3D(FVector(0.1f, 1.0f, 2.1f));
	// Hinge is at Y = 0, the panel extends to +Y. Actor origin is on the floor.
	DoorMesh->SetRelativeLocation(FVector(0.f, 50.f, 105.f));
}

void AFBDoor::BeginPlay()
{
	Super::BeginPlay();

	if (bStartOpen)
	{
		bIsOpen = true;
		CurrentYaw = TargetYaw = OpenAngle;
		Hinge->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));
	}
}

void AFBDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, OpenSpeed);
	Hinge->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));

	if (FMath::IsNearlyEqual(CurrentYaw, TargetYaw, 0.1f))
	{
		CurrentYaw = TargetYaw;
		Hinge->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));
		SetActorTickEnabled(false);
	}
}

void AFBDoor::SetOpen(bool bOpen, AActor* Opener)
{
	bIsOpen = bOpen;
	// An open door never body-blocks whoever walks through (the swinging panel used to wedge the player in the
	// doorway); it still blocks the look-trace so it can be closed again.
	DoorMesh->SetCollisionResponseToChannel(ECC_Pawn, bOpen ? ECR_Ignore : ECR_Block);

	if (bOpen)
	{
		// Swing away from whoever opens it. Positive yaw moves the panel towards -X (behind the door).
		float Direction = 1.f;
		if (Opener)
		{
			const FVector LocalOpener = GetActorTransform().InverseTransformPosition(Opener->GetActorLocation());
			Direction = (LocalOpener.X >= 0.f) ? 1.f : -1.f;
		}
		TargetYaw = OpenAngle * Direction;
	}
	else
	{
		TargetYaw = 0.f;
	}

	SetActorTickEnabled(true);
}

bool AFBDoor::CanInteract_Implementation(AActor* InteractingActor) const
{
	return true; // Locked doors are still "usable" so the player gets the locked message.
}

FText AFBDoor::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	if (bLocked)
	{
		return HasKey(InteractingActor) ? NSLOCTEXT("FB", "DoorPromptUnlock", "Unlock door")
		                                : NSLOCTEXT("FB", "DoorPromptLocked", "Locked door");
	}
	return bIsOpen ? NSLOCTEXT("FB", "DoorPromptClose", "Close door")
	               : NSLOCTEXT("FB", "DoorPromptOpen", "Open door");
}

bool AFBDoor::HasKey(const AActor* InteractingActor) const
{
	if (RequiredKeyId.IsNone() || !InteractingActor)
	{
		return false;
	}
	const UFBInventoryComponent* Inventory = InteractingActor->FindComponentByClass<UFBInventoryComponent>();
	return Inventory && Inventory->HasItem(RequiredKeyId);
}

void AFBDoor::Interact_Implementation(AActor* InteractingActor)
{
	if (bLocked)
	{
		if (!HasKey(InteractingActor))
		{
			AFBHUD::ShowNotificationFor(InteractingActor, LockedMessage);
			return;
		}

		UFBInventoryComponent* Inventory = InteractingActor->FindComponentByClass<UFBInventoryComponent>();
		const FText KeyName = Inventory->GetItemDisplayName(RequiredKeyId);
		if (bConsumeKey)
		{
			Inventory->RemoveItem(RequiredKeyId, 1);
		}
		bLocked = false;
		UFBSaveSubsystem::NotifyDoorUnlocked(this);
		AFBHUD::ShowNotificationFor(InteractingActor, FText::Format(NSLOCTEXT("FB", "DoorUnlocked", "Unlocked with {0}."), KeyName));
	}

	SetOpen(!bIsOpen, InteractingActor);
}
