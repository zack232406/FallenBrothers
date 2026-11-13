// Fallen Brothers - Talkable NPC.
//
// A townsperson you can interview. Look at them + F.
// Questions and answers are edited in the Details panel ("Dialogue" section).
// Comes pre-filled with a demo NPC (Martha Kell, diner owner) for the demo case.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FBInteractable.h"
#include "Dialogue/FBDialogueTypes.h"
#include "FBNPC.generated.h"

class UStaticMeshComponent;
class UCapsuleComponent;
class UFBCaseJournalComponent;

UCLASS()
class FALLENBROTHERS_API AFBNPC : public AActor, public IFBInteractable
{
	GENERATED_BODY()

public:
	AFBNPC();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return true; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	/** Called by the HUD when the conversation starts / ends. */
	void BeginConversation(AActor* Player);
	void EndConversation();

	/** Indices into Options that the player may ask right now. */
	void GetAvailableOptions(const UFBCaseJournalComponent* Journal, TArray<int32>& OutIndices) const;

	/** Remember that an option was asked (shown dimmed afterwards). */
	void MarkAsked(FName OptionId) { AskedOptions.Add(OptionId); }
	bool WasAsked(FName OptionId) const { return AskedOptions.Contains(OptionId); }

	const TArray<FFBDialogueOption>& GetOptions() const { return Options; }
	FText GetNPCName() const { return NPCName; }
	FText GetRole() const { return NPCRole; }
	FText GetGreeting() const { return Greeting; }
	FText GetFarewell() const { return Farewell; }

	/** World point the player's camera looks at while talking. */
	FVector GetHeadLocation() const;

	/**
	 * Turns this into an invisible stand-in holding an ordinary townsperson's conversation: the HUD's interview
	 * screen talks to it, the townsperson moves it to its own head. No body, no collision.
	 */
	void ConfigureProxy(const FText& InName, const FText& InRole, const FText& InGreeting, const FText& InFarewell, const TArray<FFBDialogueOption>& InOptions);

	/** The conversation of preset InPreset (e.g. MotelClerk) on an invisible stand-in for a pedestrian's body. */
	void ConfigureStoryProxy(FName InPreset);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText NPCName;

	/** Shown under the name, e.g. "Diner owner". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText NPCRole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Greeting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Farewell;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FFBDialogueOption> Options;

	/** Who this is: None = Martha (the demo default), "MotelClerk" = the motel's night clerk (room 7 key card). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName Preset;

	/** Placeholder clothing colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FLinearColor ClothingColor = FLinearColor(0.35f, 0.15f, 0.12f);

	/** How fast they turn to face you. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float TurnSpeed = 4.f;

private:
	void BuildDemoNPC();
	void BuildMotelClerk();
	void BuildFamilyAndLibrary(FName Who);
	void ApplyPreset();
	FName AppliedPreset;

	TWeakObjectPtr<AActor> ConversationPartner;
	TSet<FName> AskedOptions;
};
