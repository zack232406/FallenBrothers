// Fallen Brothers - Paranormal hotspot.

#include "Paranormal/FBParanormalHotspot.h"

#include "Paranormal/FBParanormalSourceComponent.h"
#include "Components/SphereComponent.h"

AFBParanormalHotspot::AFBParanormalHotspot()
{
	PrimaryActorTick.bCanEverTick = false;

	Source = CreateDefaultSubobject<UFBParanormalSourceComponent>(TEXT("Source"));
	SetRootComponent(Source);

	RadiusPreview = CreateDefaultSubobject<USphereComponent>(TEXT("RadiusPreview"));
	RadiusPreview->SetupAttachment(Source);
	RadiusPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RadiusPreview->SetHiddenInGame(true);
	RadiusPreview->ShapeColor = FColor(170, 60, 255);
	RadiusPreview->SetSphereRadius(Source->Radius);
}

void AFBParanormalHotspot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Keep the preview sphere in sync when you change Radius in the Details panel.
	RadiusPreview->SetSphereRadius(Source->Radius);
}
