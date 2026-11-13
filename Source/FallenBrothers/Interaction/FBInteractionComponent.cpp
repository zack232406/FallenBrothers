// Fallen Brothers - Interaction component.

#include "Interaction/FBInteractionComponent.h"

#include "Interaction/FBInteractable.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UFBInteractionComponent::UFBInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UFBInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFocus();
}

bool UFBInteractionComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}

	// For a player this is exactly what the camera sees.
	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

void UFBInteractionComponent::UpdateFocus()
{
	AActor* NewFocus = nullptr;

	FVector ViewLocation;
	FRotator ViewRotation;
	if (GetViewPoint(ViewLocation, ViewRotation))
	{
		const FVector End = ViewLocation + ViewRotation.Vector() * InteractionDistance;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(FBInteractionTrace), false, GetOwner());
		FHitResult Hit;
		const bool bHit = GetWorld()->SweepSingleByChannel(Hit, ViewLocation, End, FQuat::Identity, TraceChannel,
		                                                   FCollisionShape::MakeSphere(TraceRadius), Params);

		AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
		if (HitActor && HitActor->Implements<UFBInteractable>())
		{
			NewFocus = HitActor;
		}

		if (bDrawDebug)
		{
			DrawDebugLine(GetWorld(), ViewLocation, bHit ? Hit.ImpactPoint : End, NewFocus ? FColor::Green : FColor::Red, false, 0.f, 0, 0.5f);
		}
	}

	if (NewFocus != FocusedActor.Get())
	{
		FocusedActor = NewFocus;
		OnFocusChanged.Broadcast(NewFocus);
	}
}

bool UFBInteractionComponent::TryInteract()
{
	UpdateFocus(); // Make sure we act on what is under the crosshair right now.

	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return false;
	}

	if (!IFBInteractable::Execute_CanInteract(Target, GetOwner()))
	{
		return false;
	}

	IFBInteractable::Execute_Interact(Target, GetOwner());
	return true;
}

bool UFBInteractionComponent::GetFocusedPrompt(FText& OutText, bool& bOutCanInteract) const
{
	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return false;
	}

	OutText = IFBInteractable::Execute_GetInteractionText(Target, GetOwner());
	bOutCanInteract = IFBInteractable::Execute_CanInteract(Target, GetOwner());
	return true;
}
