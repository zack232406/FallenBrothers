// Fallen Brothers - Townsfolk: the town's people, on Epic's Game Animation Sample locomotion.
//
// The C++ parent of the Game Animation Sample's character (SandboxCharacter_CMC is reparented to this class).
// That blueprint brings the AAA-grade movement: motion matching over hundreds of mocap clips (starts, stops,
// pivots, turns in place), foot placement / leg IK on curbs and slopes, the character-movement physics.
// It drives an invisible UEFN mannequin; the person you see is a real model retargeted from it every frame
// (ABP_GenericRetarget + an IK Retargeter named in the look's tag), or a whole MetaHuman (visual override).
//
// This class adds what the town needs on top: the look, a walk along the sidewalks (corners, crossings,
// pauses, keeping out of the player's way), and the interview (look at them + F).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/FBInteractable.h"
#include "Dialogue/FBDialogueTypes.h"
#include "FBTownsfolk.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class AFBNPC;

UCLASS(Blueprintable)
class FALLENBROTHERS_API AFBTownsfolk : public ACharacter, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBTownsfolk();

	/** The model to wear (retargeted from the hidden mannequin every frame). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Look")
	TSoftObjectPtr<USkeletalMesh> FBLookMesh;

	/** IK Retargeter for that model, by name (a key of ABP_GenericRetarget's IKRetargeter_Map). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Look")
	FName FBLookRetargeter;

	/** Or a whole assembled MetaHuman blueprint (BP_Kellan...) worn through the visual override. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Look")
	TSoftClassPtr<AActor> FBLookActor;

	/** Turns the model if it doesn't face the way it walks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Look")
	float FBLookYaw = 0.f;

	/** How tall this person is (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Look")
	float FBHeight = 176.f;

	/** Walk the town's sidewalks. Off = stays where placed (a clerk behind the counter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Walk")
	bool FBWander = true;

	/** Walking pace (cm/s). 0 = a random everyday pace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Walk")
	float FBWalkPace = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Dialogue")
	bool FBFemale = false;

	/** Story character: an FBNPC preset (MotelClerk, Martha...). None = the conversation below or a random one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Dialogue")
	FName FBStoryPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Dialogue")
	FText FBName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Dialogue")
	FText FBRole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Dialogue", meta = (MultiLine = true))
	FText FBGreeting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Dialogue", meta = (MultiLine = true))
	FText FBFarewell;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Townsfolk|Dialogue")
	TArray<FFBDialogueOption> FBOptions;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return true; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	/** The town's everyday faces: model, retargeter, height range. */
	struct FLookDef
	{
		const TCHAR* Mesh;
		const TCHAR* Retargeter;
		float MinHeight;
		float MaxHeight;
		bool bFemale;
		float Yaw;
	};
	static TArrayView<const FLookDef> Looks();

private:
	void ApplyLook();
	void FitLook(float DeltaSeconds);
	void SetUpGaspMovement();
	void Walk(float DeltaSeconds);
	void SetStripTarget();
	void ArriveAtTarget();
	bool IsTalking() const;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> LookBody;

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
	bool bWasTalking = false;
	float FitWait = 0.f;
	int32 FitRuns = 0;
	bool bFitDone = true;
};
