// Fallen Brothers - Paranormal hotspot.
//
// A placeable actor that emits EMF. Drag it into a room where "something" is.
// The purple sphere is only visible in the editor and shows the EMF radius.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBParanormalHotspot.generated.h"

class UFBParanormalSourceComponent;
class USphereComponent;

UCLASS()
class FALLENBROTHERS_API AFBParanormalHotspot : public AActor
{
	GENERATED_BODY()

public:
	AFBParanormalHotspot();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBParanormalSourceComponent> Source;

	/** Editor-only visual of the radius. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> RadiusPreview;
};
