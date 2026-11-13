// Fallen Brothers - Paranormal world subsystem.
//
// One per level. Keeps track of every paranormal source (EMF emitters, later creatures)
// and every UV trace, so equipment can query them cheaply without searching the world.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FBParanormalSubsystem.generated.h"

class UFBParanormalSourceComponent;
class AFBUVTrace;
class AFBSaltLine;

UCLASS()
class FALLENBROTHERS_API UFBParanormalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterSource(UFBParanormalSourceComponent* Source);
	void UnregisterSource(UFBParanormalSourceComponent* Source);

	void RegisterUVTrace(AFBUVTrace* Trace);
	void UnregisterUVTrace(AFBUVTrace* Trace);

	void RegisterSaltLine(AFBSaltLine* Line);
	void UnregisterSaltLine(AFBSaltLine* Line);

	/** True if moving from A to B would cross any salt line (ghosts can't). */
	bool IsBlockedBySalt(const FVector& A, const FVector& B) const;

	/** EMF reading (0-5) at a world location: strongest nearby source wins. */
	UFUNCTION(BlueprintPure, Category = "Paranormal")
	float GetEMFLevelAt(const FVector& Location) const;

	const TArray<TWeakObjectPtr<AFBUVTrace>>& GetUVTraces() const { return UVTraces; }

private:
	TArray<TWeakObjectPtr<UFBParanormalSourceComponent>> Sources;
	TArray<TWeakObjectPtr<AFBUVTrace>> UVTraces;
	TArray<TWeakObjectPtr<AFBSaltLine>> SaltLines;
};
