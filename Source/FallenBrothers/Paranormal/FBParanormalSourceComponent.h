// Fallen Brothers - Paranormal source component.
//
// Add this to anything that should make the EMF detector react: a haunted object,
// a hotspot in a room, and later the creatures themselves.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FBParanormalSourceComponent.generated.h"

UCLASS(ClassGroup = (FallenBrothers), meta = (BlueprintSpawnableComponent))
class FALLENBROTHERS_API UFBParanormalSourceComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFBParanormalSourceComponent();

	/** EMF level (0-5) produced at this point, given a location in the world. */
	float GetEMFAt(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Paranormal")
	void SetSourceActive(bool bNewActive) { bActive = bNewActive; }

	/** Peak EMF level right at the source (0-5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paranormal", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float EMFStrength = 5.f;

	/** Distance (cm) at which the reading drops to 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paranormal", meta = (ClampMin = "10.0"))
	float Radius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paranormal")
	bool bActive = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
