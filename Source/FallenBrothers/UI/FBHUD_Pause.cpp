// Fallen Brothers - HUD: pause menu.

#include "UI/FBHUD.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void AFBHUD::OpenPauseMenu()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || bDeathScreen || PauseMenu.IsOpen())
	{
		return;
	}

	CloseScreen();
	bInventoryOpen = false;

	UGameplayStatics::SetGamePaused(this, true);

	PC->bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);

	PauseMenu.OnResumeRequested = [this]() { ClosePauseMenu(); };
	PauseMenu.Open(EFBMenuPage::Pause, PC);
}

void AFBHUD::ClosePauseMenu()
{
	if (!PauseMenu.IsOpen())
	{
		return;
	}

	PauseMenu.Close();
	MenuClosedTime = FPlatformTime::Seconds();
	UGameplayStatics::SetGamePaused(this, false);

	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

bool AFBHUD::WasMenuJustClosed() const
{
	return FPlatformTime::Seconds() - MenuClosedTime < 0.3;
}

void AFBHUD::BackUI()
{
	if (PauseMenu.IsOpen())
	{
		PauseMenu.Back();
		return;
	}
	CloseScreen();
}

void AFBHUD::AdjustUI(int32 Direction)
{
	if (PauseMenu.IsOpen())
	{
		PauseMenu.Adjust(Direction);
	}
}

void AFBHUD::ClickUI()
{
	if (PauseMenu.IsOpen())
	{
		PauseMenu.Click();
	}
}
