// Fallen Brothers - Salt line.
//
// A line of salt poured on the floor (by the salt canister, or placed in the editor).
// Ghosts can't cross it. It registers itself with the paranormal subsystem.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBSaltLine.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class FALLENBROTHERS_API AFBSaltLine : public AActor
{
	GENERATED_BODY()

public:
	AFBSaltLine();

	/** Extend the line to a new point on the floor. */
	void AddPoint(const FVector& WorldPoint);

	/** Does the 2D movement A -> B cross this line? */
	bool BlocksPath(const FVector& A, const FVector& B) const;

	float GetLength() const { return TotalLength; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Grains;

	/** Points of the line in world space. Can also be filled in the editor for pre-made lines. */
	UPROPERTY(EditAnywhere, Category = "Salt", meta = (MakeEditWidget = true))
	TArray<FVector> Points;

private:
	void AddSegmentVisual(const FVector& A, const FVector& B);

	float TotalLength = 0.f;
};
