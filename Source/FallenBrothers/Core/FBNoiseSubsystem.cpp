// Fallen Brothers - Noise subsystem.

#include "Core/FBNoiseSubsystem.h"

#include "Engine/World.h"

void UFBNoiseSubsystem::ReportNoise(const UObject* WorldContext, const FVector& Location, float Loudness, AActor* Instigator)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || Loudness <= 0.f)
	{
		return;
	}
	if (UFBNoiseSubsystem* Subsystem = World->GetSubsystem<UFBNoiseSubsystem>())
	{
		Subsystem->OnNoise.Broadcast(Location, Loudness, Instigator);
	}
}
