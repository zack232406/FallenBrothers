// Fallen Brothers - Bodycam camera behaviour.
//
// Makes the first-person camera feel like a head-mounted bodycam:
// smooth crouch height, head bob synced to footsteps, idle breathing, look sway,
// leaning with wall collision, landing impact, procedural handheld shake that
// gets stronger with speed, and the bodycam post-process look.
//
// 100% C++ - no Blueprint or editor assets needed.
// Owner must be an ACharacter with:  Capsule -> CameraRoot (SceneComponent) -> CameraComponent

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "FBBodycamComponent.generated.h"

class ACharacter;
class UCameraComponent;
class UMaterialInterface;

/** Fired on every footfall. Later: footstep sounds + noise that creatures can hear. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBodycamFootstep, float, GroundSpeed);

UCLASS(ClassGroup = (FallenBrothers), meta = (BlueprintSpawnableComponent))
class FALLENBROTHERS_API UFBBodycamComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFBBodycamComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Raw look input from the player this frame (used for camera sway). */
	void AddLookInput(const FVector2D& LookInput);

	/** Sudden camera jolt (recoil, getting hit). Decays back to zero. */
	void AddKick(const FRotator& Kick) { KickRotation += Kick; }

	/** -1 = lean left, 0 = upright, 1 = lean right. */
	void SetLeanInput(float Direction);

	/** Called by the character on crouch (+) / uncrouch (-) so the camera glides instead of snapping. */
	void OnCapsuleHeightChanged(float HalfHeightAdjust);

	/** Applies the post-process values below to the camera. Called automatically in BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Bodycam|Post Process")
	void ApplyPostProcessDefaults();

	/** Player settings: field of view, brightness (exposure stops) and motion strength (0 = steady camera). */
	void ApplyUserSettings(float InFieldOfView, float InBrightness, float InMotionScale);

	UFUNCTION(BlueprintPure, Category = "Bodycam")
	float GetLeanAlpha() const { return LeanAlpha; }

	UFUNCTION(BlueprintPure, Category = "Bodycam")
	UCameraComponent* GetCamera() const { return Camera; }

	UPROPERTY(BlueprintAssignable, Category = "Bodycam")
	FOnBodycamFootstep OnFootstep;

	// ================================================================
	// TUNING - change these numbers here in C++ and recompile.
	// ================================================================

	// ---------------------------------------------------------------- Mount
	/** Camera position relative to the eyes. A real bodycam sits a bit forward of the face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mount")
	FVector MountOffset = FVector(8.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mount")
	float EyeHeightInterpSpeed = 10.f;

	// ---------------------------------------------------------------- Head bob
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Head Bob")
	bool bEnableHeadBob = true;

	/** Distance (cm) of one full bob cycle = two steps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Head Bob")
	float StrideLength = 150.f;

	/** Ground speed where bob intensity = 1 (your walk speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Head Bob")
	float BobReferenceSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Head Bob")
	float BobMaxIntensity = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Head Bob")
	float BobVertical = 1.4f;      // cm dip per step

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Head Bob")
	float BobHorizontal = 0.9f;    // cm side sway

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Head Bob")
	float BobRoll = 0.6f;          // degrees

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Head Bob")
	float BobBlendSpeed = 6.f;

	// ---------------------------------------------------------------- Breathing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Breathing")
	float BreathAmplitude = 0.35f; // cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Breathing")
	float BreathFrequency = 0.25f; // breaths per second

	// ---------------------------------------------------------------- Handheld shake (procedural)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Shake")
	bool bEnableHandheldShake = true;

	/** Rotation jitter in degrees when standing still / at full sprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Shake")
	float IdleShakeAngle = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Shake")
	float SprintShakeAngle = 1.2f;

	/** How fast the jitter moves when idle / sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Shake")
	float IdleShakeFrequency = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Shake")
	float SprintShakeFrequency = 2.5f;

	/** Ground speed that counts as "full sprint" for the shake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Shake")
	float ShakeReferenceSpeed = 500.f;

	// ---------------------------------------------------------------- Look sway
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Look Sway")
	float LookSwayRoll = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Look Sway")
	float LookSwayLag = 0.004f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Look Sway")
	float MaxLookSwayRoll = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Look Sway")
	float MaxLookSwayLag = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Look Sway")
	float LookSwayInterpSpeed = 6.f;

	// ---------------------------------------------------------------- Lean
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Lean")
	float LeanDistance = 32.f;     // cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Lean")
	float LeanAngle = 12.f;        // degrees

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Lean")
	float LeanInterpSpeed = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Lean")
	float LeanProbeRadius = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Lean")
	TEnumAsByte<ECollisionChannel> LeanCollisionChannel = ECC_Visibility;

	// ---------------------------------------------------------------- Landing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Landing")
	float LandImpulseScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Landing")
	float LandMaxImpulse = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Landing")
	float LandSpringStiffness = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Landing")
	float LandSpringDamping = 14.f;

	// ---------------------------------------------------------------- Post process
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	bool bApplyPostProcessOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float FieldOfView = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float ChromaticAberrationIntensity = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float VignetteIntensity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float FilmGrainIntensity = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float MotionBlurAmount = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float MotionBlurMax = 6.f;

	/** Lowest exposure (EV100) the camera may use. Higher = dark places stay darker. Try -1..2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float ExposureMinEV = -1.2f; // a moonlit street stays readable (eyes adapt), dark rooms still need the flashlight

	/** Highest exposure (EV100). Keep high so daylight doesn't blow out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float ExposureMaxEV = 20.f;

	/** How fast the camera adapts to light changes (like a real camera, a bit slow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	float ExposureAdaptSpeed = 1.5f;

	/** Optional fisheye post-process material. Leave empty for now - we'll make it later. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Post Process")
	TObjectPtr<UMaterialInterface> LensDistortionMaterial;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateEyeHeight(float DeltaTime);
	void UpdateLanding(float DeltaTime, bool bFalling, float VerticalVelocity);
	void UpdateHeadBob(float DeltaTime, float GroundSpeed, bool bOnGround);
	void UpdateHandheldShake(float DeltaTime, float GroundSpeed, bool bOnGround);
	void UpdateLookSway(float DeltaTime);
	void UpdateLean(float DeltaTime);
	void ApplyCameraTransform();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CameraRoot;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

	// Runtime state
	float CurrentEyeHeight = 0.f;
	float LastOwnerZ = 0.f;        // to smooth curb / stair steps (the capsule pops up, the head shouldn't)
	bool bWasOnGround = false;

	float BobPhase = 0.f;
	float BobIntensity = 0.f;
	FVector BobOffset = FVector::ZeroVector;
	float BobRollCurrent = 0.f;
	float BreathTime = 0.f;

	float ShakeTime = 0.f;
	float ShakeAngleCurrent = 0.f;
	FRotator ShakeRotation = FRotator::ZeroRotator;

	FVector2D PendingLookInput = FVector2D::ZeroVector;
	FRotator KickRotation = FRotator::ZeroRotator;
	FRotator LookSway = FRotator::ZeroRotator;

	/** Scales head bob, breathing, handheld shake and look sway (settings: "Bodycam Motion"). */
	float MotionScale = 1.f;

	float LeanInput = 0.f;
	float LeanAlpha = 0.f;
	float CurrentLeanOffset = 0.f;

	float LandOffset = 0.f;
	float LandSpringVelocity = 0.f;
	float LastFallSpeed = 0.f;
	bool bWasFalling = false;
};
