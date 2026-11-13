// Fallen Brothers - Salt canister.

#include "Equipment/FBSaltCanister.h"

#include "Inventory/FBInventoryComponent.h"
#include "Paranormal/FBSaltLine.h"
#include "UI/FBHUD.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBSaltCanister::AFBSaltCanister()
{
	// Real model (if imported): /Game/FallenBrothers/Items/item_salt
	ModelName = TEXT("item_salt");
	ModelLength = 18.f;

	DisplayName = NSLOCTEXT("FB", "SaltCanister", "Salt");
	HoldOffset = FVector(35.f, 18.f, -22.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Placeholder: a 20 cm tall, 8 cm wide canister.
	Canister = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Canister"));
	Canister->SetupAttachment(Root);
	Canister->SetStaticMesh(Cylinder.Object);
	Canister->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.2f));
	Canister->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Canister->SetCastShadow(false);
	if (BasicMaterial.Succeeded())
	{
		Canister->SetMaterial(0, BasicMaterial.Object);
	}
}

void AFBSaltCanister::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInstanceDynamic* MID = Canister->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.3f, 0.7f));
	}
}

UFBInventoryComponent* AFBSaltCanister::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UFBInventoryComponent>() : nullptr;
}

void AFBSaltCanister::UseEquipment()
{
	if (bPouring)
	{
		StopPouring();
		return;
	}

	UFBInventoryComponent* Inventory = GetInventory();
	if (RemainingInCurrentItem <= 0.f && (!Inventory || !Inventory->HasItem(SaltItemId)))
	{
		AFBHUD::ShowNotificationFor(GetOwner(), NSLOCTEXT("FB", "NoSalt", "Out of salt."), 1.5f);
		return;
	}

	FVector Start;
	if (!FindFloorPoint(Start))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	CurrentLine = GetWorld()->SpawnActor<AFBSaltLine>(AFBSaltLine::StaticClass(), FTransform(Start), Params);
	if (CurrentLine)
	{
		CurrentLine->AddPoint(Start);
		LastPoint = Start;
		bPouring = true;
	}
}

void AFBSaltCanister::StopPouring()
{
	bPouring = false;
	CurrentLine = nullptr;
}

void AFBSaltCanister::OnUnequipped()
{
	StopPouring();
	Super::OnUnequipped();
}

bool AFBSaltCanister::FindFloorPoint(FVector& OutPoint) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}

	// Point in front of the player's feet, then straight down to the floor.
	FVector Forward = Pawn->GetActorForwardVector();
	Forward.Z = 0.f;
	Forward.Normalize();
	const FVector Above = Pawn->GetActorLocation() + Forward * PourDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltFloor), false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Above, Above - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}
	return false;
}

void AFBSaltCanister::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bPouring || !CurrentLine)
	{
		return;
	}

	FVector Point;
	if (!FindFloorPoint(Point))
	{
		return;
	}

	// Only add a point once we've moved far enough.
	const float SegmentLen = FVector::Dist2D(Point, LastPoint);
	if (SegmentLen < PointSpacing)
	{
		return;
	}

	// Use up salt.
	if (RemainingInCurrentItem < SegmentLen)
	{
		UFBInventoryComponent* Inventory = GetInventory();
		if (!Inventory || !Inventory->RemoveItem(SaltItemId, 1))
		{
			AFBHUD::ShowNotificationFor(GetOwner(), NSLOCTEXT("FB", "SaltRanOut", "The salt ran out."), 1.5f);
			StopPouring();
			return;
		}
		RemainingInCurrentItem += SaltPerItem;
	}
	RemainingInCurrentItem -= SegmentLen;

	CurrentLine->AddPoint(Point);
	LastPoint = Point;
}

void AFBSaltCanister::DrawEquipmentHUD(AFBHUD* HUD, float UIScale)
{
	if (!HUD || !HUD->HasCanvas())
	{
		return;
	}
	Super::DrawEquipmentHUD(HUD, UIScale);

	const UFBInventoryComponent* Inventory = GetInventory();
	const int32 Count = Inventory ? Inventory->GetItemCount(SaltItemId) : 0;
	const FString Text = bPouring
		? FString::Printf(TEXT("POURING...  %d left"), Count)
		: FString::Printf(TEXT("[LMB] Pour line   %d left"), Count);
	DrawRightAlignedText(HUD, Text, HUD->GetScreenSize().X - 40.f * UIScale, HUD->GetScreenSize().Y - 42.f * UIScale,
	                     FLinearColor(1.f, 1.f, 1.f, 0.65f), 0.9f * UIScale);
}
