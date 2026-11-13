// Fallen Brothers - Grab component.

#include "Physics/FBGrabComponent.h"

#include "Physics/FBPhysicsProp.h"
#include "UI/FBHUD.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UFBGrabComponent::UFBGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics; // Set the target before physics runs.
}

void UFBGrabComponent::BeginPlay()
{
	Super::BeginPlay();
	SetInterpolationSpeed(30.f);
}

bool UFBGrabComponent::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

UPrimitiveComponent* UFBGrabComponent::FindGrabbable(FHitResult& OutHit) const
{
	FVector ViewLoc;
	FRotator ViewRot;
	if (!GetView(ViewLoc, ViewRot))
	{
		return nullptr;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FBGrabTrace), false, GetOwner());
	const FVector End = ViewLoc + ViewRot.Vector() * GrabReach;

	if (!GetWorld()->SweepSingleByChannel(OutHit, ViewLoc, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(5.f), Params))
	{
		return nullptr;
	}

	UPrimitiveComponent* Comp = OutHit.GetComponent();
	return (Comp && Comp->IsSimulatingPhysics()) ? Comp : nullptr;
}

bool UFBGrabComponent::TryGrab()
{
	if (IsHolding())
	{
		return false;
	}

	FHitResult Hit;
	UPrimitiveComponent* Comp = FindGrabbable(Hit);
	if (!Comp)
	{
		return false;
	}

	const float Mass = Comp->GetMass();
	if (Mass > MaxGrabMassKg)
	{
		AFBHUD::ShowNotificationFor(GetOwner(), NSLOCTEXT("FB", "TooHeavy", "Too heavy to lift."), 2.f);
		return false;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	GetView(ViewLoc, ViewRot);

	const AFBPhysicsProp* Prop = Cast<AFBPhysicsProp>(Comp->GetOwner());
	bHoldingMelee = Prop && Prop->IsMeleeWeapon();
	HeldMassKg = Mass;

	// Keep the object's current orientation relative to where we're facing (yaw only).
	const FQuat ViewYaw = FRotator(0.f, ViewRot.Yaw, 0.f).Quaternion();
	HoldRotationOffset = ViewYaw.Inverse() * Comp->GetComponentQuat();

	// Hold at least far enough that it doesn't clip into us.
	const float MinDist = Comp->Bounds.SphereRadius + 45.f;
	CurrentHoldDistance = FMath::Clamp(FVector::Dist(ViewLoc, Comp->GetComponentLocation()), MinDist, FMath::Max(HoldDistance, MinDist));

	// Don't let the held object shove the player around.
	SavedPawnResponse = Comp->GetCollisionResponseToChannel(ECC_Pawn);
	Comp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	ApplyHandleStrength(Mass, false);
	GrabComponentAtLocationWithRotation(Comp, NAME_None, Comp->GetComponentLocation(), Comp->GetComponentRotation());
	return true;
}

void UFBGrabComponent::Drop()
{
	if (UPrimitiveComponent* Comp = GetGrabbedComponent())
	{
		Comp->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
		ReleaseComponent();
	}
	SwingTime = -1.f;
}

void UFBGrabComponent::Throw()
{
	UPrimitiveComponent* Comp = GetGrabbedComponent();
	if (!Comp)
	{
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	GetView(ViewLoc, ViewRot);

	Drop();

	const float Speed = FMath::Clamp(ThrowImpulse / FMath::Max(HeldMassKg, 0.1f), 150.f, MaxThrowSpeed);
	const FVector PlayerVelocity = GetOwner()->GetVelocity();
	Comp->SetPhysicsLinearVelocity(PlayerVelocity + ViewRot.Vector() * Speed);
	// A little spin looks natural.
	Comp->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 180.f);
}

void UFBGrabComponent::PrimaryAction()
{
	if (!IsHolding())
	{
		return;
	}

	if (bHoldingMelee)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (SwingTime < 0.f && Now - LastSwingEnd >= SwingCooldown)
		{
			SwingTime = 0.f;
			ApplyHandleStrength(HeldMassKg, true);
		}
	}
	else
	{
		Throw();
	}
}

void UFBGrabComponent::ApplyHandleStrength(float MassKg, bool bSwinging)
{
	if (bSwinging)
	{
		// Stiff and fast so the swing actually has speed.
		SetLinearStiffness(4000.f);
		SetLinearDamping(60.f);
		SetAngularStiffness(6000.f);
		SetAngularDamping(200.f);
		SetInterpolationSpeed(80.f);
		return;
	}

	// Heavier = softer spring = lags and sways more.
	const float Heaviness = FMath::Clamp(MassKg / FMath::Max(MaxGrabMassKg, 1.f), 0.f, 1.f);
	SetLinearStiffness(FMath::Lerp(900.f, 250.f, Heaviness));
	SetLinearDamping(FMath::Lerp(60.f, 120.f, Heaviness));
	SetAngularStiffness(FMath::Lerp(1500.f, 400.f, Heaviness));
	SetAngularDamping(FMath::Lerp(150.f, 300.f, Heaviness));
	SetInterpolationSpeed(FMath::Lerp(30.f, 8.f, Heaviness));
}

void UFBGrabComponent::ComputeTarget(FVector& OutLocation, FRotator& OutRotation, float DeltaTime)
{
	FVector ViewLoc;
	FRotator ViewRot;
	GetView(ViewLoc, ViewRot);

	if (!bHoldingMelee)
	{
		OutLocation = ViewLoc + ViewRot.Vector() * CurrentHoldDistance;
		OutRotation = (FRotator(0.f, ViewRot.Yaw, 0.f).Quaternion() * HoldRotationOffset).Rotator();
		return;
	}

	// Melee: held in the right hand, pipe pointing forward and up.
	FVector Offset = MeleeHoldOffset;
	FRotator Extra(-60.f, 0.f, 0.f); // Tilt the pipe's long axis forward.

	if (SwingTime >= 0.f)
	{
		SwingTime += DeltaTime;
		const float A = FMath::Clamp(SwingTime / SwingDuration, 0.f, 1.f);
		const float Arc = FMath::Sin(A * UE_PI); // 0 -> 1 -> 0

		// Right-high to left-low, pushed forward in the middle of the swing.
		Offset.Y = FMath::Lerp(40.f, -45.f, A);
		Offset.Z = FMath::Lerp(25.f, -25.f, A);
		Offset.X += 35.f * Arc;
		Extra.Pitch = -60.f - 40.f * Arc;
		Extra.Yaw = FMath::Lerp(35.f, -55.f, A);

		if (A >= 1.f)
		{
			SwingTime = -1.f;
			LastSwingEnd = GetWorld()->GetTimeSeconds();
			ApplyHandleStrength(HeldMassKg, false);
		}
	}

	const FQuat ViewQuat = ViewRot.Quaternion();
	OutLocation = ViewLoc + ViewQuat.RotateVector(Offset);
	OutRotation = (ViewQuat * Extra.Quaternion()).Rotator();
}

void UFBGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	UPrimitiveComponent* Held = GetGrabbedComponent();
	if (Held)
	{
		FVector TargetLoc;
		FRotator TargetRot;
		ComputeTarget(TargetLoc, TargetRot, DeltaTime);
		SetTargetLocationAndRotation(TargetLoc, TargetRot);

		// Stuck behind something? Let go.
		if (SwingTime < 0.f && FVector::Dist(Held->GetComponentLocation(), TargetLoc) > BreakDistance)
		{
			Drop();
		}
		FocusComponent.Reset();
	}
	else
	{
		FHitResult Hit;
		FocusComponent = FindGrabbable(Hit);
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UFBGrabComponent::GetPrompt(FText& OutText) const
{
	if (IsHolding())
	{
		OutText = bHoldingMelee
			? NSLOCTEXT("FB", "GrabHintMelee", "[LMB] Swing    [G] Throw    [RMB] Drop")
			: NSLOCTEXT("FB", "GrabHint", "[LMB] Throw    [RMB] Drop");
		return true;
	}

	const UPrimitiveComponent* Comp = FocusComponent.Get();
	if (!Comp)
	{
		return false;
	}

	FText Name = NSLOCTEXT("FB", "GrabObject", "object");
	if (const AFBPhysicsProp* Prop = Cast<AFBPhysicsProp>(Comp->GetOwner()))
	{
		Name = Prop->GetDisplayName();
	}

	OutText = (Comp->GetMass() > MaxGrabMassKg)
		? FText::Format(NSLOCTEXT("FB", "GrabTooHeavy", "{0} (too heavy)"), Name)
		: FText::Format(NSLOCTEXT("FB", "GrabPrompt", "[RMB] Grab {0}"), Name);
	return true;
}
