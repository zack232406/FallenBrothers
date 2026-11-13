// Fallen Brothers - Main menu game mode + player controller.

#include "Core/FBMenuGameMode.h"

#include "UI/FBMenuHUD.h"

#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"

AFBMenuGameMode::AFBMenuGameMode()
{
	DefaultPawnClass = nullptr; // Nobody walks around in the menu.
	PlayerControllerClass = AFBMenuPlayerController::StaticClass();
	HUDClass = AFBMenuHUD::StaticClass();
}

// ------------------------------------------------------------------ Controller

void AFBMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Mouse cursor on, keyboard still works.
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	// Look through the first camera placed in the map (the menu "shot").
	// Again one tick later, in case the engine resets the view after spawning us.
	UseMenuCamera();
	GetWorldTimerManager().SetTimerForNextTick(this, &AFBMenuPlayerController::UseMenuCamera);
}

void AFBMenuPlayerController::UseMenuCamera()
{
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		SetViewTarget(*It);
		return;
	}
}

void AFBMenuPlayerController::CreateMenuInput()
{
	if (MenuContext)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name) -> UInputAction*
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
		Action->ValueType = EInputActionValueType::Boolean;
		Action->bTriggerWhenPaused = true;
		return Action;
	};

	UpAction      = MakeAction(TEXT("IA_MenuUp"));
	DownAction    = MakeAction(TEXT("IA_MenuDown"));
	LeftAction    = MakeAction(TEXT("IA_MenuLeft"));
	RightAction   = MakeAction(TEXT("IA_MenuRight"));
	ConfirmAction = MakeAction(TEXT("IA_MenuConfirm"));
	BackAction    = MakeAction(TEXT("IA_MenuBack"));
	ClickAction   = MakeAction(TEXT("IA_MenuClick"));

	MenuContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Menu"));
	MenuContext->MapKey(UpAction, EKeys::Up);
	MenuContext->MapKey(UpAction, EKeys::W);
	MenuContext->MapKey(DownAction, EKeys::Down);
	MenuContext->MapKey(DownAction, EKeys::S);
	MenuContext->MapKey(LeftAction, EKeys::Left);
	MenuContext->MapKey(LeftAction, EKeys::A);
	MenuContext->MapKey(RightAction, EKeys::Right);
	MenuContext->MapKey(RightAction, EKeys::D);
	MenuContext->MapKey(ConfirmAction, EKeys::Enter);
	MenuContext->MapKey(ConfirmAction, EKeys::SpaceBar);
	MenuContext->MapKey(ConfirmAction, EKeys::F);
	MenuContext->MapKey(BackAction, EKeys::BackSpace);
	MenuContext->MapKey(BackAction, EKeys::Escape);
	MenuContext->MapKey(ClickAction, EKeys::LeftMouseButton);
}

void AFBMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateMenuInput();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();
		Subsystem->AddMappingContext(MenuContext, 0);
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("Menu: Enhanced Input component missing (Project Settings > Input > Default Input Component Class)."));
		return;
	}

	Input->BindAction(UpAction, ETriggerEvent::Started, this, &AFBMenuPlayerController::OnUp);
	Input->BindAction(DownAction, ETriggerEvent::Started, this, &AFBMenuPlayerController::OnDown);
	Input->BindAction(LeftAction, ETriggerEvent::Started, this, &AFBMenuPlayerController::OnLeft);
	Input->BindAction(RightAction, ETriggerEvent::Started, this, &AFBMenuPlayerController::OnRight);
	Input->BindAction(ConfirmAction, ETriggerEvent::Started, this, &AFBMenuPlayerController::OnConfirm);
	Input->BindAction(BackAction, ETriggerEvent::Started, this, &AFBMenuPlayerController::OnBack);
	Input->BindAction(ClickAction, ETriggerEvent::Started, this, &AFBMenuPlayerController::OnClick);
}

FFBMenu* AFBMenuPlayerController::GetMenu() const
{
	AFBMenuHUD* HUD = Cast<AFBMenuHUD>(GetHUD());
	return HUD ? &HUD->GetMenu() : nullptr;
}
