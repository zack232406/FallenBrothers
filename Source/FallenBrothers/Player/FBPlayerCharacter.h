// Fallen Brothers - Player character (Chris).
//
// Movement + input. All camera "feel" lives in UFBBodycamComponent.
// Input (keys) is created in C++ in CreateDefaultInput() - no editor assets needed.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FBPlayerCharacter.generated.h"

class UCameraComponent;
class UFBBodycamComponent;
class UFBInteractionComponent;
class UFBEquipmentComponent;
class UFBInventoryComponent;
class UFBGrabComponent;
class UFBCaseJournalComponent;
class UFBHealthComponent;
class AFBHUD;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class FALLENBROTHERS_API AFBPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFBPlayerCharacter();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UFUNCTION(BlueprintPure, Category = "Movement")
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	UCameraComponent* GetBodycamCamera() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	UFBBodycamComponent* GetBodycam() const { return Bodycam; }

protected:
	// ---------------------------------------------------------------- Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> CameraRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBBodycamComponent> Bodycam;

	/** Finds what we look at and uses it (doors, evidence, NPCs...). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBInteractionComponent> Interaction;

	/** Flashlight, EMF reader, UV light (and weapons later). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBEquipmentComponent> Equipment;

	/** Keys, batteries, salt, iron... */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBInventoryComponent> Inventory;

	/** Pick up / throw / swing physics objects. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBGrabComponent> Grab;

	/** Clues, evidence, research notes. J to read. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBCaseJournalComponent> Journal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBHealthComponent> Health;

	/** Seconds after dying before the level restarts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RestartDelay = 5.f;

	// ---------------------------------------------------------------- Input
	// Created automatically in C++ if left empty.
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputMappingContext> DefaultMappingContext;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LeanLeftAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LeanRightAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> FlashlightAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> UseEquipmentAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> Slot1Action;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> Slot2Action;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> Slot3Action;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> Slot4Action;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> Slot5Action;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> QuickSaveAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> QuickLoadAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> CycleEquipmentAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> InventoryAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> GrabAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ThrowAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> JournalAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> UIUpAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> UIDownAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> UIConfirmAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> UIBackAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> UILeftAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> UIRightAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> UIClickAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> PauseAction;

	// ---------------------------------------------------------------- Movement tuning
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float WalkSpeed = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float SprintSpeed = 520.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float CrouchSpeed = 140.f;

	/** Sprint only when pushing forward at least this much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float SprintForwardThreshold = 0.5f;

	/** true = press C to toggle crouch, false = hold C. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") bool bToggleCrouch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float LookSensitivity = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") bool bInvertLookY = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float MinViewPitch = -80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float MaxViewPitch = 80.f;

private:
	/** Builds all input actions + key bindings in code (WASD, mouse, Space, Shift, C, Q, E). */
	void CreateDefaultInput();

	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void SprintPressed(const FInputActionValue& Value);
	void SprintReleased(const FInputActionValue& Value);
	void CrouchPressed(const FInputActionValue& Value);
	void CrouchReleased(const FInputActionValue& Value);
	void LeanLeftPressed(const FInputActionValue& Value)   { bLeanLeftHeld = true; }
	void LeanLeftReleased(const FInputActionValue& Value)  { bLeanLeftHeld = false; }
	void LeanRightPressed(const FInputActionValue& Value)  { bLeanRightHeld = true; }
	void LeanRightReleased(const FInputActionValue& Value) { bLeanRightHeld = false; }
	void InteractPressed(const FInputActionValue& Value);
	void FlashlightPressed(const FInputActionValue& Value);
	void UseEquipmentPressed(const FInputActionValue& Value);
	void Slot1Pressed(const FInputActionValue& Value);
	void Slot2Pressed(const FInputActionValue& Value);
	void Slot3Pressed(const FInputActionValue& Value);
	void CycleEquipment(const FInputActionValue& Value);
	void InventoryPressed(const FInputActionValue& Value);
	void ReloadPressed(const FInputActionValue& Value);
	void Slot4Pressed(const FInputActionValue& Value);
	void Slot5Pressed(const FInputActionValue& Value);
	void QuickSavePressed(const FInputActionValue& Value);
	void QuickLoadPressed(const FInputActionValue& Value);

	/** Footsteps make noise: crouch = almost silent, sprint = loud. */
	UFUNCTION()
	void HandleFootstep(float GroundSpeed);

	UFUNCTION()
	void HandleDeath();

	void RestartLevel();
	FTimerHandle RestartTimer;
	void GrabPressed(const FInputActionValue& Value);
	void ThrowPressed(const FInputActionValue& Value);
	void JumpPressed(const FInputActionValue& Value);
	void JournalPressed(const FInputActionValue& Value);
	void UIUpPressed(const FInputActionValue& Value);
	void UIDownPressed(const FInputActionValue& Value);
	void UIConfirmPressed(const FInputActionValue& Value);
	void UIBackPressed(const FInputActionValue& Value);
	void UILeftPressed(const FInputActionValue& Value);
	void UIRightPressed(const FInputActionValue& Value);
	void UIClickPressed(const FInputActionValue& Value);
	void PausePressed(const FInputActionValue& Value);

	/** Mouse sensitivity, invert Y, FOV, brightness, bodycam motion (from the Settings menu). */
	void ApplyUserSettings();
	FDelegateHandle SettingsChangedHandle;

	/** Our HUD (null if not a player). */
	AFBHUD* GetFBHUD() const;

	/** True while the journal / computer screen is open: movement and actions are ignored. */
	bool IsUIOpen() const;

	void UpdateSprint();

	FVector2D CurrentMoveInput = FVector2D::ZeroVector;
	bool bSprintHeld = false;
	bool bIsSprinting = false;
	bool bLeanLeftHeld = false;
	bool bLeanRightHeld = false;
};
