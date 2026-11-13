// Fallen Brothers - Bodycam camera behaviour.

#include "Player/FBBodycamComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

UFBBodycamComponent::UFBBodycamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Run after movement so the camera uses this frame's final position/velocity.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UFBBodycamComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("FBBodycamComponent: owner is not a Character."));
		SetComponentTickEnabled(false);
		return;
	}

	Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
	CameraRoot = Camera ? Camera->GetAttachParent() : nullptr;
	if (!Camera || !CameraRoot)
	{
		UE_LOG(LogTemp, Error, TEXT("FBBodycamComponent: needs a Camera attached to a CameraRoot."));
		SetComponentTickEnabled(false);
		return;
	}

	CurrentEyeHeight = OwnerCharacter->BaseEyeHeight;
	ShakeAngleCurrent = IdleShakeAngle;

	if (bApplyPostProcessOnBeginPlay)
	{
		ApplyPostProcessDefaults();
	}
}

void UFBBodycamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerCharacter || !Camera || !CameraRoot)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	const FVector Velocity = OwnerCharacter->GetVelocity();
	const float GroundSpeed = Velocity.Size2D();
	const bool bOnGround = Movement && Movement->IsMovingOnGround();
	const bool bFalling = Movement && Movement->IsFalling();

	// Stepping up a curb or a stair: the capsule jumps up in one frame. Keep the head where it was and let it
	// follow over a fraction of a second, like a real step (same for stepping down without falling).
	{
		const float OwnerZ = OwnerCharacter->GetActorLocation().Z;
		const float Dz = OwnerZ - LastOwnerZ;
		if (bOnGround && bWasOnGround && FMath::Abs(Dz) > 2.f && FMath::Abs(Dz) < 50.f
			&& FMath::Abs(Dz) > FMath::Abs(Velocity.Z * DeltaTime) + 1.5f)
		{
			CurrentEyeHeight -= Dz;
		}
		LastOwnerZ = OwnerZ;
		bWasOnGround = bOnGround;
	}
	UpdateEyeHeight(DeltaTime);
	UpdateLanding(DeltaTime, bFalling, Velocity.Z);
	UpdateHeadBob(DeltaTime, GroundSpeed, bOnGround);
	UpdateHandheldShake(DeltaTime, GroundSpeed, bOnGround);
	UpdateLookSway(DeltaTime);
	UpdateLean(DeltaTime);
	KickRotation = FMath::RInterpTo(KickRotation, FRotator::ZeroRotator, DeltaTime, 9.f);

	ApplyCameraTransform();
}

// ------------------------------------------------------------------ Public API

void UFBBodycamComponent::AddLookInput(const FVector2D& LookInput)
{
	PendingLookInput += LookInput;
}

void UFBBodycamComponent::SetLeanInput(float Direction)
{
	LeanInput = FMath::Clamp(Direction, -1.f, 1.f);
}

void UFBBodycamComponent::OnCapsuleHeightChanged(float HalfHeightAdjust)
{
	// The capsule centre moves when crouching. Compensate so the camera stays put,
	// then UpdateEyeHeight eases it smoothly to the new height.
	CurrentEyeHeight += HalfHeightAdjust;
}

void UFBBodycamComponent::ApplyPostProcessDefaults()
{
	if (!Camera)
	{
		return;
	}

	Camera->SetFieldOfView(FieldOfView);
	Camera->PostProcessBlendWeight = 1.f;

	FPostProcessSettings& PP = Camera->PostProcessSettings;

	PP.bOverride_SceneFringeIntensity = true;   // chromatic aberration
	PP.SceneFringeIntensity = ChromaticAberrationIntensity;

	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = VignetteIntensity;

	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = FilmGrainIntensity;

	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = MotionBlurAmount;

	PP.bOverride_MotionBlurMax = true;
	PP.MotionBlurMax = MotionBlurMax;

	// Auto exposure: stop the camera from "night-visioning" dark rooms, so darkness stays dark
	// and the flashlight matters. Values are EV100 (project uses the extended luminance range).
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = ExposureMinEV;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = ExposureMaxEV;
	PP.bOverride_AutoExposureSpeedUp = true;
	PP.AutoExposureSpeedUp = ExposureAdaptSpeed;
	PP.bOverride_AutoExposureSpeedDown = true;
	PP.AutoExposureSpeedDown = ExposureAdaptSpeed;

	if (LensDistortionMaterial)
	{
		PP.AddBlendable(LensDistortionMaterial.Get(), 1.f);
	}
}

void UFBBodycamComponent::ApplyUserSettings(float InFieldOfView, float InBrightness, float InMotionScale)
{
	FieldOfView = InFieldOfView;
	MotionScale = FMath::Max(0.f, InMotionScale);

	if (Camera)
	{
		Camera->SetFieldOfView(FieldOfView);
		Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
		Camera->PostProcessSettings.AutoExposureBias = InBrightness;
	}
}

// ------------------------------------------------------------------ Updates

void UFBBodycamComponent::UpdateEyeHeight(float DeltaTime)
{
	// ACharacter switches BaseEyeHeight to CrouchedEyeHeight while crouched.
	CurrentEyeHeight = FMath::FInterpTo(CurrentEyeHeight, OwnerCharacter->BaseEyeHeight, DeltaTime, EyeHeightInterpSpeed);
}

void UFBBodycamComponent::UpdateLanding(float DeltaTime, bool bFalling, float VerticalVelocity)
{
	if (bFalling)
	{
		LastFallSpeed = FMath::Max(0.f, -VerticalVelocity);
	}
	else if (bWasFalling)
	{
		// Just landed: kick the camera down, the spring brings it back.
		LandSpringVelocity -= FMath::Min(LastFallSpeed * LandImpulseScale, LandMaxImpulse);
		LastFallSpeed = 0.f;
	}
	bWasFalling = bFalling;

	const float Dt = FMath::Min(DeltaTime, 1.f / 30.f);
	const float Accel = -LandOffset * LandSpringStiffness - LandSpringVelocity * LandSpringDamping;
	LandSpringVelocity += Accel * Dt;
	LandOffset += LandSpringVelocity * Dt;
}

void UFBBodycamComponent::UpdateHeadBob(float DeltaTime, float GroundSpeed, bool bOnGround)
{
	const float TargetIntensity = (bEnableHeadBob && bOnGround)
		? FMath::Clamp(GroundSpeed / BobReferenceSpeed, 0.f, BobMaxIntensity)
		: 0.f;
	BobIntensity = FMath::FInterpTo(BobIntensity, TargetIntensity, DeltaTime, BobBlendSpeed);

	if (bOnGround && GroundSpeed > 1.f)
	{
		const int32 PrevStep = FMath::FloorToInt(BobPhase / UE_PI);
		BobPhase += (GroundSpeed * DeltaTime / StrideLength) * UE_TWO_PI;
		const int32 NewStep = FMath::FloorToInt(BobPhase / UE_PI);

		if (NewStep != PrevStep && GroundSpeed > 20.f)
		{
			OnFootstep.Broadcast(GroundSpeed);
		}
		if (BobPhase >= UE_TWO_PI)
		{
			BobPhase -= UE_TWO_PI;
		}
	}

	const float S = FMath::Sin(BobPhase);
	BobOffset.X = 0.f;
	BobOffset.Y = S * BobHorizontal * BobIntensity;
	BobOffset.Z = -FMath::Abs(S) * BobVertical * BobIntensity;
	BobRollCurrent = S * BobRoll * BobIntensity;

	// Breathing fades out as walking bob takes over.
	BreathTime += DeltaTime;
	const float BreathWeight = 1.f - FMath::Clamp(BobIntensity, 0.f, 1.f);
	BobOffset.Z += FMath::Sin(BreathTime * UE_TWO_PI * BreathFrequency) * BreathAmplitude * BreathWeight;
}

void UFBBodycamComponent::UpdateHandheldShake(float DeltaTime, float GroundSpeed, bool bOnGround)
{
	if (!bEnableHandheldShake)
	{
		ShakeRotation = FRotator::ZeroRotator;
		return;
	}

	const float SpeedAlpha = bOnGround ? FMath::Clamp(GroundSpeed / ShakeReferenceSpeed, 0.f, 1.f) : 0.3f;

	// Accumulate time with a changing speed (instead of Time * Frequency) so the noise never jumps.
	ShakeTime += DeltaTime * FMath::Lerp(IdleShakeFrequency, SprintShakeFrequency, SpeedAlpha);
	ShakeAngleCurrent = FMath::FInterpTo(ShakeAngleCurrent, FMath::Lerp(IdleShakeAngle, SprintShakeAngle, SpeedAlpha), DeltaTime, 4.f);

	// Different offsets so pitch/yaw/roll don't move together.
	ShakeRotation.Pitch = FMath::PerlinNoise1D(ShakeTime)          * ShakeAngleCurrent;
	ShakeRotation.Yaw   = FMath::PerlinNoise1D(ShakeTime + 37.13f) * ShakeAngleCurrent;
	ShakeRotation.Roll  = FMath::PerlinNoise1D(ShakeTime + 91.71f) * ShakeAngleCurrent * 0.6f;
}

void UFBBodycamComponent::UpdateLookSway(float DeltaTime)
{
	// Per-frame input -> rate, so sway feels the same at any FPS.
	const FVector2D LookRate = PendingLookInput / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
	PendingLookInput = FVector2D::ZeroVector;

	FRotator Target;
	Target.Roll  = FMath::Clamp(LookRate.X * LookSwayRoll, -MaxLookSwayRoll, MaxLookSwayRoll);
	Target.Yaw   = FMath::Clamp(-LookRate.X * LookSwayLag, -MaxLookSwayLag, MaxLookSwayLag);
	Target.Pitch = FMath::Clamp(-LookRate.Y * LookSwayLag, -MaxLookSwayLag, MaxLookSwayLag);

	LookSway = FMath::RInterpTo(LookSway, Target, DeltaTime, LookSwayInterpSpeed);
}

void UFBBodycamComponent::UpdateLean(float DeltaTime)
{
	LeanAlpha = FMath::FInterpTo(LeanAlpha, LeanInput, DeltaTime, LeanInterpSpeed);

	float DesiredOffset = LeanAlpha * LeanDistance;

	// Stop the camera from leaning through walls.
	if (!FMath::IsNearlyZero(DesiredOffset, 0.1f))
	{
		const FVector Start = CameraRoot->GetComponentLocation();
		const FVector End = Start + CameraRoot->GetRightVector() * DesiredOffset;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamLean), false, GetOwner());
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, LeanCollisionChannel,
		                                     FCollisionShape::MakeSphere(LeanProbeRadius), Params))
		{
			DesiredOffset = FMath::Sign(DesiredOffset) * Hit.Distance;
		}
	}

	CurrentLeanOffset = FMath::FInterpTo(CurrentLeanOffset, DesiredOffset, DeltaTime, LeanInterpSpeed * 2.f);
}

void UFBBodycamComponent::ApplyCameraTransform()
{
	// Root: eye height + landing dip, rotated to where the player looks.
	CameraRoot->SetRelativeLocation(FVector(0.f, 0.f, CurrentEyeHeight + LandOffset));
	CameraRoot->SetWorldRotation(OwnerCharacter->GetControlRotation());

	// Camera: mount + bob + lean, plus all the small rotations on top.
	const float LeanRoll = (LeanDistance > 0.f) ? (CurrentLeanOffset / LeanDistance) * LeanAngle : 0.f;

	// MotionScale (settings) tones down the "handheld" movement for players who get motion sick.
	const float M = MotionScale;
	const FVector Location = MountOffset + BobOffset * M + FVector(0.f, CurrentLeanOffset, 0.f);
	const FRotator Rotation(
		(LookSway.Pitch + ShakeRotation.Pitch) * M + KickRotation.Pitch,
		(LookSway.Yaw + ShakeRotation.Yaw) * M + KickRotation.Yaw,
		(BobRollCurrent + LookSway.Roll + ShakeRotation.Roll) * M + LeanRoll + KickRotation.Roll);

	Camera->SetRelativeLocationAndRotation(Location, Rotation);
}
