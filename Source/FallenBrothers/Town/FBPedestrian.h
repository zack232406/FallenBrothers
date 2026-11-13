// Fallen Brothers - A pedestrian: the new townspeople.
//
// A real Character (capsule + Character Movement): steps up and down curbs physically, never walks through
// people or walls, and dodges other pedestrians (RVO avoidance). The look is an assembled MetaHuman
// blueprint spawned at play and carried along; its body is driven by a 1D blend space over ground speed,
// so starting, walking and stopping blend smoothly and the stride always matches the speed.
//
// Everyone can be questioned (look at them + F). Story characters carry their own conversation (or an FBNPC
// preset such as MotelClerk); everybody else gets a random townsperson's conversation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/FBInteractable.h"
#include "Dialogue/FBDialogueTypes.h"
#include "FBPedestrian.generated.h"

class UBlendSpace;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class AFBNPC;

UCLASS()
class FALLENBROTHERS_API AFBPedestrian : public ACharacter, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBPedestrian();

	/** The look: an assembled MetaHuman blueprint (BP_...). Spawned when play starts and carried by this character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedestrian")
	TSoftClassPtr<AActor> Look;

	/** Idle / walk blend space over ground speed for the look's body. Empty = the shared MetaHuman one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedestrian")
	TObjectPtr<UBlendSpace> Locomotion;

	/** Seated idle (only used when bSeated). Empty = the shared MetaHuman one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedestrian")
	TObjectPtr<UAnimSequenceBase> SitAnim;

	/** Walk the town's sidewalks. Off = stays where placed (a shopkeeper, a clerk behind the counter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedestrian")
	bool bWander = true;

	/** Sits where placed (bench, booth), facing the actor's forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedestrian")
	bool bSeated = false;

	/** Walking speed (cm/s). 0 = a random everyday pace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedestrian")
	float WalkSpeed = 0.f;

	/** For the random conversation's first names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedestrian")
	bool bFemale = false;

	/** Story character: an FBNPC preset (e.g. MotelClerk). None = use the conversation below (or a random one). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName StoryPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText NPCName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText NPCRole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Greeting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Farewell;

	/** A story character's questions. Empty (and no preset) = a random townsperson's conversation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FFBDialogueOption> Options;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return true; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

private:
	void SpawnLook();
	void UpdateBody();
	void Walk(float DeltaSeconds);
	void SetStripTarget();
	void ArriveAtTarget();
	bool IsTalking() const;

	UPROPERTY(Transient)
	TObjectPtr<AActor> LookActor;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<AFBNPC> TalkProxy;

	FRandomStream Random;
	int32 Strip = INDEX_NONE;
	bool bTowardsB = true;
	float Lateral = 0.f;
	FVector Target = FVector::ZeroVector;
	float PauseLeft = 0.f;
	float StuckCheck = 0.f;
	FVector StuckFrom = FVector::ZeroVector;
	bool bAskedName = false;
	bool bBodyOnSit = false;
};
