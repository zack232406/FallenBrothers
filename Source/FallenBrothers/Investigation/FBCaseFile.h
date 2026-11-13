// Fallen Brothers - Case file.
//
// One per mission level. Holds everything the player can research about this case:
// the town archive (newspapers, records) and the theories the journal can reach.
// Drop one into the level; it comes pre-filled with a demo case you can edit in Details.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Investigation/FBInvestigationTypes.h"
#include "FBCaseFile.generated.h"

class UFBCaseJournalComponent;

UCLASS()
class FALLENBROTHERS_API AFBCaseFile : public AActor
{
	GENERATED_BODY()

public:
	AFBCaseFile();

	/** The first case file in the level (or null). */
	static AFBCaseFile* Find(const UObject* WorldContext);

	/** Indices of archive entries whose search term is unlocked for this journal. */
	void GetAvailableArchive(const UFBCaseJournalComponent* Journal, TArray<int32>& OutIndices) const;

	/** Theories the player has enough clues for. */
	void GetUnlockedTheories(const UFBCaseJournalComponent* Journal, TArray<FText>& OutTheories) const;

	const TArray<FFBArchiveEntry>& GetArchive() const { return Archive; }
	FText GetCaseTitle() const { return CaseTitle; }
	FText GetTownName() const { return TownName; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText CaseTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText TownName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FFBArchiveEntry> Archive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FFBCaseTheory> Theories;

private:
	void BuildDemoCase();
};
