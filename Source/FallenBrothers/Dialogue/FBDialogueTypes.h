// Fallen Brothers - Dialogue data types.

#pragma once

#include "CoreMinimal.h"
#include "FBDialogueTypes.generated.h"

/** One question the player can ask an NPC, and the NPC's answer. */
USTRUCT(BlueprintType)
struct FFBDialogueOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName Id;

	/** What Chris asks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText PlayerLine;

	/** What the NPC answers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText NpcResponse;

	/** Only appears once the player knows ALL of these clues (evidence, research, other talks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FName> RequiredClues;

	/** Asking this gives these clues. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FName> GrantsClues;

	/** Only appears while the player carries this item (e.g. the fake FBI badge). None = no item needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName RequiredItemId;

	/** Asking this hands the player an item (e.g. a motel key card). None = nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName GrantsItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText GrantsItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText GrantsItemDescription;

	/** Chris holds up his fake FBI credentials while saying this line (shown on screen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bShowsBadge = false;

	/** Written into the journal (as TALK). Empty = nothing written. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText JournalNote;
};
