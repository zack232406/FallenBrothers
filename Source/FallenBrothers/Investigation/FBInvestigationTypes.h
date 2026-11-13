// Fallen Brothers - Investigation data types.

#pragma once

#include "CoreMinimal.h"
#include "FBInvestigationTypes.generated.h"

UENUM(BlueprintType)
enum class EFBJournalEntryType : uint8
{
	Evidence,   // Physical evidence picked up
	Trace,      // UV traces, marks, signs
	Research,   // Read on the computer
	Interview,  // NPC conversations (step 7)
	Note        // Anything else
};

/** One line in the case journal. */
USTRUCT(BlueprintType)
struct FFBJournalEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal", meta = (MultiLine = true))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	EFBJournalEntryType Type = EFBJournalEntryType::Note;
};

/** One searchable record in the town archive (newspaper, police report, obituary...). */
USTRUCT(BlueprintType)
struct FFBArchiveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	FName Id;

	/** What appears in the search list / gets typed into the search bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	FString SearchTerm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	FText Title;

	/** e.g. "Harlow Falls Gazette - Oct 14, 1952" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	FText Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive", meta = (MultiLine = true))
	FText Body;

	/** The search term only appears once the player knows ALL of these clues. Empty = available from the start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	TArray<FName> RequiredClues;

	/** Reading this record gives these clues (which unlock more searches). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	TArray<FName> GrantsClues;

	/** Short summary written into the journal after reading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive", meta = (MultiLine = true))
	FText JournalNote;
};

/** A conclusion the journal shows once enough clues are known. */
USTRUCT(BlueprintType)
struct FFBCaseTheory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theory")
	TArray<FName> RequiredClues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theory", meta = (MultiLine = true))
	FText Text;
};
