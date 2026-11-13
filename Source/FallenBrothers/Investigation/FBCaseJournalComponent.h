// Fallen Brothers - Case journal component.
//
// Lives on the player. Remembers every clue (an id like "Clue_MillFire") and
// every journal entry (evidence found, traces seen, articles read).
// Press J to read it.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Investigation/FBInvestigationTypes.h"
#include "FBCaseJournalComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJournalUpdated);

UCLASS(ClassGroup = (FallenBrothers), meta = (BlueprintSpawnableComponent))
class FALLENBROTHERS_API UFBCaseJournalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFBCaseJournalComponent();

	/** Adds an entry (ignored if an entry with the same Id exists). Returns true if new. */
	UFUNCTION(BlueprintCallable, Category = "Journal")
	bool AddEntry(const FFBJournalEntry& Entry, bool bNotifyPlayer = true);

	UFUNCTION(BlueprintCallable, Category = "Journal")
	void AddClues(const TArray<FName>& NewClues);

	UFUNCTION(BlueprintCallable, Category = "Journal")
	void AddClue(FName Clue);

	UFUNCTION(BlueprintPure, Category = "Journal")
	bool HasClue(FName Clue) const { return Clues.Contains(Clue); }

	UFUNCTION(BlueprintPure, Category = "Journal")
	bool HasAllClues(const TArray<FName>& Required) const;

	UFUNCTION(BlueprintPure, Category = "Journal")
	bool HasEntry(FName EntryId) const;

	UFUNCTION(BlueprintPure, Category = "Journal")
	const TArray<FFBJournalEntry>& GetEntries() const { return Entries; }

	TArray<FName> GetClueArray() const { return Clues.Array(); }

	/** Replace everything (used when loading a save). */
	void RestoreState(const TArray<FFBJournalEntry>& InEntries, const TArray<FName>& InClues)
	{
		Entries = InEntries;
		Clues = TSet<FName>(InClues);
		OnJournalUpdated.Broadcast();
	}

	UPROPERTY(BlueprintAssignable, Category = "Journal")
	FOnJournalUpdated OnJournalUpdated;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Journal")
	TArray<FFBJournalEntry> Entries;

	UPROPERTY(VisibleInstanceOnly, Category = "Journal")
	TSet<FName> Clues;
};
