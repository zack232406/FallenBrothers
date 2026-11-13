// Fallen Brothers - Water, ice and wind areas. Scale the orange box over the area.
//
//   FBWater - lake / creek / flooded basement. The water surface is the TOP of the box.
//             Wood floats, iron sinks (by real density), the current carries things downstream,
//             wading is slow and loud. (For deep water you can swim in, also add an engine
//             "Physics Volume" with "Water Volume" ticked.)
//   FBIce   - frozen pond / icy road. The ice sheet is the TOP of the box. Characters slide,
//             props glide. "Thin Ice": it cracks under weight and breaks into floating chunks
//             (put an FBWater under it!). A creature lured onto it goes straight through.
//   FBWind  - storm area. Blows along the forward arrow with gusts: real air drag on every
//             physics object (light things fly, heavy crates creep), pushes people in gusts,
//             swings hanging lamps.

#pragma once

#include "CoreMinimal.h"
#include "Combat/FBDamageTypes.h"
#include "Physics/FBPhysicsZone.h"
#include "FBWeatherZones.generated.h"

class ACharacter;
class UMaterialInterface;
class UPhysicalMaterial;
class UStaticMeshComponent;

// ====================================================================== Water

UCLASS()
class FALLENBROTHERS_API AFBWater : public AFBPhysicsZone
{
	GENERATED_BODY()

public:
	AFBWater();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Water")
	float GetSurfaceZ() const;

	/** Current (cm/s, world space). A creek: e.g. (150, 0, 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	FVector Flow = FVector::ZeroVector;

	/** How strongly water slows things down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	float Drag = 2.5f;

	/** Walking speed at waist-deep water (1 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water", meta = (ClampMin = "0.1", ClampMax = "1"))
	float WadeSpeedMultiplier = 0.45f;

	/** Your water material (translucent). Empty = placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	TObjectPtr<UMaterialInterface> WaterMaterial;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Surface;

private:
	float NextSplash = 0.f;
};

// ====================================================================== Ice

UCLASS()
class FALLENBROTHERS_API AFBIce : public AFBPhysicsZone, public IFBDamageable
{
	GENERATED_BODY()

public:
	AFBIce();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;

public:

	// IFBDamageable - shoot the ice
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	/** Breaks a hole in thin ice. */
	UFUNCTION(BlueprintCallable, Category = "Ice")
	void BreakAt(const FVector& Where, float Radius);

	/** Ground friction on ice (normal ground is 8). Lower = more sliding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice", meta = (ClampMin = "0"))
	float Grip = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice")
	float IceThickness = 8.f;

	/** Cracks under weight and breaks into chunks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Thin Ice")
	bool bThinIce = false;

	/** Seconds a man can stand still before it breaks (creatures: 3x faster). Walking keeps you safer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Thin Ice", meta = (EditCondition = "bThinIce"))
	float BreakTime = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Thin Ice", meta = (EditCondition = "bThinIce"))
	float TileSize = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice")
	TObjectPtr<UMaterialInterface> IceMaterial;

private:
	struct FSavedMovement
	{
		float GroundFriction = 8.f;
		float BrakingDeceleration = 2048.f;
		float MaxAcceleration = 2048.f;
	};

	void Restore(ACharacter* C, const FSavedMovement& Saved) const;
	/** Makes the parts. Runs in the editor AND again when the game starts (play copies don't keep them). */
	void Build();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Tiles;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> SlipperyMaterial;

	TMap<TWeakObjectPtr<ACharacter>, FSavedMovement> OnIce;
	TMap<TWeakObjectPtr<ACharacter>, float> Strain;
	float NextCrack = 0.f;
};

// ====================================================================== Wind

UCLASS()
class FALLENBROTHERS_API AFBWind : public AFBPhysicsZone
{
	GENERATED_BODY()

public:
	AFBWind();

	virtual void Tick(float DeltaSeconds) override;

	/** Wind velocity right now (cm/s), gusts included. */
	UFUNCTION(BlueprintPure, Category = "Wind")
	FVector GetWindVelocity() const { return CurrentWind; }

	/** Average wind speed in m/s. Breeze 5, strong 12, storm 20+. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0", ClampMax = "60"))
	float WindSpeed = 12.f;

	/** 0 = steady, 1 = violent gusts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0", ClampMax = "1"))
	float Gustiness = 0.5f;

	/** Gusts per second (roughly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	float GustFrequency = 0.25f;

	/** How hard gusts shove people (cm/s² at full storm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	float CharacterPush = 180.f;

private:
	FVector CurrentWind = FVector::ZeroVector;
	float Time = 0.f;
};
