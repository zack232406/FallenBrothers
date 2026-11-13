// Fallen Brothers - Menu logic + drawing.

#include "UI/FBMenu.h"

#include "Core/FBProjectSettings.h"
#include "Core/FBSettingsSubsystem.h"
#include "Mission/FBCampaignSubsystem.h"
#include "Mission/FBMissionManager.h"
#include "Save/FBSaveSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace FBMenuStyle
{
	static const FLinearColor Amber(0.95f, 0.75f, 0.35f, 1.f);
	static const FLinearColor White(1.f, 1.f, 1.f, 0.9f);
	static const FLinearColor Soft(1.f, 1.f, 1.f, 0.55f);
	static const FLinearColor Dim(1.f, 1.f, 1.f, 0.22f);

	static const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	static const TCHAR* WindowNames[] = { TEXT("Fullscreen"), TEXT("Borderless"), TEXT("Windowed") };
}

// ------------------------------------------------------------------ Open / close

void FFBMenu::Open(EFBMenuPage InRootPage, APlayerController* InPC)
{
	PC = InPC;
	RootPage = InRootPage;
	OpenTime = FPlatformTime::Seconds();
	Status.Reset();
	LastMouse = FVector2D(-1.f, -1.f);
	RefreshSaveInfo();

	// Main menu: start on Continue if there is a save, otherwise on New Game.
	const int32 First = (InRootPage == EFBMenuPage::Main && !bHasSave) ? 1 : 0;
	GoTo(InRootPage, First);
}

void FFBMenu::Close()
{
	Page = EFBMenuPage::Closed;
	ItemRects.Reset();
	ValueCenters.Reset();
}

void FFBMenu::GoTo(EFBMenuPage NewPage, int32 NewSelection)
{
	Page = NewPage;
	Selection = NewSelection;
}

UWorld* FFBMenu::GetWorld() const
{
	return PC.IsValid() ? PC->GetWorld() : nullptr;
}

void FFBMenu::RefreshSaveInfo()
{
	const UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(GetWorld());
	bHasSave = Saves && Saves->HasSave();
	SaveSummary = bHasSave ? Saves->GetSaveSummary() : FString();
}

void FFBMenu::SetStatus(const FString& Text)
{
	Status = Text;
	StatusTime = FPlatformTime::Seconds();
}

// ------------------------------------------------------------------ Items

void FFBMenu::BuildItems(TArray<FItem>& Out) const
{
	Out.Reset();
	auto Add = [&Out](EAction Action, const TCHAR* Label, bool bEnabled = true) -> FItem&
	{
		FItem& Item = Out.AddDefaulted_GetRef();
		Item.Action = Action;
		Item.Label = Label;
		Item.bEnabled = bEnabled;
		return Item;
	};
	auto AddValue = [&Out](EAction Action, const TCHAR* Label, const FString& Value)
	{
		FItem& Item = Out.AddDefaulted_GetRef();
		Item.Action = Action;
		Item.Label = Label;
		Item.Value = Value;
		Item.bAdjustable = true;
	};

	switch (Page)
	{
	case EFBMenuPage::Main:
		Add(EAction::Continue, TEXT("Continue"), bHasSave).Value = SaveSummary;
		Add(EAction::NewGame, TEXT("New Game"));
		Add(EAction::Settings, TEXT("Settings"));
		Add(EAction::Quit, TEXT("Quit"));
		break;

	case EFBMenuPage::Pause:
		Add(EAction::Resume, TEXT("Resume"));
		Add(EAction::Save, TEXT("Save Game"));
		Add(EAction::LoadLast, TEXT("Load Last Save"), bHasSave);
		Add(EAction::Settings, TEXT("Settings"));
		Add(EAction::QuitToMenu, TEXT("Quit to Main Menu"));
		Add(EAction::Quit, TEXT("Quit to Desktop"));
		break;

	case EFBMenuPage::Settings:
	{
		const UFBSettingsSubsystem* SettingsSys = UFBSettingsSubsystem::Get(GetWorld());
		const FFBUserSettings S = SettingsSys ? SettingsSys->GetSettings() : FFBUserSettings();
		const int32 Quality = SettingsSys ? SettingsSys->GetQualityLevel() : 3;
		const int32 Window = SettingsSys ? SettingsSys->GetWindowMode() : 0;

		AddValue(EAction::Sensitivity, TEXT("Mouse Sensitivity"), FString::Printf(TEXT("%.1f"), S.MouseSensitivity));
		AddValue(EAction::InvertY, TEXT("Invert Mouse Y"), S.bInvertY ? TEXT("On") : TEXT("Off"));
		AddValue(EAction::CameraMotion, TEXT("Bodycam Motion"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S.CameraMotion * 100.f)));
		AddValue(EAction::FieldOfView, TEXT("Field of View"), FString::Printf(TEXT("%d"), FMath::RoundToInt(S.FieldOfView)));
		AddValue(EAction::Brightness, TEXT("Brightness"), FString::Printf(TEXT("%+.1f"), S.Brightness));
		AddValue(EAction::Quality, TEXT("Graphics Quality"), FBMenuStyle::QualityNames[FMath::Clamp(Quality, 0, 4)]);
		AddValue(EAction::WindowMode, TEXT("Display Mode"), FBMenuStyle::WindowNames[FMath::Clamp(Window, 0, 2)]);
		Add(EAction::Back, TEXT("Back"));
		break;
	}

	case EFBMenuPage::ConfirmNewGame:
	case EFBMenuPage::ConfirmLoad:
	case EFBMenuPage::ConfirmQuitToMenu:
		Add(EAction::Yes, TEXT("Yes"));
		Add(EAction::No, TEXT("No"));
		break;

	default:
		break;
	}
}

// ------------------------------------------------------------------ Input

void FFBMenu::Navigate(int32 Direction)
{
	TArray<FItem> Items;
	BuildItems(Items);
	if (Items.Num() == 0)
	{
		return;
	}

	// Step over disabled items (e.g. Continue without a save).
	int32 Index = Selection;
	for (int32 Tries = 0; Tries < Items.Num(); ++Tries)
	{
		Index = (Index + Direction + Items.Num()) % Items.Num();
		if (Items[Index].bEnabled)
		{
			Selection = Index;
			return;
		}
	}
}

void FFBMenu::Adjust(int32 Direction)
{
	TArray<FItem> Items;
	BuildItems(Items);
	if (Items.IsValidIndex(Selection) && Items[Selection].bAdjustable)
	{
		ChangeValue(Items[Selection].Action, Direction);
	}
}

void FFBMenu::Confirm()
{
	TArray<FItem> Items;
	BuildItems(Items);
	if (!Items.IsValidIndex(Selection) || !Items[Selection].bEnabled)
	{
		return;
	}

	if (Items[Selection].bAdjustable)
	{
		ChangeValue(Items[Selection].Action, 1); // Enter on a setting = next value.
		return;
	}
	Activate(Items[Selection].Action);
}

void FFBMenu::Back()
{
	switch (Page)
	{
	case EFBMenuPage::Settings:
		GoTo(RootPage, RootPage == EFBMenuPage::Main ? 2 : 3); // Back onto "Settings".
		break;
	case EFBMenuPage::ConfirmNewGame:
		GoTo(RootPage, 1);
		break;
	case EFBMenuPage::ConfirmLoad:
		GoTo(RootPage, 2);
		break;
	case EFBMenuPage::ConfirmQuitToMenu:
		GoTo(RootPage, 4);
		break;
	case EFBMenuPage::Pause:
		if (OnResumeRequested)
		{
			OnResumeRequested();
		}
		break;
	default:
		break; // Main menu: nowhere to go back to.
	}
}

int32 FFBMenu::FindItemUnderMouse(float& OutMouseX) const
{
	float MX = 0.f, MY = 0.f;
	if (!PC.IsValid() || !PC->GetMousePosition(MX, MY))
	{
		return INDEX_NONE;
	}
	OutMouseX = MX;
	for (int32 i = 0; i < ItemRects.Num(); ++i)
	{
		if (ItemRects[i].IsInside(FVector2D(MX, MY)))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void FFBMenu::Click()
{
	float MouseX = 0.f;
	const int32 Index = FindItemUnderMouse(MouseX);
	if (Index == INDEX_NONE)
	{
		return;
	}

	TArray<FItem> Items;
	BuildItems(Items);
	if (!Items.IsValidIndex(Index) || !Items[Index].bEnabled)
	{
		return;
	}

	Selection = Index;
	if (Items[Index].bAdjustable && ValueCenters.IsValidIndex(Index))
	{
		// Click the left arrow side to lower, the right side to raise.
		ChangeValue(Items[Index].Action, MouseX < ValueCenters[Index] ? -1 : 1);
		return;
	}
	Confirm();
}

// ------------------------------------------------------------------ Actions

void FFBMenu::Activate(EAction Action)
{
	UWorld* World = GetWorld();
	UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(World);

	switch (Action)
	{
	case EAction::Continue:
		if (Saves && bHasSave)
		{
			SetStatus(TEXT("Loading..."));
			Saves->LoadGame(World);
		}
		break;

	case EAction::NewGame:
		if (bHasSave)
		{
			GoTo(EFBMenuPage::ConfirmNewGame, 1); // Default to "No".
		}
		else
		{
			StartNewGame();
		}
		break;

	case EAction::Settings:
		GoTo(EFBMenuPage::Settings, 0);
		break;

	case EAction::Quit:
		UKismetSystemLibrary::QuitGame(World, PC.Get(), EQuitPreference::Quit, false);
		break;

	case EAction::Resume:
		if (OnResumeRequested)
		{
			OnResumeRequested();
		}
		break;

	case EAction::Save:
		if (Saves && Saves->SaveGame(World, /*bQuiet*/ true))
		{
			SetStatus(TEXT("Game saved."));
			RefreshSaveInfo();
		}
		else
		{
			SetStatus(TEXT("Can't save right now."));
		}
		break;

	case EAction::LoadLast:
		GoTo(EFBMenuPage::ConfirmLoad, 1);
		break;

	case EAction::QuitToMenu:
		GoTo(EFBMenuPage::ConfirmQuitToMenu, 1);
		break;

	case EAction::Back:
	case EAction::No:
		Back();
		break;

	case EAction::Yes:
		if (Page == EFBMenuPage::ConfirmNewGame)
		{
			StartNewGame();
		}
		else if (Page == EFBMenuPage::ConfirmLoad)
		{
			LoadLastSave();
		}
		else if (Page == EFBMenuPage::ConfirmQuitToMenu)
		{
			QuitToMainMenu();
		}
		break;

	default:
		break;
	}
}

void FFBMenu::ChangeValue(EAction Action, int32 Direction)
{
	UFBSettingsSubsystem* SettingsSys = UFBSettingsSubsystem::Get(GetWorld());
	if (!SettingsSys)
	{
		return;
	}

	FFBUserSettings S = SettingsSys->GetSettings();
	switch (Action)
	{
	case EAction::Sensitivity:  S.MouseSensitivity += 0.1f * Direction; break;
	case EAction::InvertY:      S.bInvertY = !S.bInvertY; break;
	case EAction::CameraMotion: S.CameraMotion += 0.1f * Direction; break;
	case EAction::FieldOfView:  S.FieldOfView += 5.f * Direction; break;
	case EAction::Brightness:   S.Brightness += 0.25f * Direction; break;

	case EAction::Quality:
		SettingsSys->SetQualityLevel((SettingsSys->GetQualityLevel() + Direction + 5) % 5);
		return;

	case EAction::WindowMode:
		SettingsSys->SetWindowMode((SettingsSys->GetWindowMode() + Direction + 3) % 3);
		if (GIsEditor)
		{
			SetStatus(TEXT("Display mode applies in the packaged game."));
		}
		return;

	default:
		return;
	}

	// Avoid float drift (0.30000001).
	S.MouseSensitivity = FMath::RoundToFloat(S.MouseSensitivity * 10.f) / 10.f;
	S.CameraMotion = FMath::RoundToFloat(S.CameraMotion * 10.f) / 10.f;
	SettingsSys->SetSettings(S);
}

void FFBMenu::Unpause()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
}

void FFBMenu::StartNewGame()
{
	UWorld* World = GetWorld();
	if (UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(World))
	{
		Saves->DeleteSave();
	}
	if (const UGameInstance* GI = World ? World->GetGameInstance() : nullptr)
	{
		if (UFBCampaignSubsystem* Campaign = GI->GetSubsystem<UFBCampaignSubsystem>())
		{
			Campaign->SetCompletedMissions(TSet<FName>());
		}
	}

	SetStatus(TEXT("Loading..."));
	Unpause();
	UFBProjectSettings::OpenFirstCase(World);
}

void FFBMenu::LoadLastSave()
{
	UWorld* World = GetWorld();
	Unpause();
	if (UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(World))
	{
		Saves->LoadGame(World);
	}
}

void FFBMenu::QuitToMainMenu()
{
	UWorld* World = GetWorld();
	Unpause();
	UFBProjectSettings::OpenMainMenu(World);
}

// ------------------------------------------------------------------ Drawing

void FFBMenu::Draw(AHUD* HUD, const FVector2D& Screen, float S)
{
	if (!IsOpen() || !HUD)
	{
		return;
	}

	UFont* Font = GEngine->GetMediumFont();
	const double Now = FPlatformTime::Seconds();
	const float FadeIn = FMath::Clamp(static_cast<float>(Now - OpenTime) / 0.4f, 0.f, 1.f);
	auto Fade = [FadeIn](FLinearColor C) { C.A *= FadeIn; return C; };

	const float X = 120.f * S;
	float Y = Screen.Y * 0.22f;

	// ---------------------------------------------------------------- Title
	struct FLine
	{
		FString Text;
		FLinearColor Color;
	};
	FString Title;
	TArray<FLine> SubLines;

	switch (Page)
	{
	case EFBMenuPage::Main:
		Title = TEXT("FALLEN BROTHERS");
		SubLines.Add({ TEXT("Two brothers. Thirteen towns. Nobody else is coming."), FBMenuStyle::Soft });
		break;

	case EFBMenuPage::Pause:
		Title = TEXT("PAUSED");
		if (const AFBMissionManager* Mission = AFBMissionManager::Find(GetWorld()))
		{
			SubLines.Add({ FString::Printf(TEXT("%s  -  %s"), *Mission->GetMissionTitle().ToString(), *Mission->GetTownName().ToString()), FBMenuStyle::Amber });
			const FString Objective = Mission->GetCurrentObjectiveText().ToString();
			if (!Objective.IsEmpty())
			{
				SubLines.Add({ TEXT("Objective: ") + Objective, FBMenuStyle::Soft });
			}
		}
		break;

	case EFBMenuPage::Settings:
		Title = TEXT("SETTINGS");
		break;

	case EFBMenuPage::ConfirmNewGame:
		Title = TEXT("START A NEW GAME?");
		SubLines.Add({ TEXT("Your saved progress will be deleted."), FBMenuStyle::Soft });
		break;

	case EFBMenuPage::ConfirmLoad:
		Title = TEXT("LOAD LAST SAVE?");
		SubLines.Add({ SaveSummary, FBMenuStyle::Amber });
		SubLines.Add({ TEXT("Anything since then will be lost."), FBMenuStyle::Soft });
		break;

	case EFBMenuPage::ConfirmQuitToMenu:
		Title = TEXT("QUIT TO MAIN MENU?");
		SubLines.Add({ TEXT("Anything since your last save will be lost."), FBMenuStyle::Soft });
		break;

	default:
		break;
	}

	HUD->DrawText(Title, Fade(FLinearColor(1.f, 1.f, 1.f, 0.95f)), X, Y, Font, 3.f * S);
	Y += 78.f * S;
	for (const FLine& Line : SubLines)
	{
		HUD->DrawText(Line.Text, Fade(Line.Color), X + 4.f * S, Y, Font, 1.05f * S);
		Y += 30.f * S;
	}

	// ---------------------------------------------------------------- Items
	TArray<FItem> Items;
	BuildItems(Items);
	Selection = Items.Num() > 0 ? FMath::Clamp(Selection, 0, Items.Num() - 1) : 0;

	Y = FMath::Max(Y + 40.f * S, Screen.Y * 0.42f);
	const float LineH = 50.f * S;
	const float RowW = 700.f * S;
	const float ValueCenterX = X + 520.f * S;

	// Rectangles first, so the mouse can pick the row this frame.
	ItemRects.SetNum(Items.Num());
	ValueCenters.SetNum(Items.Num());
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const float RowY = Y + i * LineH;
		ItemRects[i] = FBox2D(FVector2D(X - 24.f * S, RowY - 8.f * S), FVector2D(X + RowW, RowY + LineH - 10.f * S));
		ValueCenters[i] = ValueCenterX;
	}

	// Mouse hover selects (only when the mouse actually moved, so the keyboard still works).
	float MX = 0.f, MY = 0.f;
	if (PC.IsValid() && PC->GetMousePosition(MX, MY))
	{
		const FVector2D Mouse(MX, MY);
		if (!Mouse.Equals(LastMouse, 1.f))
		{
			LastMouse = Mouse;
			for (int32 i = 0; i < Items.Num(); ++i)
			{
				if (Items[i].bEnabled && ItemRects[i].IsInside(Mouse))
				{
					Selection = i;
					break;
				}
			}
		}
	}

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FItem& Item = Items[i];
		const float RowY = Y + i * LineH;
		const bool bSelected = (i == Selection) && Item.bEnabled;

		if (bSelected)
		{
			const FBox2D& R = ItemRects[i];
			HUD->DrawRect(Fade(FLinearColor(0.95f, 0.75f, 0.35f, 0.10f)), R.Min.X, R.Min.Y, R.GetSize().X, R.GetSize().Y);
			HUD->DrawRect(Fade(FBMenuStyle::Amber), R.Min.X, R.Min.Y, 4.f * S, R.GetSize().Y);
		}

		const FLinearColor LabelColor = !Item.bEnabled ? FBMenuStyle::Dim : (bSelected ? FBMenuStyle::Amber : FBMenuStyle::White);
		HUD->DrawText(Item.Label, Fade(LabelColor), X, RowY, Font, 1.35f * S);

		if (Item.bAdjustable)
		{
			const FString Value = bSelected ? FString::Printf(TEXT("<   %s   >"), *Item.Value) : Item.Value;
			float VW = 0.f, VH = 0.f;
			HUD->GetTextSize(Value, VW, VH, Font, 1.2f * S);
			HUD->DrawText(Value, Fade(bSelected ? FBMenuStyle::White : FBMenuStyle::Soft), ValueCenterX - VW * 0.5f, RowY + 2.f * S, Font, 1.2f * S);
		}
		else if (!Item.Value.IsEmpty())
		{
			// Extra info, e.g. the save on "Continue".
			float LW = 0.f, LH = 0.f;
			HUD->GetTextSize(Item.Label, LW, LH, Font, 1.35f * S);
			HUD->DrawText(Item.Value, Fade(FBMenuStyle::Soft), X + LW + 24.f * S, RowY + 6.f * S, Font, 0.95f * S);
		}
	}

	// ---------------------------------------------------------------- Status + key hints
	const float AfterItems = Y + Items.Num() * LineH + 20.f * S;
	const float StatusAge = static_cast<float>(Now - StatusTime);
	if (!Status.IsEmpty() && StatusAge < 3.f)
	{
		FLinearColor C = FBMenuStyle::Amber;
		C.A = FMath::Clamp((3.f - StatusAge) / 0.5f, 0.f, 1.f);
		HUD->DrawText(Status, C, X, AfterItems, Font, 1.1f * S);
	}

	const FString Hints = (Page == EFBMenuPage::Settings)
		? TEXT("W/S  select      A/D  change      BACKSPACE  back")
		: TEXT("W/S  select      ENTER  confirm      BACKSPACE  back");
	HUD->DrawText(Hints, Fade(FLinearColor(1.f, 1.f, 1.f, 0.35f)), X, Screen.Y - 70.f * S, Font, 0.9f * S);
}
