// Fallen Brothers - HUD screens: case journal (J) and research computer terminal.
// Part of AFBHUD, split into its own file to keep FBHUD.cpp readable.

#include "UI/FBHUD.h"

#include "Investigation/FBCaseFile.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Investigation/FBResearchComputer.h"
#include "Mission/FBMissionManager.h"
#include "Dialogue/FBNPC.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace FBScreenStyle
{
	// Journal: warm paper-ish text on dark.
	const FLinearColor JournalBg(0.02f, 0.02f, 0.02f, 0.88f);
	const FLinearColor JournalText(0.92f, 0.88f, 0.8f, 0.95f);
	const FLinearColor JournalDim(0.92f, 0.88f, 0.8f, 0.45f);
	const FLinearColor JournalAccent(0.95f, 0.75f, 0.35f, 1.f);

	// Terminal: old green phosphor monitor.
	const FLinearColor TermBg(0.01f, 0.03f, 0.02f, 0.94f);
	const FLinearColor TermText(0.45f, 1.f, 0.55f, 0.95f);
	const FLinearColor TermDim(0.45f, 1.f, 0.55f, 0.4f);
	const FLinearColor TermHighlight(0.45f, 1.f, 0.55f, 0.18f);

	// Typing speed of the search bar (characters per second).
	constexpr float TypeSpeed = 28.f;
}

// ------------------------------------------------------------------ Open / close / input

UFBCaseJournalComponent* AFBHUD::GetJournal() const
{
	const APawn* Pawn = GetOwningPawn();
	return Pawn ? Pawn->FindComponentByClass<UFBCaseJournalComponent>() : nullptr;
}

void AFBHUD::ToggleJournal()
{
	if (ActiveScreen == EFBScreen::Journal)
	{
		CloseScreen();
		return;
	}
	ActiveScreen = EFBScreen::Journal;
	ScreenSelection = 0;
	bInventoryOpen = false;
}

void AFBHUD::OpenComputer(AFBResearchComputer* Computer)
{
	ActiveScreen = EFBScreen::Computer;
	ActiveComputer = Computer;
	ScreenSelection = 0;
	ComputerReadingIndex = INDEX_NONE;
	ComputerPendingIndex = INDEX_NONE;
	TypingStartTime = -1.f;
	bInventoryOpen = false;
}

void AFBHUD::CloseScreen()
{
	if (AFBNPC* NPC = ActiveNPC.Get())
	{
		NPC->EndConversation();
	}
	ActiveNPC.Reset();
	ActiveScreen = EFBScreen::None;
	ActiveComputer.Reset();
	TypingStartTime = -1.f;
}

void AFBHUD::NavigateUI(int32 Direction)
{
	if (PauseMenu.IsOpen())
	{
		PauseMenu.Navigate(Direction);
		return;
	}
	if (ActiveScreen == EFBScreen::None)
	{
		return;
	}

	int32 Count = 0;
	if (ActiveScreen == EFBScreen::Dialogue)
	{
		Count = GetDialogueChoiceCount();
	}
	else if (ActiveScreen == EFBScreen::Journal)
	{
		const UFBCaseJournalComponent* Journal = GetJournal();
		Count = Journal ? Journal->GetEntries().Num() : 0;
	}
	else if (const AFBCaseFile* Case = AFBCaseFile::Find(this))
	{
		TArray<int32> Available;
		Case->GetAvailableArchive(GetJournal(), Available);
		Count = Available.Num();
	}

	ScreenSelection = Count > 0 ? FMath::Clamp(ScreenSelection + Direction, 0, Count - 1) : 0;
}

void AFBHUD::ConfirmUI()
{
	if (PauseMenu.IsOpen())
	{
		PauseMenu.Confirm();
		return;
	}
	if (ActiveScreen == EFBScreen::Dialogue)
	{
		ConfirmDialogue();
		return;
	}

	if (ActiveScreen != EFBScreen::Computer || TypingStartTime >= 0.f)
	{
		return; // Journal has nothing to confirm; ignore while typing.
	}

	const AFBCaseFile* Case = AFBCaseFile::Find(this);
	if (!Case)
	{
		return;
	}

	TArray<int32> Available;
	Case->GetAvailableArchive(GetJournal(), Available);
	if (!Available.IsValidIndex(ScreenSelection))
	{
		return;
	}

	// Start "typing" the search term.
	ComputerPendingIndex = Available[ScreenSelection];
	ComputerReadingIndex = INDEX_NONE;
	TypingStartTime = GetWorld()->GetRealTimeSeconds();
}

void AFBHUD::FinishComputerSearch()
{
	TypingStartTime = -1.f;
	ComputerReadingIndex = ComputerPendingIndex;
	ComputerPendingIndex = INDEX_NONE;

	const AFBCaseFile* Case = AFBCaseFile::Find(this);
	UFBCaseJournalComponent* Journal = GetJournal();
	if (!Case || !Journal || !Case->GetArchive().IsValidIndex(ComputerReadingIndex))
	{
		return;
	}

	// Reading a record writes it into the journal and unlocks new clues / searches.
	const FFBArchiveEntry& Entry = Case->GetArchive()[ComputerReadingIndex];
	FFBJournalEntry JournalEntry;
	JournalEntry.Id = Entry.Id;
	JournalEntry.Title = Entry.Title;
	JournalEntry.Text = Entry.JournalNote.IsEmpty() ? Entry.Body : Entry.JournalNote;
	JournalEntry.Type = EFBJournalEntryType::Research;
	Journal->AddEntry(JournalEntry);
	Journal->AddClues(Entry.GrantsClues);
}

// ------------------------------------------------------------------ Text helper

float AFBHUD::DrawWrappedText(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, float Scale, float MaxY)
{
	UFont* Font = GEngine->GetMediumFont();
	float Unused = 0.f, LineH = 0.f;
	GetTextSize(TEXT("Ag"), Unused, LineH, Font, Scale);
	LineH *= 1.2f;

	TArray<FString> Paragraphs;
	Text.ParseIntoArray(Paragraphs, TEXT("\n"), /*bCullEmpty*/ false);

	for (const FString& Paragraph : Paragraphs)
	{
		if (Y > MaxY)
		{
			break;
		}

		TArray<FString> Words;
		Paragraph.ParseIntoArrayWS(Words);
		if (Words.Num() == 0)
		{
			Y += LineH * 0.6f; // Blank line between paragraphs.
			continue;
		}

		FString Line;
		for (const FString& Word : Words)
		{
			const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			float W = 0.f, H = 0.f;
			GetTextSize(Candidate, W, H, Font, Scale);

			if (W > MaxWidth && !Line.IsEmpty())
			{
				DrawText(Line, Color, X, Y, Font, Scale);
				Y += LineH;
				Line = Word;
				if (Y > MaxY)
				{
					return Y;
				}
			}
			else
			{
				Line = Candidate;
			}
		}

		if (!Line.IsEmpty())
		{
			DrawText(Line, Color, X, Y, Font, Scale);
			Y += LineH;
		}
	}
	return Y;
}

// ------------------------------------------------------------------ Journal screen

void AFBHUD::DrawJournalScreen()
{
	using namespace FBScreenStyle;

	const float S = GetUIScale();
	UFont* Font = GEngine->GetMediumFont();
	const float Margin = 90.f * S;
	const float X = Margin, Y = Margin;
	const float W = Canvas->ClipX - Margin * 2.f;
	const float H = Canvas->ClipY - Margin * 2.f;

	DrawRect(JournalBg, X, Y, W, H);

	// Header
	const AFBCaseFile* Case = AFBCaseFile::Find(this);
	const FString Header = Case
		? FString::Printf(TEXT("CASE JOURNAL  -  %s, %s"), *Case->GetCaseTitle().ToString().ToUpper(), *Case->GetTownName().ToString())
		: FString(TEXT("CASE JOURNAL"));
	DrawText(Header, JournalAccent, X + 30.f * S, Y + 24.f * S, Font, 1.3f * S);

	// Current objective under the header.
	if (const AFBMissionManager* Mission = AFBMissionManager::Find(this))
	{
		const FText Objective = Mission->GetCurrentObjectiveText();
		const FString Line = Objective.IsEmpty()
			? FString(TEXT("CASE CLOSED"))
			: FString::Printf(TEXT("OBJECTIVE:  %s"), *Objective.ToString());
		DrawText(Line, JournalText, X + 30.f * S, Y + 58.f * S, Font, 1.f * S);
	}
	DrawText(TEXT("[Up/Down] Browse     [J / Backspace] Close"), JournalDim, X + 30.f * S, Y + H - 40.f * S, Font, 0.9f * S);

	const UFBCaseJournalComponent* Journal = GetJournal();
	const TArray<FFBJournalEntry> Empty;
	const TArray<FFBJournalEntry>& Entries = Journal ? Journal->GetEntries() : Empty;

	const float ListX = X + 30.f * S;
	const float ListW = W * 0.34f;
	const float TopY = Y + 105.f * S;
	const float LineH = 30.f * S;
	const float BottomY = Y + H - 60.f * S;

	if (Entries.Num() == 0)
	{
		DrawText(TEXT("Nothing yet. Look around, collect evidence, use the archive computer."), JournalDim, ListX, TopY, Font, 1.f * S);
		return;
	}

	ScreenSelection = FMath::Clamp(ScreenSelection, 0, Entries.Num() - 1);

	// Left: entry list, newest first. Scrolls to keep the selection visible.
	const int32 MaxVisible = FMath::Max(1, FMath::FloorToInt((BottomY - TopY) / LineH));
	const int32 FirstVisible = FMath::Clamp(ScreenSelection - MaxVisible + 1, 0, FMath::Max(0, Entries.Num() - MaxVisible));

	for (int32 Row = 0; Row < MaxVisible; ++Row)
	{
		const int32 i = FirstVisible + Row; // index in "newest first" order
		if (i >= Entries.Num())
		{
			break;
		}
		const FFBJournalEntry& E = Entries[Entries.Num() - 1 - i];
		const float RowY = TopY + Row * LineH;

		if (i == ScreenSelection)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.08f), ListX - 8.f * S, RowY - 3.f * S, ListW, LineH);
		}

		const TCHAR* Tag = TEXT("NOTE");
		switch (E.Type)
		{
		case EFBJournalEntryType::Evidence:  Tag = TEXT("EVID"); break;
		case EFBJournalEntryType::Trace:     Tag = TEXT("TRACE"); break;
		case EFBJournalEntryType::Research:  Tag = TEXT("FILE"); break;
		case EFBJournalEntryType::Interview: Tag = TEXT("TALK"); break;
		default: break;
		}
		DrawText(Tag, JournalDim, ListX, RowY, Font, 0.8f * S);
		DrawText(E.Title.ToString(), i == ScreenSelection ? JournalText : JournalDim, ListX + 60.f * S, RowY, Font, 1.f * S);
	}

	// Right: selected entry.
	const FFBJournalEntry& Sel = Entries[Entries.Num() - 1 - ScreenSelection];
	const float DetailX = ListX + ListW + 30.f * S;
	const float DetailW = X + W - 30.f * S - DetailX;

	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), DetailX - 16.f * S, TopY, 1.f, BottomY - TopY);
	DrawText(Sel.Title.ToString(), JournalAccent, DetailX, TopY, Font, 1.15f * S);

	// Theories at the bottom part of the right column.
	TArray<FText> Theories;
	if (Case)
	{
		Case->GetUnlockedTheories(Journal, Theories);
	}
	const float TheoryTop = Theories.Num() > 0 ? BottomY - (40.f + 60.f * Theories.Num()) * S : BottomY;

	DrawWrappedText(Sel.Text.ToString(), DetailX, TopY + 40.f * S, DetailW, JournalText, 1.f * S, TheoryTop - 20.f * S);

	if (Theories.Num() > 0)
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), DetailX, TheoryTop - 10.f * S, DetailW, 1.f);
		DrawText(TEXT("THEORY"), JournalAccent, DetailX, TheoryTop, Font, 1.f * S);
		float TY = TheoryTop + 30.f * S;
		for (const FText& T : Theories)
		{
			TY = DrawWrappedText(FString(TEXT("- ")) + T.ToString(), DetailX, TY, DetailW, JournalText, 0.95f * S, BottomY);
		}
	}
}

// ------------------------------------------------------------------ Computer screen

void AFBHUD::DrawComputerScreen()
{
	using namespace FBScreenStyle;

	const float S = GetUIScale();
	UFont* Font = GEngine->GetMediumFont();
	const float Margin = 70.f * S;
	const float X = Margin, Y = Margin;
	const float W = Canvas->ClipX - Margin * 2.f;
	const float H = Canvas->ClipY - Margin * 2.f;

	DrawRect(TermBg, X, Y, W, H);
	// Scanlines for that CRT feel.
	for (float LineY = Y; LineY < Y + H; LineY += 4.f * S)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.18f), X, LineY, W, 1.f * S);
	}

	const AFBResearchComputer* Computer = ActiveComputer.Get();
	const FString Title = Computer ? Computer->GetTerminalName().ToString() : FString(TEXT("ARCHIVE TERMINAL"));
	DrawText(Title, TermText, X + 30.f * S, Y + 22.f * S, Font, 1.15f * S);
	DrawText(TEXT("[Up/Down] Select     [Enter / F] Search     [Backspace] Log off"), TermDim, X + 30.f * S, Y + H - 40.f * S, Font, 0.9f * S);

	const AFBCaseFile* Case = AFBCaseFile::Find(this);
	if (!Case)
	{
		DrawText(TEXT("NO ARCHIVE DATA. (Place an FBCaseFile in the level.)"), TermText, X + 30.f * S, Y + 90.f * S, Font, 1.f * S);
		return;
	}

	const UFBCaseJournalComponent* Journal = GetJournal();
	TArray<int32> Available;
	Case->GetAvailableArchive(Journal, Available);
	ScreenSelection = Available.Num() > 0 ? FMath::Clamp(ScreenSelection, 0, Available.Num() - 1) : 0;

	const TArray<FFBArchiveEntry>& Archive = Case->GetArchive();
	const float ListX = X + 30.f * S;
	const float ListW = W * 0.3f;
	const float TopY = Y + 80.f * S;
	const float LineH = 32.f * S;
	const float BottomY = Y + H - 60.f * S;

	// Left: search terms.
	DrawText(TEXT("SEARCH TERMS"), TermDim, ListX, TopY, Font, 0.9f * S);
	for (int32 i = 0; i < Available.Num(); ++i)
	{
		const FFBArchiveEntry& E = Archive[Available[i]];
		const float RowY = TopY + 34.f * S + i * LineH;
		if (RowY > BottomY)
		{
			break;
		}
		if (i == ScreenSelection)
		{
			DrawRect(TermHighlight, ListX - 8.f * S, RowY - 3.f * S, ListW, LineH);
		}
		const bool bRead = Journal && Journal->HasEntry(E.Id);
		DrawText(E.SearchTerm, bRead ? TermDim : TermText, ListX, RowY, Font, 1.f * S);
		if (!bRead)
		{
			float TW = 0.f, TH = 0.f;
			GetTextSize(TEXT("NEW"), TW, TH, Font, 0.75f * S);
			DrawText(TEXT("NEW"), TermText, ListX + ListW - 20.f * S - TW, RowY + 3.f * S, Font, 0.75f * S);
		}
	}

	// Right: search bar + result.
	const float RX = ListX + ListW + 30.f * S;
	const float RW = X + W - 30.f * S - RX;
	DrawRect(TermDim, RX - 16.f * S, TopY, 1.f, BottomY - TopY);

	// Search bar text: typing animation, or the last search.
	FString BarText;
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (TypingStartTime >= 0.f && Archive.IsValidIndex(ComputerPendingIndex))
	{
		const FString& Term = Archive[ComputerPendingIndex].SearchTerm;
		const int32 Chars = FMath::FloorToInt((Now - TypingStartTime) * TypeSpeed);
		BarText = Term.Left(FMath::Min(Chars, Term.Len()));
		if (Chars >= Term.Len() + 10) // Short pause = "searching..."
		{
			FinishComputerSearch();
		}
	}
	else if (Archive.IsValidIndex(ComputerReadingIndex))
	{
		BarText = Archive[ComputerReadingIndex].SearchTerm;
	}

	const bool bCursorOn = FMath::Fmod(Now, 1.f) < 0.55f;
	DrawRect(FLinearColor(0.45f, 1.f, 0.55f, 0.08f), RX, TopY, RW, 40.f * S);
	DrawText(FString::Printf(TEXT("SEARCH > %s%s"), *BarText, bCursorOn ? TEXT("_") : TEXT(" ")), TermText, RX + 12.f * S, TopY + 8.f * S, Font, 1.05f * S);

	const float ResultY = TopY + 60.f * S;
	if (TypingStartTime >= 0.f)
	{
		const int32 Dots = FMath::FloorToInt(Now * 3.f) % 4;
		DrawText(FString(TEXT("Searching")) + FString::ChrN(Dots, TEXT('.')), TermDim, RX, ResultY, Font, 1.f * S);
	}
	else if (Archive.IsValidIndex(ComputerReadingIndex))
	{
		const FFBArchiveEntry& E = Archive[ComputerReadingIndex];
		DrawText(E.Title.ToString().ToUpper(), TermText, RX, ResultY, Font, 1.15f * S);
		DrawText(E.Source.ToString(), TermDim, RX, ResultY + 32.f * S, Font, 0.9f * S);
		DrawWrappedText(E.Body.ToString(), RX, ResultY + 72.f * S, RW, TermText, 1.f * S, BottomY - 10.f * S);
	}
	else
	{
		DrawText(TEXT("Select a search term and press Enter."), TermDim, RX, ResultY, Font, 1.f * S);
	}
}
