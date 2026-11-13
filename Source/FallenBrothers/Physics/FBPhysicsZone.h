// Fallen Brothers - Base for box-shaped physics areas (poltergeist room, water, ice, wind).
//
// Scale the orange box in the viewport (or set Box Extent) to cover the area.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBPhysicsZone.generated.h"

class ACharacter;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Abstract)
class FALLENBROTHERS_API AFBPhysicsZone : public AActor
{
	GENERATED_BODY()

public:
	AFBPhysicsZone();

	/** Is this world point inside the box? */
	bool IsInside(const FVector& World) const;

	UFUNCTION(BlueprintPure, Category = "Zone")
	FVector GetBoxExtent() const;

protected:
	/** Every character (Chris, Zack, NPC-characters, creatures) whose body is inside. */
	void GetCharactersInside(TArray<ACharacter*>& Out) const;

	/** Every physics-simulating piece inside (props, debris, shards...). */
	void GetBodiesInside(TArray<UPrimitiveComponent*>& Out) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Area;
};
