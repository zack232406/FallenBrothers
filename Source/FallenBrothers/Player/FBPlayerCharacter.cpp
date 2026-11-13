// Fallen Brothers - Player character (Chris).

#include "Player/FBPlayerCharacter.h"

#include "Town/FBFootsteps.h"
#include "Core/FBRealModels.h"

#include "Player/FBBodycamComponent.h"
#include "Interaction/FBInteractionComponent.h"
#include "Equipment/FBEquipmentComponent.h"
#include "Inventory/FBInventoryComponent.h"
#include "Physics/FBGrabComponent.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Dialogue/FBNPC.h"
#include "Combat/FBHealthComponent.h"
#include "Core/FBNoiseSubsystem.h"
#include "Save/FBSaveSubsystem.h"
#include "Core/FBSettingsSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/FBHUD.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

AFBPlayerCharacter::AFBPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// ~1.84 m tall person.
	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);

	// Body turns with the mouse left/right; up/down only moves the camera.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Eye heights from capsule centre. Standing ~158 cm, crouched ~93 cm from the floor.
	BaseEyeHeight = 66.f;
	CrouchedEyeHeight = 38.f;

	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	CameraRoot->SetupAttachment(GetCapsuleComponent());
	CameraRoot->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("BodycamCamera"));
	Camera->SetupAttachment(CameraRoot);
	Camera->bUsePawnControlRotation = false; // Bodycam component rotates the root itself.

	Bodycam = CreateDefaultSubobject<UFBBodycamComponent>(TEXT("Bodycam"));
	Interaction = CreateDefaultSubobject<UFBInteractionComponent>(TEXT("Interaction"));
	Equipment = CreateDefaultSubobject<UFBEquipmentComponent>(TEXT("Equipment"));
	Inventory = CreateDefaultSubobject<UFBInventoryComponent>(TEXT("Inventory"));
	Grab = CreateDefaultSubobject<UFBGrabComponent>(TEXT("Grab"));
	Journal = CreateDefaultSubobject<UFBCaseJournalComponent>(TEXT("Journal"));
	Health = CreateDefaultSubobject<UFBHealthComponent>(TEXT("Health"));

	GetMesh()->SetOwnerNoSee(true); // No body mesh yet.

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(55.f);
	Move->bCanWalkOffLedgesWhenCrouching = true;
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->MaxAcceleration = 1400.f;
	Move->BrakingDecelerationWalking = 1600.f;
	Move->GroundFriction = 7.f;
	Move->JumpZVelocity = 380.f;
	Move->AirControl = 0.15f;
}

void AFBPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Chris's real body (if imported): you don't see it from the bodycam, but it casts your shadow.
	FBRealModels::DressCharacter(this, GetCapsuleComponent(), TEXT("char_chris"), 180.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), true);
	Health->OnDeath.AddDynamic(this, &AFBPlayerCharacter::HandleDeath);
	Bodycam->OnFootstep.AddDynamic(this, &AFBPlayerCharacter::HandleFootstep);

	// Settings menu values (and re-apply whenever they change).
	ApplyUserSettings();
	if (UFBSettingsSubsystem* SettingsSys = UFBSettingsSubsystem::Get(this))
	{
		SettingsChangedHandle = SettingsSys->OnSettingsChanged.AddUObject(this, &AFBPlayerCharacter::ApplyUserSettings);
	}

	// One tick later (everything in the level has started): apply a pending load, if any.
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(this))
		{
			Saves->OnLevelStarted(GetWorld());
		}
	}));
}

void AFBPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFBSettingsSubsystem* SettingsSys = UFBSettingsSubsystem::Get(this))
	{
		SettingsSys->OnSettingsChanged.Remove(SettingsChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AFBPlayerCharacter::ApplyUserSettings()
{
	const UFBSettingsSubsystem* SettingsSys = UFBSettingsSubsystem::Get(this);
	if (!SettingsSys)
	{
		return;
	}
	const FFBUserSettings& S = SettingsSys->GetSettings();
	LookSensitivity = S.MouseSensitivity;
	bInvertLookY = S.bInvertY;
	Bodycam->ApplyUserSettings(S.FieldOfView, S.Brightness, S.CameraMotion);
}

void AFBPlayerCharacter::HandleDeath()
{
	// Drop everything, stop moving, camera slumps.
	Grab->Drop();
	GetCharacterMovement()->DisableMovement();
	Bodycam->AddKick(FRotator(-25.f, 10.f, 35.f));

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		DisableInput(PC);
	}
	if (AFBHUD* HUD = GetFBHUD())
	{
		HUD->CloseScreen();
		HUD->ShowDeathScreen();
	}

	GetWorldTimerManager().SetTimer(RestartTimer, this, &AFBPlayerCharacter::RestartLevel, RestartDelay, false);
}

void AFBPlayerCharacter::RestartLevel()
{
	// Back to the last save if there is one, otherwise restart the level.
	UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(this);
	if (Saves && Saves->HasSave() && Saves->LoadGame(this))
	{
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void AFBPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Runs before the player possesses us, so the input exists in time.
	CreateDefaultInput();
}

// ------------------------------------------------------------------ Input creation (no editor assets)

void AFBPlayerCharacter::CreateDefaultInput()
{
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type) -> UInputAction*
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
		Action->ValueType = Type;
		return Action;
	};

	if (!MoveAction)      MoveAction      = MakeAction(TEXT("IA_Move"),      EInputActionValueType::Axis2D);
	if (!LookAction)      LookAction      = MakeAction(TEXT("IA_Look"),      EInputActionValueType::Axis2D);
	if (!JumpAction)      JumpAction      = MakeAction(TEXT("IA_Jump"),      EInputActionValueType::Boolean);
	if (!SprintAction)    SprintAction    = MakeAction(TEXT("IA_Sprint"),    EInputActionValueType::Boolean);
	if (!CrouchAction)    CrouchAction    = MakeAction(TEXT("IA_Crouch"),    EInputActionValueType::Boolean);
	if (!LeanLeftAction)  LeanLeftAction  = MakeAction(TEXT("IA_LeanLeft"),  EInputActionValueType::Boolean);
	if (!LeanRightAction) LeanRightAction = MakeAction(TEXT("IA_LeanRight"), EInputActionValueType::Boolean);
	if (!InteractAction)  InteractAction  = MakeAction(TEXT("IA_Interact"),  EInputActionValueType::Boolean);
	if (!FlashlightAction)     FlashlightAction     = MakeAction(TEXT("IA_Flashlight"),     EInputActionValueType::Boolean);
	if (!UseEquipmentAction)   UseEquipmentAction   = MakeAction(TEXT("IA_UseEquipment"),   EInputActionValueType::Boolean);
	if (!Slot1Action)          Slot1Action          = MakeAction(TEXT("IA_Slot1"),          EInputActionValueType::Boolean);
	if (!Slot2Action)          Slot2Action          = MakeAction(TEXT("IA_Slot2"),          EInputActionValueType::Boolean);
	if (!Slot3Action)          Slot3Action          = MakeAction(TEXT("IA_Slot3"),          EInputActionValueType::Boolean);
	if (!Slot4Action)          Slot4Action          = MakeAction(TEXT("IA_Slot4"),          EInputActionValueType::Boolean);
	if (!Slot5Action)          Slot5Action          = MakeAction(TEXT("IA_Slot5"),          EInputActionValueType::Boolean);
	if (!QuickSaveAction)      QuickSaveAction      = MakeAction(TEXT("IA_QuickSave"),      EInputActionValueType::Boolean);
	if (!QuickLoadAction)      QuickLoadAction      = MakeAction(TEXT("IA_QuickLoad"),      EInputActionValueType::Boolean);
	if (!CycleEquipmentAction) CycleEquipmentAction = MakeAction(TEXT("IA_CycleEquipment"), EInputActionValueType::Axis1D);
	if (!InventoryAction)      InventoryAction      = MakeAction(TEXT("IA_Inventory"),      EInputActionValueType::Boolean);
	if (!ReloadAction)         ReloadAction         = MakeAction(TEXT("IA_Reload"),         EInputActionValueType::Boolean);
	if (!GrabAction)           GrabAction           = MakeAction(TEXT("IA_Grab"),           EInputActionValueType::Boolean);
	if (!ThrowAction)          ThrowAction          = MakeAction(TEXT("IA_Throw"),          EInputActionValueType::Boolean);
	if (!JournalAction)        JournalAction        = MakeAction(TEXT("IA_Journal"),        EInputActionValueType::Boolean);
	if (!UIUpAction)           UIUpAction           = MakeAction(TEXT("IA_UIUp"),           EInputActionValueType::Boolean);
	if (!UIDownAction)         UIDownAction         = MakeAction(TEXT("IA_UIDown"),         EInputActionValueType::Boolean);
	if (!UIConfirmAction)      UIConfirmAction      = MakeAction(TEXT("IA_UIConfirm"),      EInputActionValueType::Boolean);
	if (!UIBackAction)         UIBackAction         = MakeAction(TEXT("IA_UIBack"),         EInputActionValueType::Boolean);
	if (!UILeftAction)         UILeftAction         = MakeAction(TEXT("IA_UILeft"),         EInputActionValueType::Boolean);
	if (!UIRightAction)        UIRightAction        = MakeAction(TEXT("IA_UIRight"),        EInputActionValueType::Boolean);
	if (!UIClickAction)        UIClickAction        = MakeAction(TEXT("IA_UIClick"),        EInputActionValueType::Boolean);
	if (!PauseAction)          PauseAction          = MakeAction(TEXT("IA_Pause"),          EInputActionValueType::Boolean);

	// These keep working while the game is paused (pause menu).
	for (UInputAction* Action : { UIUpAction.Get(), UIDownAction.Get(), UIConfirmAction.Get(), UIBackAction.Get(),
	                              UILeftAction.Get(), UIRightAction.Get(), UIClickAction.Get(), PauseAction.Get(), InteractAction.Get() })
	{
		Action->bTriggerWhenPaused = true;
	}

	if (DefaultMappingContext)
	{
		return; // Someone assigned one manually - use it.
	}

	UInputMappingContext* IMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));

	// Helpers for the modifiers WASD needs.
	auto AddSwizzle = [IMC](FEnhancedActionKeyMapping& Mapping)
	{
		// Turns a 1D key press (X) into the Y axis = forward/back.
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(IMC));
	};
	auto AddNegate = [IMC](FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(IMC));
	};

	// W = forward, S = back, D = right, A = left
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(MoveAction, EKeys::W); AddSwizzle(M); }
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(MoveAction, EKeys::S); AddSwizzle(M); AddNegate(M); }
	IMC->MapKey(MoveAction, EKeys::D);
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(MoveAction, EKeys::A); AddNegate(M); }

	// Mouse look
	IMC->MapKey(LookAction, EKeys::Mouse2D);

	IMC->MapKey(JumpAction, EKeys::SpaceBar);
	IMC->MapKey(SprintAction, EKeys::LeftShift);
	IMC->MapKey(CrouchAction, EKeys::C);
	IMC->MapKey(CrouchAction, EKeys::LeftControl);
	IMC->MapKey(LeanLeftAction, EKeys::Q);
	IMC->MapKey(LeanRightAction, EKeys::E);
	// Interact = F (Q/E are lean). Change the key here, and InteractKeyLabel in FBHUD.h.
	IMC->MapKey(InteractAction, EKeys::F);

	// Equipment
	IMC->MapKey(FlashlightAction, EKeys::T);
	IMC->MapKey(UseEquipmentAction, EKeys::LeftMouseButton);
	IMC->MapKey(Slot1Action, EKeys::One);
	IMC->MapKey(Slot2Action, EKeys::Two);
	IMC->MapKey(Slot3Action, EKeys::Three);
	IMC->MapKey(Slot4Action, EKeys::Four);
	IMC->MapKey(Slot5Action, EKeys::Five);

	// Save / load
	IMC->MapKey(QuickSaveAction, EKeys::F5);
	IMC->MapKey(QuickLoadAction, EKeys::F9);
	IMC->MapKey(CycleEquipmentAction, EKeys::MouseWheelAxis);

	// Inventory
	IMC->MapKey(InventoryAction, EKeys::Tab);
	IMC->MapKey(ReloadAction, EKeys::R);

	// Physics
	IMC->MapKey(GrabAction, EKeys::RightMouseButton);
	IMC->MapKey(ThrowAction, EKeys::G);

	// Journal + screen navigation
	IMC->MapKey(JournalAction, EKeys::J);
	IMC->MapKey(UIUpAction, EKeys::Up);
	IMC->MapKey(UIDownAction, EKeys::Down);
	IMC->MapKey(UIConfirmAction, EKeys::Enter);
	IMC->MapKey(UIBackAction, EKeys::BackSpace);
	IMC->MapKey(UIUpAction, EKeys::W);
	IMC->MapKey(UIDownAction, EKeys::S);
	IMC->MapKey(UILeftAction, EKeys::Left);
	IMC->MapKey(UILeftAction, EKeys::A);
	IMC->MapKey(UIRightAction, EKeys::Right);
	IMC->MapKey(UIRightAction, EKeys::D);
	IMC->MapKey(UIClickAction, EKeys::LeftMouseButton);

	// Pause: Esc in the packaged game. In the editor Esc stops Play, so P works too.
	IMC->MapKey(PauseAction, EKeys::Escape);
	IMC->MapKey(PauseAction, EKeys::P);

	DefaultMappingContext = IMC;
}

void AFBPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}

		// The engine's development shortcuts use F1-F5 / F9 (F5 = shader complexity view,
		// F9 = screenshot). They clash with quick save / quick load, so remove them for our player.
		if (PC->PlayerInput)
		{
			PC->PlayerInput->DebugExecBindings.RemoveAll([](const FKeyBind& Bind)
			{
				return Bind.Key == EKeys::F1 || Bind.Key == EKeys::F2 || Bind.Key == EKeys::F3 ||
				       Bind.Key == EKeys::F4 || Bind.Key == EKeys::F5 || Bind.Key == EKeys::F9;
			});
		}

		// Coming from the main menu (mouse cursor on) -> back to normal game controls.
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());

		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = MinViewPitch;
			PC->PlayerCameraManager->ViewPitchMax = MaxViewPitch;
		}
	}
}

void AFBPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("Enhanced Input component not found. Project Settings > Input > Default Input Component Class must be EnhancedInputComponent."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &ThisClass::StopMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::JumpPressed);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::SprintPressed);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::SprintReleased);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::CrouchPressed);
	Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ThisClass::CrouchReleased);
	Input->BindAction(LeanLeftAction, ETriggerEvent::Started, this, &ThisClass::LeanLeftPressed);
	Input->BindAction(LeanLeftAction, ETriggerEvent::Completed, this, &ThisClass::LeanLeftReleased);
	Input->BindAction(LeanRightAction, ETriggerEvent::Started, this, &ThisClass::LeanRightPressed);
	Input->BindAction(LeanRightAction, ETriggerEvent::Completed, this, &ThisClass::LeanRightReleased);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::InteractPressed);
	Input->BindAction(FlashlightAction, ETriggerEvent::Started, this, &ThisClass::FlashlightPressed);
	Input->BindAction(UseEquipmentAction, ETriggerEvent::Started, this, &ThisClass::UseEquipmentPressed);
	Input->BindAction(Slot1Action, ETriggerEvent::Started, this, &ThisClass::Slot1Pressed);
	Input->BindAction(Slot2Action, ETriggerEvent::Started, this, &ThisClass::Slot2Pressed);
	Input->BindAction(Slot3Action, ETriggerEvent::Started, this, &ThisClass::Slot3Pressed);
	Input->BindAction(Slot4Action, ETriggerEvent::Started, this, &ThisClass::Slot4Pressed);
	Input->BindAction(Slot5Action, ETriggerEvent::Started, this, &ThisClass::Slot5Pressed);
	Input->BindAction(QuickSaveAction, ETriggerEvent::Started, this, &ThisClass::QuickSavePressed);
	Input->BindAction(QuickLoadAction, ETriggerEvent::Started, this, &ThisClass::QuickLoadPressed);
	Input->BindAction(CycleEquipmentAction, ETriggerEvent::Started, this, &ThisClass::CycleEquipment);
	Input->BindAction(InventoryAction, ETriggerEvent::Started, this, &ThisClass::InventoryPressed);
	Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &ThisClass::ReloadPressed);
	Input->BindAction(GrabAction, ETriggerEvent::Started, this, &ThisClass::GrabPressed);
	Input->BindAction(ThrowAction, ETriggerEvent::Started, this, &ThisClass::ThrowPressed);
	Input->BindAction(JournalAction, ETriggerEvent::Started, this, &ThisClass::JournalPressed);
	Input->BindAction(UIUpAction, ETriggerEvent::Started, this, &ThisClass::UIUpPressed);
	Input->BindAction(UIDownAction, ETriggerEvent::Started, this, &ThisClass::UIDownPressed);
	Input->BindAction(UIConfirmAction, ETriggerEvent::Started, this, &ThisClass::UIConfirmPressed);
	Input->BindAction(UIBackAction, ETriggerEvent::Started, this, &ThisClass::UIBackPressed);
	Input->BindAction(UILeftAction, ETriggerEvent::Started, this, &ThisClass::UILeftPressed);
	Input->BindAction(UIRightAction, ETriggerEvent::Started, this, &ThisClass::UIRightPressed);
	Input->BindAction(UIClickAction, ETriggerEvent::Started, this, &ThisClass::UIClickPressed);
	Input->BindAction(PauseAction, ETriggerEvent::Started, this, &ThisClass::PausePressed);
}

AFBHUD* AFBPlayerCharacter::GetFBHUD() const
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	return PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr;
}

bool AFBPlayerCharacter::IsUIOpen() const
{
	const AFBHUD* HUD = GetFBHUD();
	return HUD && HUD->IsScreenOpen();
}

void AFBPlayerCharacter::JumpPressed(const FInputActionValue& Value)
{
	if (!IsUIOpen())
	{
		Jump();
	}
}

void AFBPlayerCharacter::JournalPressed(const FInputActionValue& Value)
{
	if (AFBHUD* HUD = GetFBHUD())
	{
		HUD->ToggleJournal();
	}
}

void AFBPlayerCharacter::UIUpPressed(const FInputActionValue& Value)      { if (AFBHUD* HUD = GetFBHUD()) HUD->NavigateUI(-1); }
void AFBPlayerCharacter::UIDownPressed(const FInputActionValue& Value)    { if (AFBHUD* HUD = GetFBHUD()) HUD->NavigateUI(1); }
void AFBPlayerCharacter::UIConfirmPressed(const FInputActionValue& Value) { if (AFBHUD* HUD = GetFBHUD()) HUD->ConfirmUI(); }
void AFBPlayerCharacter::UIBackPressed(const FInputActionValue& Value)    { if (AFBHUD* HUD = GetFBHUD()) HUD->BackUI(); }
void AFBPlayerCharacter::UILeftPressed(const FInputActionValue& Value)    { if (AFBHUD* HUD = GetFBHUD()) HUD->AdjustUI(-1); }
void AFBPlayerCharacter::UIRightPressed(const FInputActionValue& Value)   { if (AFBHUD* HUD = GetFBHUD()) HUD->AdjustUI(1); }
void AFBPlayerCharacter::UIClickPressed(const FInputActionValue& Value)   { if (AFBHUD* HUD = GetFBHUD()) HUD->ClickUI(); }

void AFBPlayerCharacter::PausePressed(const FInputActionValue& Value)
{
	AFBHUD* HUD = GetFBHUD();
	if (!HUD || Health->IsDead())
	{
		return;
	}

	if (HUD->IsPauseMenuOpen())
	{
		HUD->BackUI();        // Esc in the menu = back / resume
	}
	else if (HUD->IsScreenOpen())
	{
		HUD->CloseScreen();   // Esc closes the journal / computer / conversation first
	}
	else
	{
		HUD->OpenPauseMenu();
	}
}

void AFBPlayerCharacter::GrabPressed(const FInputActionValue& Value)
{
	if (IsUIOpen())
	{
		return;
	}

	if (Grab->IsHolding())
	{
		Grab->Drop();
	}
	else if (Grab->TryGrab())
	{
		Equipment->Holster(); // Hands are busy now.
	}
}

void AFBPlayerCharacter::ThrowPressed(const FInputActionValue& Value)
{
	if (IsUIOpen())
	{
		return;
	}

	Grab->Throw();
}

void AFBPlayerCharacter::InventoryPressed(const FInputActionValue& Value)
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (AFBHUD* HUD = PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ToggleInventory();
	}
}

void AFBPlayerCharacter::ReloadPressed(const FInputActionValue& Value)
{
	if (IsUIOpen())
	{
		return;
	}
	Equipment->TryReload(Inventory);
}

void AFBPlayerCharacter::FlashlightPressed(const FInputActionValue& Value)   { Equipment->ToggleFlashlight(); }
void AFBPlayerCharacter::UseEquipmentPressed(const FInputActionValue& Value)
{
	const AFBHUD* HUD = GetFBHUD();
	if (IsUIOpen() || (HUD && HUD->WasMenuJustClosed()))
	{
		return;
	}

	// Holding something? Left click swings / throws it instead.
	if (Grab->IsHolding())
	{
		Grab->PrimaryAction();
		return;
	}
	Equipment->UseActiveEquipment();
}
void AFBPlayerCharacter::Slot1Pressed(const FInputActionValue& Value)        { Equipment->EquipSlot(0); }
void AFBPlayerCharacter::Slot2Pressed(const FInputActionValue& Value)        { Equipment->EquipSlot(1); }
void AFBPlayerCharacter::Slot3Pressed(const FInputActionValue& Value)        { Equipment->EquipSlot(2); }
void AFBPlayerCharacter::Slot4Pressed(const FInputActionValue& Value)        { Equipment->EquipSlot(3); }
void AFBPlayerCharacter::Slot5Pressed(const FInputActionValue& Value)        { Equipment->EquipSlot(4); }

void AFBPlayerCharacter::QuickSavePressed(const FInputActionValue& Value)
{
	if (UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(this))
	{
		Saves->SaveGame(this);
	}
}

void AFBPlayerCharacter::QuickLoadPressed(const FInputActionValue& Value)
{
	if (UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(this))
	{
		Saves->LoadGame(this);
	}
}

void AFBPlayerCharacter::HandleFootstep(float GroundSpeed)
{
	const float Loudness = bIsCrouched ? 0.1f : (bIsSprinting ? 1.0f : 0.4f);
	// The sound of the step: what's underfoot, splashing on wet streets.
	FBFootsteps::Play(this, GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
		bIsCrouched ? 0.25f : (bIsSprinting ? 0.95f : 0.6f));
	UFBNoiseSubsystem::ReportNoise(this, GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Loudness, this);
}

void AFBPlayerCharacter::CycleEquipment(const FInputActionValue& Value)
{
	const float Dir = Value.Get<float>();
	Equipment->CycleEquipment(Dir > 0.f ? -1 : 1); // Wheel up = previous, wheel down = next.
}

void AFBPlayerCharacter::InteractPressed(const FInputActionValue& Value)
{
	if (IsUIOpen())
	{
		if (AFBHUD* HUD = GetFBHUD())
		{
			HUD->ConfirmUI();
		}
		return;
	}
	Interaction->TryInteract();
}

void AFBPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateSprint();

	const float LeanDir = bIsSprinting ? 0.f : (bLeanRightHeld ? 1.f : 0.f) - (bLeanLeftHeld ? 1.f : 0.f);
	Bodycam->SetLeanInput(LeanDir);

	// While talking, the camera slowly turns to look the NPC in the face.
	if (const AFBHUD* HUD = GetFBHUD())
	{
		if (const AFBNPC* NPC = HUD->GetDialogueNPC())
		{
			if (Controller)
			{
				const FRotator Target = (NPC->GetHeadLocation() - Camera->GetComponentLocation()).Rotation();
				Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Target, DeltaSeconds, 3.f));
			}
		}
	}
}

// ------------------------------------------------------------------ Input handlers

void AFBPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (IsUIOpen())
	{
		return;
	}

	CurrentMoveInput = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), CurrentMoveInput.Y);
	AddMovementInput(GetActorRightVector(), CurrentMoveInput.X);
}

void AFBPlayerCharacter::StopMove(const FInputActionValue& Value)
{
	CurrentMoveInput = FVector2D::ZeroVector;
}

void AFBPlayerCharacter::Look(const FInputActionValue& Value)
{
	if (IsUIOpen())
	{
		return;
	}

	const FVector2D LookInput = Value.Get<FVector2D>() * LookSensitivity;

	AddControllerYawInput(LookInput.X);
	// Invert Y is a Settings option.
	AddControllerPitchInput(bInvertLookY ? LookInput.Y : -LookInput.Y);

	Bodycam->AddLookInput(LookInput);
}

void AFBPlayerCharacter::SprintPressed(const FInputActionValue& Value)
{
	bSprintHeld = true;
	if (bIsCrouched)
	{
		UnCrouch();
	}
}

void AFBPlayerCharacter::SprintReleased(const FInputActionValue& Value)
{
	bSprintHeld = false;
}

void AFBPlayerCharacter::CrouchPressed(const FInputActionValue& Value)
{
	if (bToggleCrouch && bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AFBPlayerCharacter::CrouchReleased(const FInputActionValue& Value)
{
	if (!bToggleCrouch)
	{
		UnCrouch();
	}
}

// ------------------------------------------------------------------ Crouch / sprint

void AFBPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	Bodycam->OnCapsuleHeightChanged(ScaledHalfHeightAdjust);
}

void AFBPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	Bodycam->OnCapsuleHeightChanged(-ScaledHalfHeightAdjust);
}

void AFBPlayerCharacter::UpdateSprint()
{
	bIsSprinting = bSprintHeld && !bIsCrouched && CurrentMoveInput.Y >= SprintForwardThreshold;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
}
