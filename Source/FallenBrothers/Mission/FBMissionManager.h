// Fallen Brothers - Mission manager.
//
// One per case level. Holds the case title, the town and the objectives.
// Objectives complete automatically when the player learns a clue
// (evidence, computer research, interviews, killing a creature, burning remains...).
// When every required objective is done: CASE CLOSED -> travel to the next town's level.
//
// Comes pre-filled with the demo case "The Weeping Mill" - edit everything in Details.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBMissionManager.generated.h"

USTRUCT(BlueprintType)
struct FFBObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FText Text;

	/** Done as soon as the player's journal has this clue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FName CompleteWhenClue;

	/** Optional objectives don't block finishing the case. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	bool bOptional = false;
};

/** One line for the journal's objective list. */
struct FFBObjectiveStatus
{
	FText Text;
	bool bDone = false;
	bool bOptional = false;
};

UCLASS()
class FALLENBROTHERS_API AFBMissionManager : public AActor
{
	GENERATED_BODY()

public:
	AFBMissionManager();

	virtual void Tick(float DeltaSeconds) override;

	/** The mission manager in this level (or null). */
	static AFBMissionManager* Find(const UObject* WorldContext);

	/** Text of the first unfinished required objective (empty when the case is closed). */
	FText GetCurrentObjectiveText() const;

	/** Objectives the player has reached so far, in order. */
	void GetVisibleObjectives(TArray<FFBObjectiveStatus>& Out) const;

	bool IsMissionComplete() const { return bMissionComplete; }
	FText GetMissionTitle() const { return MissionTitle; }
	FText GetTownName() const { return TownName; }
	FName GetMissionId() const { return MissionId; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Unique id, used by the save system. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName MissionId = TEXT("Case01_WeepingMill");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 MissionNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 TotalMissions = 13;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText MissionTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText TownName;

	/** What the brother says when you arrive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (MultiLine = true))
	FText ArrivalLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FFBObjective> Objectives;

	/** Level (map) name of the next case, e.g. "Case02_Map". Empty = end of the demo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName NextLevelName;

	/** Seconds on the CASE CLOSED screen before travelling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	float EndDelay = 9.f;

private:
	void BuildDemoMission();
	void ShowIntro();
	void ShowFirstObjective();
	void SayArrivalLine();
	void CompleteMission();
	void TravelToNextCase();
	int32 GetCurrentObjectiveIndex() const;
	class AFBHUD* GetHUD() const;

	TSet<FName> CompletedObjectives;
	int32 AnnouncedObjectiveIndex = INDEX_NONE;
	bool bIntroDone = false;
	bool bMissionComplete = false;
	bool bAutosavePending = false;
	FTimerHandle IntroTimer;
	FTimerHandle ObjectiveTimer;
	FTimerHandle LineTimer;
	FTimerHandle TravelTimer;
};
