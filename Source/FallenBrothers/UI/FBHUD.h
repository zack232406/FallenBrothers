// Fallen Brothers - HUD (pure C++, drawn with Canvas).
//
// Minimal bodycam HUD: REC indicator + timestamp, small crosshair dot,
// interaction prompt and short notifications. We'll move to UMG widgets in the UI step.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/FBMenu.h"
#include "FBHUD.generated.h"

class AFBResearchComputer;
class UFBCaseJournalComponent;
class AFBNPC;

/** Full-screen screens that take over input (journal, computer terminal). */
enum class EFBScreen : uint8
{
	None,
	Journal,
	Computer,
	Dialogue
};

UCLASS()
class FALLENBROTHERS_API AFBHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Show a short message at the bottom of the screen. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowNotification(const FText& Message, float Duration = 3.f);

	/** Big cinematic card in the middle of the screen (case intro, CASE CLOSED...). */
	void ShowTitleCard(const FText& Top, const FText& Title, const FText& Bottom, float Duration = 5.f);

	/** Current objective, shown top-right for a few seconds whenever it changes. Empty = clear. */
	void ShowObjective(const FText& Objective);

	/** Spoken line at the bottom of the screen, e.g. "ZACK: Behind you!" */
	void ShowSubtitle(const FText& Speaker, const FText& Line, float Duration = 4.f);

	/** Helper: subtitle on player 0's screen. */
	static void ShowSubtitleFor(const UObject* WorldContext, const FText& Speaker, const FText& Line, float Duration = 4.f);

	/** Helper: find the HUD of the player who owns ContextActor (a pawn) and show a message. */
	static void ShowNotificationFor(const AActor* ContextActor, const FText& Message, float Duration = 3.f);

	/** Tab: show / hide the inventory list. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleInventory() { bInventoryOpen = !bInventoryOpen; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	// ---------------------------------------------------------------- Screens (journal / computer)
	/** J: open / close the case journal. */
	void ToggleJournal();

	/** Open the town archive terminal of this computer. */
	void OpenComputer(AFBResearchComputer* Computer);

	/** Bodycam "SIGNAL LOST" screen when the player dies. */
	void ShowDeathScreen();

	/** Start talking to an NPC. */
	void OpenDialogue(AFBNPC* NPC);

	/** The NPC we're talking to (the player's camera turns to face them). */
	AFBNPC* GetDialogueNPC() const;

	void CloseScreen();

	/** While a screen (or the pause menu) is open the character ignores movement / look / actions. */
	bool IsScreenOpen() const { return ActiveScreen != EFBScreen::None || PauseMenu.IsOpen(); }

	// ---------------------------------------------------------------- Pause menu (FBHUD_Pause.cpp)
	/** Esc / P: pauses the game and shows the menu. */
	void OpenPauseMenu();
	void ClosePauseMenu();
	bool IsPauseMenuOpen() const { return PauseMenu.IsOpen(); }

	/** True for a moment after the pause menu closed (so clicking "Resume" doesn't also fire the shotgun). */
	bool WasMenuJustClosed() const;

	/** Left / Right arrows (settings values). */
	void AdjustUI(int32 Direction);

	/** Left mouse button while a menu is open. */
	void ClickUI();

	/** Up / Down arrows. */
	void NavigateUI(int32 Direction);

	/** Enter (or F). */
	void ConfirmUI();

	/** Backspace (and Esc / P while paused). */
	void BackUI();

	/** For equipment HUD drawing (Canvas itself is protected in UE 5.7). Only valid during DrawHUD. */
	bool HasCanvas() const { return Canvas != nullptr; }
	FVector2D GetScreenSize() const;

	// ---------------------------------------------------------------- Tuning
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowTimestamp = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowCrosshair = true;

	/** Camera label shown next to the timestamp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FString CameraLabel = TEXT("CAM 01");

	/** Key name shown in the prompt. Keep in sync with the interact key in FBPlayerCharacter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FString InteractKeyLabel = TEXT("F");

private:
	float GetUIScale() const;
	void DrawTimestamp();
	void DrawCrosshairAndPrompt();
	void DrawEquipment();
	void DrawInventory();
	void DrawHealthOverlay();
	void DrawDeathScreen();
	void DrawNotification();
	void DrawSubtitle();
	void DrawTitleCard();
	void DrawObjective();
	void DrawCenteredText(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale);

	// Screens (implemented in FBHUD_Screens.cpp)
	void DrawJournalScreen();
	void DrawComputerScreen();
	void FinishComputerSearch();

	// Dialogue (implemented in FBHUD_Dialogue.cpp)
	void DrawDialogueScreen();
	void ConfirmDialogue();
	int32 GetDialogueChoiceCount() const;
	bool IsDialogueLineFinished() const;
	void SetDialogueLine(const FText& Line);
	UFBCaseJournalComponent* GetJournal() const;
	/** Word-wrapped text. Returns the Y after the last line. Stops drawing past MaxY. */
	float DrawWrappedText(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, float Scale, float MaxY);

	FFBMenu PauseMenu;
	double MenuClosedTime = -100.0;

	bool bInventoryOpen = false;
	bool bDeathScreen = false;
	float DeathScreenTime = 0.f;

	EFBScreen ActiveScreen = EFBScreen::None;
	int32 ScreenSelection = 0;
	TWeakObjectPtr<AFBResearchComputer> ActiveComputer;
	int32 ComputerReadingIndex = INDEX_NONE;   // Archive entry currently shown
	int32 ComputerPendingIndex = INDEX_NONE;   // Archive entry being "typed"
	float TypingStartTime = -1.f;

	TWeakObjectPtr<AFBNPC> ActiveNPC;
	FText DialogueLine;               // What the NPC is currently saying
	float DialogueLineStartTime = 0.f;
	bool bDialogueEnding = false;      // Showing the farewell; next confirm closes
	float BadgeShownTime = -100.f;     // Chris holds up the fake FBI badge (dialogue)
	void DrawFBIBadge(float Alpha);

	FText TitleTop;
	FText TitleMain;
	FText TitleBottom;
	float TitleStartTime = -100.f;
	float TitleDuration = 0.f;

	FText ObjectiveText;
	float ObjectiveShownTime = -100.f;

	FText SubtitleSpeaker;
	FText SubtitleText;
	float SubtitleEndTime = 0.f;

	FText NotificationText;
	float NotificationEndTime = 0.f;
};
