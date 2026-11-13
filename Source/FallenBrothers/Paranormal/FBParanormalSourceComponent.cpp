// Fallen Brothers - Paranormal source component.

#include "Paranormal/FBParanormalSourceComponent.h"

#include "Paranormal/FBParanormalSubsystem.h"
#include "Engine/World.h"

UFBParanormalSourceComponent::UFBParanormalSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFBParanormalSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UFBParanormalSubsystem* Subsystem = GetWorld()->GetSubsystem<UFBParanormalSubsystem>())
	{
		Subsystem->RegisterSource(this);
	}
}

void UFBParanormalSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UFBParanormalSubsystem* Subsystem = World->GetSubsystem<UFBParanormalSubsystem>())
		{
			Subsystem->UnregisterSource(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

float UFBParanormalSourceComponent::GetEMFAt(const FVector& WorldLocation) const
{
	if (!bActive || Radius <= 0.f)
	{
		return 0.f;
	}

	const float Distance = FVector::Dist(WorldLocation, GetComponentLocation());
	if (Distance >= Radius)
	{
		return 0.f;
	}

	// Rises quickly near the source, gentle at the edge.
	const float T = 1.f - Distance / Radius;
	return EMFStrength * FMath::Sqrt(T);
}
