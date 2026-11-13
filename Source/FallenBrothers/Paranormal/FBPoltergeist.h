// Fallen Brothers - Poltergeist room.
//
// Drop it in a haunted room / house and scale the box over it. While the player is inside
// (and its ghost isn't banished) the room comes alive:
//   - Rattle:   objects shake and hop in place
//   - Levitate: an object rises, slowly turns in the air... then drops - or flies at you
//   - Throw:    something is hurled at the player (it hurts)
//   - Doors:    doors in the room slam shut / swing open
//   - Lights:   street lamps and your flashlight flicker (EMF spike)
//   - Lamps:    hanging lamps (FBSwingingLamp) start swinging
// The first time you walk in, everything happens at once (a scare). Link it to a ghost so it
// gets stronger when the ghost manifests and stops for good when it's banished.

#pragma once

#include "CoreMinimal.h"
#include "Physics/FBPhysicsZone.h"
#include "FBPoltergeist.generated.h"

class AFBGhost;
class UFBParanormalSourceComponent;

UCLASS()
class FALLENBROTHERS_API AFBPoltergeist : public AFBPhysicsZone
{
	GENERATED_BODY()

public:
	AFBPoltergeist();

	virtual void Tick(float DeltaSeconds) override;

	/** Everything at once (also used for the first-entry scare). Callable from story events. */
	UFUNCTION(BlueprintCallable, Category = "Poltergeist")
	void Burst();

	/** Optional: the ghost behind it. Stronger while it's manifested, silent forever once banished. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist")
	TObjectPtr<AFBGhost> LinkedGhost;

	/** 0 = calm, 1 = furious (events come twice as often). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist", meta = (ClampMin = "0", ClampMax = "1"))
	float Activity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist")
	float EventIntervalMin = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist")
	float EventIntervalMax = 12.f;

	/** Only active while the player is inside the box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist")
	bool bOnlyWhenPlayerInside = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist")
	bool bScareOnFirstEnter = true;

	/** Heavier things are never moved (kg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist")
	float MaxLiftMassKg = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Events") bool bRattle = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Events") bool bLevitate = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Events") bool bThrow = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Events") bool bSlamDoors = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Events") bool bFlickerLights = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Events") bool bSwingLamps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Tuning")
	float LevitateHeight = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Tuning")
	float LevitateTime = 3.5f;

	/** Chance a levitating object is thrown at you instead of dropped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Tuning", meta = (ClampMin = "0", ClampMax = "1"))
	float LevitateThrowChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Tuning")
	float ThrowSpeed = 1000.f;

	/** Damage per (kg x m/s) when a thrown object hits someone. 5 kg at 10 m/s with 0.3 = 15 damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poltergeist|Tuning")
	float ThrowDamageScale = 0.3f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBParanormalSourceComponent> EMFSource;

private:
	struct FLevitating
	{
		TWeakObjectPtr<UPrimitiveComponent> Comp;
		FVector Start = FVector::ZeroVector;
		float StartTime = 0.f;
		float Phase = 0.f;
		bool bThrowAfter = false;
	};

	struct FThrown
	{
		TWeakObjectPtr<UPrimitiveComponent> Comp;
		float EndTime = 0.f;
		TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	};

	struct FRattling
	{
		TWeakObjectPtr<UPrimitiveComponent> Comp;
		float EndTime = 0.f;
		float NextKick = 0.f;
	};

	bool IsActiveNow(APawn* Player) const;
	void DoRandomEvent(APawn* Player);

	void EventRattle(int32 Count, float Duration);
	void EventLevitate(APawn* Player);
	void EventThrow(APawn* Player);
	void EventDoors();
	void EventLights(APawn* Player, float Strength);
	void EventLamps();

	void ThrowAt(UPrimitiveComponent* Comp, APawn* Player);
	void GetMovableBodies(TArray<UPrimitiveComponent*>& Out) const;
	void UpdateMotion(float Now, APawn* Player);

	TArray<FLevitating> Levitating;
	TArray<FThrown> Thrown;
	TArray<FRattling> Rattling;

	float NextEventTime = 0.f;
	float EMFPulseEnd = 0.f;
	float BaseEMF = 1.5f;
	bool bPlayerWasInside = false;
	bool bScared = false;
	bool bHadGhost = false;
};
