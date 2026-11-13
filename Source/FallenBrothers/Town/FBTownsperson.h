// Fallen Brothers - A townsperson: a downloaded, animated person living in town.
//
// They walk the town's sidewalks (turning corners, crossing streets), stop to look around, sit on benches, and
// you can question any of them (look at them + F): name, job, their own answers, sometimes a real lead.
//
// Some downloaded animations scale or shift the whole rig, so in play a person could turn into a giant.
// When play starts this measures the animated skeleton and fits the person back to TargetHeight, feet on GroundZ.

#pragma once

#include "CoreMinimal.h"
#include "Animation/SkeletalMeshActor.h"
#include "Interaction/FBInteractable.h"
#include "FBTownsperson.generated.h"

class UAnimationAsset;
class UCapsuleComponent;
class AFBNPC;

UCLASS()
class FALLENBROTHERS_API AFBTownsperson : public ASkeletalMeshActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBTownsperson();

	/** Real height of this person (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsperson")
	float TargetHeight = 175.f;

	/** World Z of the ground the person stands on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsperson")
	float GroundZ = 0.f;

	/**
	 * Optional full look for this body (a MetaHuman blueprint: face, hair, clothes). Spawned when play starts,
	 * its parts follow this actor's animated body (leader pose); its own body is hidden.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsperson")
	TSoftClassPtr<AActor> Look;

	/** Walk the whole town's sidewalks (corners, crossings) instead of strolling up and down one stretch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsperson")
	bool bWalkTheTown = true;

	/** Stays where they are (own animation, e.g. the traffic cop), still answers questions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsperson")
	bool bStandStill = false;

	/** Sits (bench / booth) at the spot they were placed, facing SeatYaw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsperson")
	bool bSeated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsperson")
	float SeatYaw = 0.f;

	/** Spawned by the crowd at play time (already facing the right way). */
	UPROPERTY()
	bool bRuntimeSpawned = false;

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return true; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	/** Walking velocity right now (cm/s, zero when standing) - the cars use it. */
	FVector GetWalkVelocity() const { return WalkVelocity; }

	/** Talking with the player right now. */
	bool IsTalking() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** What the interaction trace hits (the skin itself often has no collision bodies). */
	UPROPERTY(VisibleAnywhere, Category = "Townsperson")
	TObjectPtr<UCapsuleComponent> TalkCapsule;

private:
	enum class EMode : uint8 { Stand, Walk, Pause, Talk, Sit };

	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> WalkAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> IdleAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> LookAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> SitAnim;
	UPROPERTY(Transient) TObjectPtr<AFBNPC> TalkProxy;

	void PlayAnim(UAnimationAsset* Anim, float Rate = 1.f, bool bRandomStart = true);
	void StartWalking();
	void ArriveAtTarget();
	void SetStripTarget(bool bToEnd);
	void UpdateGround(const FVector& Dir);
	void SmoothGround(float Dt);
	void Pause(float Seconds, bool bLookAround);
	FText DisplayName() const;

	EMode Mode = EMode::Stand;
	EMode ModeBeforeTalk = EMode::Stand;
	FRandomStream Random;
	float Age = 0.f;                // seconds since play started
	float Speed = 130.f;            // cm/s
	float Yaw = 0.f;                // world yaw the person looks along
	float Wait = 0.f;
	float StepTimer = 0.f;
	float GroundTimer = 0.f;
	float CurrentGround = 0.f;      // the ground the feet are heading for (a curb top once they step onto it)
	float ShownGround = 0.f;        // where the body actually is - eases up / down curbs like a real step
	float CurSpeed = 0.f;           // walking speed right now (people speed up and slow down, never jump)
	float StepSlow = 0.f;           // seconds of "watch your step" slow-down while going up a curb
	float Lateral = 0.f;            // offset to the right of the walking direction (keep right)
	float BlockedTimer = 0.f;
	FVector Target = FVector::ZeroVector;
	FVector WalkVelocity = FVector::ZeroVector;
	bool bPersonHidden = false;
	bool bFitDone = false;

	// On the sidewalk network
	int32 Strip = INDEX_NONE;
	bool bTowardsB = true;          // walking A -> B on the current strip
	bool bOnStrip = false;          // false = crossing / turning to the start of the next strip
	float PauseAtT = -1.f;          // stop here (0..1 along the strip) to look around, -1 = no stop

	// Old style: up and down one stretch of sidewalk
	bool bStroll = false;
	FVector Home = FVector::ZeroVector;
	FVector Along = FVector::ForwardVector;
	float Reach = 1200.f;
	float Side = 1.f;

	bool bAskedName = false;
};
