// Fallen Brothers - Case journal component.

#include "Investigation/FBCaseJournalComponent.h"

#include "UI/FBHUD.h"

UFBCaseJournalComponent::UFBCaseJournalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UFBCaseJournalComponent::AddEntry(const FFBJournalEntry& Entry, bool bNotifyPlayer)
{
	if (!Entry.Id.IsNone() && HasEntry(Entry.Id))
	{
		return false;
	}

	Entries.Add(Entry);
	OnJournalUpdated.Broadcast();

	if (bNotifyPlayer)
	{
		AFBHUD::ShowNotificationFor(GetOwner(),
			FText::Format(NSLOCTEXT("FB", "JournalUpdated", "Journal updated: {0}   [J]"), Entry.Title), 3.f);
	}
	return true;
}

void UFBCaseJournalComponent::AddClues(const TArray<FName>& NewClues)
{
	bool bChanged = false;
	for (const FName& Clue : NewClues)
	{
		if (!Clue.IsNone() && !Clues.Contains(Clue))
		{
			Clues.Add(Clue);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		OnJournalUpdated.Broadcast();
	}
}

void UFBCaseJournalComponent::AddClue(FName Clue)
{
	AddClues({ Clue });
}

bool UFBCaseJournalComponent::HasAllClues(const TArray<FName>& Required) const
{
	for (const FName& Clue : Required)
	{
		if (!Clue.IsNone() && !Clues.Contains(Clue))
		{
			return false;
		}
	}
	return true;
}

bool UFBCaseJournalComponent::HasEntry(FName EntryId) const
{
	return Entries.ContainsByPredicate([EntryId](const FFBJournalEntry& E) { return E.Id == EntryId; });
}
