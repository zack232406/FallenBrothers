// Fallen Brothers - Case file.

#include "Investigation/FBCaseFile.h"

#include "Investigation/FBCaseJournalComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"

AFBCaseFile::AFBCaseFile()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BuildDemoCase();
}

AFBCaseFile* AFBCaseFile::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AFBCaseFile> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AFBCaseFile::GetAvailableArchive(const UFBCaseJournalComponent* Journal, TArray<int32>& OutIndices) const
{
	OutIndices.Reset();
	for (int32 i = 0; i < Archive.Num(); ++i)
	{
		if (!Journal || Journal->HasAllClues(Archive[i].RequiredClues))
		{
			OutIndices.Add(i);
		}
	}
}

void AFBCaseFile::GetUnlockedTheories(const UFBCaseJournalComponent* Journal, TArray<FText>& OutTheories) const
{
	OutTheories.Reset();
	for (const FFBCaseTheory& Theory : Theories)
	{
		if (Journal && Journal->HasAllClues(Theory.RequiredClues))
		{
			OutTheories.Add(Theory.Text);
		}
	}
}

// ---------------------------------------------------------------------------
// Demo case: "The Weeping Mill" (original story for testing).
//
// Clue chain:
//   start            -> "Harlow Falls disappearances"   -> Clue_OldMill, Clue_ColdSpots
//   Clue_OldMill     -> "Harlow Textile Mill"           -> Clue_MillFire
//   Clue_MillFire    -> "Elias Crane"                   -> Clue_Crane
//   Clue_Crane       -> "Crane burial record"           -> Clue_GraveScratched (no location: ask Martha -> Clue_CraneGrave)
//   Clue_ColdSpots   -> "Cold spots folklore"           -> Clue_Weakness
// Evidence / UV traces in the level can also give clues (set "Grants Clues" on them).
// ---------------------------------------------------------------------------
void AFBCaseFile::BuildDemoCase()
{
	CaseTitle = FText::FromString(TEXT("The Weeping Mill"));
	TownName = FText::FromString(TEXT("Harlow Falls"));

	auto Add = [this](FName Id, const TCHAR* Term, const TCHAR* Title, const TCHAR* Source, const TCHAR* Body,
	                  TArray<FName> Required, TArray<FName> Grants, const TCHAR* Note)
	{
		FFBArchiveEntry E;
		E.Id = Id;
		E.SearchTerm = Term;
		E.Title = FText::FromString(Title);
		E.Source = FText::FromString(Source);
		E.Body = FText::FromString(Body);
		E.RequiredClues = MoveTemp(Required);
		E.GrantsClues = MoveTemp(Grants);
		E.JournalNote = FText::FromString(Note);
		Archive.Add(E);
	};

	Add(TEXT("Arc_Disappearances"),
		TEXT("Harlow Falls disappearances"),
		TEXT("Third Person Missing Near Old Mill Road"),
		TEXT("Harlow Falls Courier - this week"),
		TEXT("Search parties returned empty-handed again on Thursday after Dale Whitmore, 34, became the third local resident "
		     "to vanish near the abandoned textile mill on Old Mill Road this year.\n\n"
		     "Volunteers describe sudden patches of freezing air inside the building and flashlights dying without warning. "
		     "\"You hear crying from the second floor,\" one searcher said. \"Then it just stops.\""),
		{}, { TEXT("Clue_OldMill"), TEXT("Clue_ColdSpots") },
		TEXT("Three people missing near the old textile mill. Witnesses: cold spots, flashlights failing, crying from upstairs."));

	Add(TEXT("Arc_MillFire"),
		TEXT("Harlow Textile Mill"),
		TEXT("Mill Fire Claims Fourteen Lives"),
		TEXT("Harlow Falls Gazette - October 14, 1952"),
		TEXT("Fourteen workers died last night when fire swept through the second floor of the Harlow Textile Mill. "
		     "Firemen found the stairwell doors chained shut from the outside.\n\n"
		     "Police wish to question foreman Elias Crane, who held the only keys and has not been seen since the blaze. "
		     "Workers had complained for months about his cruelty."),
		{ TEXT("Clue_OldMill") }, { TEXT("Clue_MillFire") },
		TEXT("1952: fire at the mill, 14 workers died behind chained doors. Foreman Elias Crane held the keys - vanished."));

	Add(TEXT("Arc_Crane"),
		TEXT("Elias Crane"),
		TEXT("Body of Missing Foreman Found in River"),
		TEXT("Harlow Falls Gazette - March 2, 1953"),
		TEXT("The remains of Elias Crane, wanted in connection with last autumn's mill fire, were pulled from the Harlow River "
		     "below the falls. An iron key ring was found clutched in his hand.\n\n"
		     "With no family willing to claim him, the town quietly buried Crane in an unmarked plot."),
		{ TEXT("Clue_MillFire") }, { TEXT("Clue_Crane") },
		TEXT("Crane's body found in the river, 1953, still holding the mill keys. Buried in an unmarked grave."));

	Add(TEXT("Arc_Grave"),
		TEXT("Crane burial record"),
		TEXT("Parish Burial Register - Entry 211"),
		TEXT("St. Agnes Chapel records"),
		TEXT("Interred: [entry scratched out - the pen went through the paper]. Plot: [illegible].\n\n"
		     "Margin note in a different hand: \"The digger refused to go back after dark. Says the ground was cold as January.\"\n\n"
		     "Someone did not want this grave found. The families who lost people in the fire might still know."),
		{ TEXT("Clue_Crane") }, { TEXT("Clue_GraveScratched") },
		TEXT("Crane's burial entry is scratched out of the register. A grave digger is mentioned - ask the old families (Martha?)."));

	Add(TEXT("Arc_Folklore"),
		TEXT("Cold spots folklore"),
		TEXT("Restless Dead: A Survey of Local Beliefs"),
		TEXT("County Historical Society pamphlet, 1961"),
		TEXT("Old settlers held that a spirit chained to the living world announces itself with sudden cold, failing lamps "
		     "and the sounds of its last hour.\n\n"
		     "Cold iron was said to scatter such a presence for a time, and a line of salt to bar its way. "
		     "Only burning the bones, salted, was believed to send it on for good."),
		{ TEXT("Clue_ColdSpots") }, { TEXT("Clue_Weakness") },
		TEXT("Folklore: iron scatters restless spirits, salt blocks them. Salting and burning the remains ends it."));

	FFBCaseTheory T1;
	T1.RequiredClues = { TEXT("Clue_MillFire"), TEXT("Clue_ColdSpots") };
	T1.Text = FText::FromString(TEXT("Something is haunting the mill. Cold spots, crying... it started with the 1952 fire."));
	Theories.Add(T1);

	FFBCaseTheory T2;
	T2.RequiredClues = { TEXT("Clue_Crane"), TEXT("Clue_Weakness") };
	T2.Text = FText::FromString(TEXT("Vengeful spirit - almost certainly Elias Crane. Iron will hold him off, salt will keep him out."));
	Theories.Add(T2);

	FFBCaseTheory T3;
	T3.RequiredClues = { TEXT("Clue_CraneGrave"), TEXT("Clue_Weakness") };
	T3.Text = FText::FromString(TEXT("PLAN: wait for dark, then dig up Crane's grave behind St. Agnes Chapel. Salt and burn the bones."));
	Theories.Add(T3);
}
