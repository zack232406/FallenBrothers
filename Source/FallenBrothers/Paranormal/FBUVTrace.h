// Fallen Brothers - UV trace.
//
// A hidden mark (handprint, footprint, symbol) that only appears when lit by the UV light.
// Placeholder visual: a flat glowing plane. Later we'll swap it for a proper decal.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBUVTrace.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class FALLENBROTHERS_API AFBUVTrace : public AActor
{
	GENERATED_BODY()

public:
	AFBUVTrace();

	virtual void Tick(float DeltaSeconds) override;

	/** Called by the UV light every frame it shines on this trace. Strength 0-1. */
	void NotifyLitByUV(float Strength);

	/** Point the UV light aims its visibility check at. */
	FVector GetRevealPoint() const { return GetActorLocation(); }

	UFUNCTION(BlueprintPure, Category = "UV Trace")
	bool HasBeenDiscovered() const { return bDiscovered; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Glow colour when fully revealed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UV Trace")
	FLinearColor GlowColor = FLinearColor(0.55f, 1.0f, 0.35f);

	/** How fast it appears / disappears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UV Trace")
	float FadeInSpeed = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UV Trace")
	float FadeOutSpeed = 2.f;

	/** Short description for later (case journal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UV Trace")
	FText Description = NSLOCTEXT("FB", "UVTraceDefault", "Glowing handprint");

	/** Clue ids this gives when first discovered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UV Trace")
	TArray<FName> GrantsClues;

private:
	void ApplyReveal();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	float Reveal = 0.f;        // Current visibility 0-1
	float LitThisFrame = 0.f;  // Highest UV strength received this frame
	bool bDiscovered = false;
};
