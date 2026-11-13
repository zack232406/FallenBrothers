// Fallen Brothers - HUD dialogue screen (NPC interviews).
// Part of AFBHUD.

#include "UI/FBHUD.h"
#include "Inventory/FBInventoryComponent.h"
#include "GameFramework/Pawn.h"

#include "Dialogue/FBNPC.h"
#include "Investigation/FBCaseJournalComponent.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace FBDialogueStyle
{
	const FLinearColor PanelBg(0.f, 0.f, 0.f, 0.78f);
	const FLinearColor Name(0.95f, 0.75f, 0.35f, 1.f);
	const FLinearColor Text(0.95f, 0.93f, 0.88f, 0.95f);
	const FLinearColor Dim(0.95f, 0.93f, 0.88f, 0.4f);
	const FLinearColor Highlight(1.f, 1.f, 1.f, 0.1f);

	constexpr float CharsPerSecond = 45.f;
}

// ------------------------------------------------------------------ State

void AFBHUD::OpenDialogue(AFBNPC* NPC)
{
	if (!NPC)
	{
		return;
	}

	ActiveScreen = EFBScreen::Dialogue;
	ActiveNPC = NPC;
	ScreenSelection = 0;
	bDialogueEnding = false;
	bInventoryOpen = false;

	NPC->BeginConversation(GetOwningPawn());
	SetDialogueLine(NPC->GetGreeting());
}

AFBNPC* AFBHUD::GetDialogueNPC() const
{
	return ActiveScreen == EFBScreen::Dialogue ? ActiveNPC.Get() : nullptr;
}

void AFBHUD::SetDialogueLine(const FText& Line)
{
	DialogueLine = Line;
	DialogueLineStartTime = GetWorld()->GetRealTimeSeconds();
}

bool AFBHUD::IsDialogueLineFinished() const
{
	const float Elapsed = GetWorld()->GetRealTimeSeconds() - DialogueLineStartTime;
	return Elapsed * FBDialogueStyle::CharsPerSecond >= DialogueLine.ToString().Len();
}

int32 AFBHUD::GetDialogueChoiceCount() const
{
	const AFBNPC* NPC = ActiveNPC.Get();
	if (!NPC || bDialogueEnding)
	{
		return 0;
	}
	TArray<int32> Available;
	NPC->GetAvailableOptions(GetJournal(), Available);
	return Available.Num() + 1; // + "Goodbye"
}

void AFBHUD::ConfirmDialogue()
{
	AFBNPC* NPC = ActiveNPC.Get();
	if (!NPC)
	{
		CloseScreen();
		return;
	}

	// Still typing? Show the whole line instead.
	if (!IsDialogueLineFinished())
	{
		DialogueLineStartTime = -10000.f;
		return;
	}

	if (bDialogueEnding)
	{
		CloseScreen();
		return;
	}

	UFBCaseJournalComponent* Journal = GetJournal();
	TArray<int32> Available;
	NPC->GetAvailableOptions(Journal, Available);

	// Last choice = goodbye.
	if (ScreenSelection >= Available.Num())
	{
		bDialogueEnding = true;
		SetDialogueLine(NPC->GetFarewell());
		return;
	}

	const FFBDialogueOption& Option = NPC->GetOptions()[Available[ScreenSelection]];
	SetDialogueLine(Option.NpcResponse);

	if (!NPC->WasAsked(Option.Id))
	{
		NPC->MarkAsked(Option.Id);

		if (Journal)
		{
			if (!Option.JournalNote.IsEmpty())
			{
				FFBJournalEntry Entry;
				Entry.Id = Option.Id;
				Entry.Title = FText::Format(NSLOCTEXT("FB", "TalkEntryTitle", "{0}: {1}"), NPC->GetNPCName(), Option.PlayerLine);
				Entry.Text = Option.JournalNote;
				Entry.Type = EFBJournalEntryType::Interview;
				Journal->AddEntry(Entry);
			}
			Journal->AddClues(Option.GrantsClues);
		}
		if (!Option.GrantsItemId.IsNone())
		{
			const APawn* Pawn = GetOwningPawn();
			if (UFBInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UFBInventoryComponent>() : nullptr)
			{
				FFBInventoryItem Item;
				Item.ItemId = Option.GrantsItemId;
				Item.DisplayName = Option.GrantsItemName.IsEmpty() ? FText::FromName(Option.GrantsItemId) : Option.GrantsItemName;
				Item.Description = Option.GrantsItemDescription;
				Item.Category = EFBItemCategory::Key;
				Item.Quantity = 1;
				Item.bStackable = false;
				if (!Inventory->HasItem(Item.ItemId) && Inventory->AddItem(Item) > 0)
				{
					ShowNotification(FText::Format(NSLOCTEXT("FB", "DialogueGotItem", "Received: {0}"), Item.DisplayName), 4.f);
				}
			}
		}
	}
	if (Option.bShowsBadge)
	{
		BadgeShownTime = GetWorld()->GetRealTimeSeconds();
	}
}

void AFBHUD::DrawFBIBadge(float Alpha)
{
	// A fake FBI credential wallet held up to the camera: leather fold, gold shield on the left, ID card on the right.
	const float S = GetUIScale();
	UFont* Font = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float W = 520.f * S, H = 300.f * S;
	const float X0 = Canvas->ClipX * 0.5f - W * 0.5f;
	const float Y0 = Canvas->ClipY * 0.16f;
	auto C = [Alpha](float R, float G, float B, float A = 1.f) { return FLinearColor(R, G, B, A * Alpha); };
	DrawRect(C(0.07f, 0.045f, 0.03f), X0 - 10.f * S, Y0 - 10.f * S, W + 20.f * S, H + 20.f * S); // leather
	DrawRect(C(0.11f, 0.07f, 0.045f), X0, Y0, W * 0.5f - 4.f * S, H);
	// Shield (gold) on the left half.
	const float Sx = X0 + W * 0.25f, Sy = Y0 + H * 0.5f;
	DrawRect(C(0.62f, 0.48f, 0.2f), Sx - 70.f * S, Sy - 90.f * S, 140.f * S, 150.f * S);
	DrawRect(C(0.75f, 0.6f, 0.28f), Sx - 58.f * S, Sy - 78.f * S, 116.f * S, 126.f * S);
	DrawRect(C(0.62f, 0.48f, 0.2f), Sx - 40.f * S, Sy + 60.f * S, 80.f * S, 22.f * S);
	DrawText(TEXT("F B I"), C(0.25f, 0.17f, 0.05f), Sx - 36.f * S, Sy - 30.f * S, Font, 1.5f * S);
	DrawText(TEXT("SPECIAL AGENT"), C(0.3f, 0.21f, 0.07f), Sx - 50.f * S, Sy + 18.f * S, Small, 1.f * S);
	// ID card on the right half.
	const float Cx = X0 + W * 0.5f + 4.f * S;
	DrawRect(C(0.9f, 0.9f, 0.86f), Cx, Y0, W * 0.5f - 4.f * S, H);
	DrawRect(C(0.12f, 0.2f, 0.42f), Cx, Y0, W * 0.5f - 4.f * S, 46.f * S);
	DrawText(TEXT("FEDERAL BUREAU OF INVESTIGATION"), C(0.95f, 0.95f, 0.95f), Cx + 10.f * S, Y0 + 14.f * S, Small, 0.95f * S);
	DrawRect(C(0.55f, 0.55f, 0.55f), Cx + 14.f * S, Y0 + 62.f * S, 92.f * S, 116.f * S); // photo
	DrawRect(C(0.35f, 0.3f, 0.27f), Cx + 36.f * S, Y0 + 78.f * S, 48.f * S, 56.f * S);
	DrawText(TEXT("HARLAN, C."), C(0.08f, 0.08f, 0.1f), Cx + 118.f * S, Y0 + 70.f * S, Font, 0.9f * S);
	DrawText(TEXT("Special Agent"), C(0.2f, 0.2f, 0.25f), Cx + 118.f * S, Y0 + 100.f * S, Small, 1.f * S);
	DrawText(TEXT("No. 4471-0938"), C(0.2f, 0.2f, 0.25f), Cx + 118.f * S, Y0 + 124.f * S, Small, 1.f * S);
	DrawText(TEXT("Expires 12/2009"), C(0.2f, 0.2f, 0.25f), Cx + 118.f * S, Y0 + 148.f * S, Small, 1.f * S);
	DrawRect(C(0.12f, 0.2f, 0.42f), Cx, Y0 + H - 30.f * S, W * 0.5f - 4.f * S, 30.f * S);
	DrawText(TEXT("U.S. DEPARTMENT OF JUSTICE"), C(0.9f, 0.9f, 0.9f), Cx + 12.f * S, Y0 + H - 23.f * S, Small, 0.9f * S);
}

// ------------------------------------------------------------------ Drawing

void AFBHUD::DrawDialogueScreen()
{
	using namespace FBDialogueStyle;

	AFBNPC* NPC = ActiveNPC.Get();
	if (!NPC)
	{
		CloseScreen();
		return;
	}

	const float S = GetUIScale();
	UFont* Font = GEngine->GetMediumFont();
	const float SW = Canvas->ClipX;
	const float SH = Canvas->ClipY;

	// Cinematic letterbox bars.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f), 0.f, 0.f, SW, SH * 0.06f);

	// The badge, held up for a few seconds when Chris shows it.
	{
		const float Since = GetWorld()->GetRealTimeSeconds() - BadgeShownTime;
		if (Since >= 0.f && Since < 5.f)
		{
			const float A = FMath::Clamp(Since / 0.25f, 0.f, 1.f) * FMath::Clamp((5.f - Since) / 0.6f, 0.f, 1.f);
			DrawFBIBadge(A);
		}
	}

	// Bottom panel.
	const float PanelH = SH * 0.40f;
	const float PanelY = SH - PanelH;
	DrawRect(PanelBg, 0.f, PanelY, SW, PanelH);

	const float X = SW * 0.12f;
	const float W = SW * 0.76f;
	float Y = PanelY + 24.f * S;

	// Name + role
	DrawText(NPC->GetNPCName().ToString().ToUpper(), Name, X, Y, Font, 1.25f * S);
	float NameW = 0.f, NameH = 0.f;
	GetTextSize(NPC->GetNPCName().ToString().ToUpper(), NameW, NameH, Font, 1.25f * S);
	DrawText(NPC->GetRole().ToString(), Dim, X + NameW + 16.f * S, Y + 5.f * S, Font, 0.9f * S);
	Y += 44.f * S;

	// Spoken line with typewriter effect.
	const FString Full = DialogueLine.ToString();
	const float Elapsed = GetWorld()->GetRealTimeSeconds() - DialogueLineStartTime;
	const int32 Shown = FMath::Clamp(FMath::FloorToInt(Elapsed * CharsPerSecond), 0, Full.Len());
	const float OptionsTop = SH - 30.f * S - 34.f * S * 6.f;
	Y = DrawWrappedText(FString::Printf(TEXT("\"%s\""), *Full.Left(Shown)), X, Y, W, Text, 1.05f * S, OptionsTop - 10.f * S);

	const float HintY = SH - 34.f * S;
	if (Shown < Full.Len())
	{
		DrawText(TEXT("[Enter / F] Skip"), Dim, X, HintY, Font, 0.85f * S);
		return;
	}

	if (bDialogueEnding)
	{
		DrawText(TEXT("[Enter / F] Leave"), Dim, X, HintY, Font, 0.85f * S);
		return;
	}

	// Choices.
	TArray<int32> Available;
	NPC->GetAvailableOptions(GetJournal(), Available);
	const int32 Count = Available.Num() + 1;
	ScreenSelection = FMath::Clamp(ScreenSelection, 0, Count - 1);

	float OY = FMath::Max(Y + 14.f * S, OptionsTop);
	const float LineH = 34.f * S;
	for (int32 i = 0; i < Count; ++i)
	{
		const bool bGoodbye = (i == Available.Num());
		FString Label;
		bool bAsked = false;
		if (bGoodbye)
		{
			Label = TEXT("Thanks for your time.");
		}
		else
		{
			const FFBDialogueOption& O = NPC->GetOptions()[Available[i]];
			Label = O.PlayerLine.ToString();
			bAsked = NPC->WasAsked(O.Id);
		}

		if (i == ScreenSelection)
		{
			DrawRect(Highlight, X - 10.f * S, OY - 4.f * S, W, LineH);
		}
		const FLinearColor Color = (i == ScreenSelection) ? Text : (bAsked || bGoodbye ? Dim : FLinearColor(Text.R, Text.G, Text.B, 0.75f));
		DrawText(FString::Printf(TEXT("%d.  %s"), i + 1, *Label), Color, X, OY, Font, 1.f * S);
		OY += LineH;
	}

	DrawText(TEXT("[Up/Down] Choose     [Enter / F] Ask     [Backspace] Leave"), Dim, X, HintY, Font, 0.85f * S);
}
