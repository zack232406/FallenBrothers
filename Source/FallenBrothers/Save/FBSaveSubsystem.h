// Fallen Brothers - Save subsystem.
//
// F5 quick save, F9 quick load, autosave on objectives / arrival, load last save on death.
// Lives for the whole session (survives level changes): loading opens the saved level,
// and once it has started the player calls OnLevelStarted() which applies the save.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FBSaveSubsystem.generated.h"

class UFBSaveGame;

UCLASS()
class FALLENBROTHERS_API UFBSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UFBSaveSubsystem* Get(const UObject* WorldContext);

	/** Write the current game to disk. bQuiet = no "Game saved" message (autosaves). */
	bool SaveGame(const UObject* WorldContext, bool bQuiet = false);

	/** Re-open the saved level and restore everything once it has started. */
	bool LoadGame(const UObject* WorldContext);

	bool HasSave() const;

	/** New Game: delete the save file and forget this session's world changes. */
	void DeleteSave();

	/** "The Weeping Mill - Harlow Falls   03.10.2026  15:20" (empty when there is no save). */
	FString GetSaveSummary() const;

	/** Called by the player one tick after the level starts. Applies a pending load, or starts fresh. */
	void OnLevelStarted(UWorld* World);

	/** Remember that this placed actor is gone for good (picked up, killed...). */
	static void NotifyActorRemoved(const AActor* Actor);

	/** Remember that this door was unlocked. */
	static void NotifyDoorUnlocked(const AActor* Door);

private:
	void ApplyLoad(UWorld* World, const UFBSaveGame* Save);

	UPROPERTY()
	TObjectPtr<UFBSaveGame> PendingLoad;

	TSet<FName> RemovedActors;
	TSet<FName> UnlockedDoors;
};
