// Fallen Brothers - Case director: every case is one day and one evening.
//
// DAY (investigation): overcast daylight, the town is busy, the ghost is only a cold spot and a needle on the EMF.
//   You can't banish it yet - iron only drives it off, and the grave can't be dug with the whole town watching.
//   Talk to the families, the library, the witnesses until you know who it is, how to stop it and where he lies.
// DUSK: once you know enough, your brother says to wait for dark. Go back to the motel: the screen fades,
//   "That night", the streets empty, the fog comes in.
// EVENING (confrontation): the ghost is awake. Dig, salt and burn while it fights back.
//
// A world subsystem: nothing to place. It runs in any level with a ghost, a grave or a mission manager.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Investigation/FBInvestigationTypes.h"
#include "Inventory/FBInventoryTypes.h"
#include "FBCaseDirector.generated.h"

UENUM(BlueprintType)
enum class EFBCasePhase : uint8
{
	Day,
	Dusk,     // the fade to night is playing
	Evening
};

/** What the player carries from the day level into the night level (journal, clues, inventory). */
UCLASS()
class FALLENBROTHERS_API UFBCaseCarryOver : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FFBJournalEntry> Entries;

	UPROPERTY()
	TArray<FName> Clues;

	UPROPERTY()
	TArray<FFBInventoryItem> Items;

	bool bPending = false;
};

UCLASS()
class FALLENBROTHERS_API UFBCaseDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UFBCaseDirector* Get(const UObject* WorldContext);

	/** True only while a case runs and it is the evening part (the grave may be dug, the ghost hunts). */
	static bool IsEvening(const UWorld* World);

	/** True while a case runs and it is still daytime (the town is busy). */
	static bool IsCaseDay(const UWorld* World);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Sets the case up (day light, sleeping ghost). Safe to call more than once; other systems call it first. */
	void EnsureStarted();

	EFBCasePhase GetPhase() const { return Phase; }
	bool IsActive() const { return bActive; }

	/** The evening part plays in its own level (the mill, the woods, the churchyard - no town). */
	bool IsNightLevel() const { return bNightLevel; }

	/** The night level of this case. If the map doesn't exist the evening happens in the day level. */
	FString NightLevelPath = TEXT("/Game/FallenBrothers/Maps/HarlowFalls_Night");

	/** Clues needed before the evening can start (all of them). */
	TArray<FName> EveningClues = { TEXT("Clue_CraneGrave"), TEXT("Clue_Weakness"), TEXT("Clue_Families"), TEXT("Clue_CraneDeath"), TEXT("Clue_Trail") };

	/** Where you wait for dark: the motel. */
	FVector WaitPoint = FVector(-16320.f, 5601.f, 124.f);
	float WaitRadius = 1800.f;

private:
	void Check();
	void StartDusk();
	void ApplyEvening(bool bInstant);
	void ApplyDay();
	void SetGhostsAwake(bool bAwake);
	void SendTownHome();
	void SpawnLastSeenSpots();
	void TravelToNight();
	void NightIntro();
	class APawn* GetPlayer() const;

	EFBCasePhase Phase = EFBCasePhase::Day;
	bool bStarted = false;
	bool bActive = false;
	bool bToldToWait = false;
	bool bNightLevel = false;
	bool bCarryChecked = false;
	FTimerHandle CheckTimer;
	FTimerHandle DuskTimer;
	FTimerHandle FadeInTimer;
};
