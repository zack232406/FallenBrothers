// Fallen Brothers - Save game data.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Inventory/FBInventoryTypes.h"
#include "Investigation/FBInvestigationTypes.h"
#include "FBSaveGame.generated.h"

UCLASS()
class FALLENBROTHERS_API UFBSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Map to open when loading. */
	UPROPERTY()
	FString LevelName;

	UPROPERTY()
	FDateTime SaveTime;

	/** Shown on the main menu's Continue button, e.g. "The Weeping Mill - Harlow Falls". */
	UPROPERTY()
	FString LocationLabel;

	// ---------------------------------------------------------------- Player
	UPROPERTY()
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator ControlRotation = FRotator::ZeroRotator;

	UPROPERTY()
	float PlayerHealthPercent = 1.f;

	UPROPERTY()
	TArray<FFBInventoryItem> Inventory;

	UPROPERTY()
	TArray<FFBJournalEntry> JournalEntries;

	/** Clues also drive objectives, so objectives come back by themselves. */
	UPROPERTY()
	TArray<FName> Clues;

	// ---------------------------------------------------------------- Campaign
	UPROPERTY()
	TArray<FName> CompletedMissions;

	// ---------------------------------------------------------------- World (current level)
	/** Picked-up items, collected evidence, dead creatures, banished ghosts. */
	UPROPERTY()
	TArray<FName> RemovedActors;

	UPROPERTY()
	TArray<FName> UnlockedDoors;
};
