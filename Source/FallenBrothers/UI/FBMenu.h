// Fallen Brothers - Menu logic + drawing (main menu, pause menu, settings).
//
// Plain C++ class (not a UObject), owned by a HUD:
//   AFBMenuHUD -> main menu (in the MainMenu map)
//   AFBHUD     -> pause menu (Esc / P in game)
// Keyboard: W/S or arrows select, A/D or arrows change values, Enter/F confirm, Backspace back.
// Mouse: hover selects, click confirms.

#pragma once

#include "CoreMinimal.h"

class AHUD;
class APlayerController;
class UWorld;

enum class EFBMenuPage : uint8
{
	Closed,
	Main,
	Pause,
	Settings,
	ConfirmNewGame,
	ConfirmLoad,
	ConfirmQuitToMenu
};

class FALLENBROTHERS_API FFBMenu
{
public:
	/** Show the menu starting at RootPage (Main or Pause). */
	void Open(EFBMenuPage InRootPage, APlayerController* InPC);
	void Close();

	bool IsOpen() const { return Page != EFBMenuPage::Closed; }

	void Navigate(int32 Direction);
	void Adjust(int32 Direction);
	void Confirm();
	/** Back one page. On the pause menu's first page this resumes the game. */
	void Back();
	/** Left mouse button. */
	void Click();

	void Draw(AHUD* HUD, const FVector2D& Screen, float Scale);

	/** The pause menu asks its HUD to unpause. */
	TFunction<void()> OnResumeRequested;

private:
	enum class EAction : uint8
	{
		Continue, NewGame, Settings, Quit,
		Resume, Save, LoadLast, QuitToMenu,
		Sensitivity, InvertY, CameraMotion, FieldOfView, Brightness, Quality, WindowMode,
		Back, Yes, No
	};

	struct FItem
	{
		EAction Action = EAction::Back;
		FString Label;
		FString Value;
		bool bEnabled = true;
		bool bAdjustable = false;
	};

	void BuildItems(TArray<FItem>& Out) const;
	void Activate(EAction Action);
	void ChangeValue(EAction Action, int32 Direction);
	void GoTo(EFBMenuPage NewPage, int32 NewSelection = 0);
	void SetStatus(const FString& Text);
	void RefreshSaveInfo();
	int32 FindItemUnderMouse(float& OutMouseX) const;

	void StartNewGame();
	void LoadLastSave();
	void QuitToMainMenu();
	void Unpause();

	UWorld* GetWorld() const;

	TWeakObjectPtr<APlayerController> PC;
	EFBMenuPage Page = EFBMenuPage::Closed;
	EFBMenuPage RootPage = EFBMenuPage::Closed;
	int32 Selection = 0;

	bool bHasSave = false;
	FString SaveSummary;

	FString Status;
	double StatusTime = -100.0;
	double OpenTime = 0.0;

	// Filled while drawing, used for mouse hover / clicks.
	TArray<FBox2D> ItemRects;
	TArray<float> ValueCenters;
	FVector2D LastMouse = FVector2D(-1.f, -1.f);
};
