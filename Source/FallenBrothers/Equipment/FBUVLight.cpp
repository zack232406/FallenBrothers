// Fallen Brothers - UV light.

#include "Equipment/FBUVLight.h"

#include "Paranormal/FBParanormalSubsystem.h"
#include "Paranormal/FBUVTrace.h"

#include "Engine/World.h"

AFBUVLight::AFBUVLight()
{
	// Real model (if imported): /Game/FallenBrothers/Items/item_uvlight
	ModelName = TEXT("item_uvlight");
	ModelLength = 22.f;

	DisplayName = NSLOCTEXT("FB", "UVLight", "UV Light");
	HoldOffset = FVector(30.f, 18.f, -18.f); // Held in the right hand.
	AimLagSpeed = 12.f;

	LightIntensity = 400.f;
	LightColor = FLinearColor(0.3f, 0.02f, 1.f);
	Range = 700.f;
	InnerConeAngle = 12.f;
	OuterConeAngle = 25.f;

	BatteryLifeSeconds = 300.f;
	bReactsToParanormal = false;
}

void AFBUVLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bLightOn)
	{
		RevealTraces();
	}
}

void AFBUVLight::RevealTraces()
{
	const UFBParanormalSubsystem* Subsystem = GetWorld()->GetSubsystem<UFBParanormalSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const float CosOuter = FMath::Cos(FMath::DegreesToRadians(OuterConeAngle));
	const float CosInner = FMath::Cos(FMath::DegreesToRadians(InnerConeAngle));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(UVReveal), false, this);
	Params.AddIgnoredActor(GetOwner()); // The player.

	for (const TWeakObjectPtr<AFBUVTrace>& TracePtr : Subsystem->GetUVTraces())
	{
		AFBUVTrace* Trace = TracePtr.Get();
		if (!Trace)
		{
			continue;
		}

		const FVector ToTrace = Trace->GetRevealPoint() - Origin;
		const float Distance = ToTrace.Size();
		if (Distance > Range || Distance < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float CosAngle = FVector::DotProduct(Forward, ToTrace / Distance);
		if (CosAngle < CosOuter)
		{
			continue; // Outside the cone.
		}

		// Blocked by a wall?
		FHitResult Hit;
		const FVector End = Trace->GetRevealPoint() - (ToTrace / Distance) * 5.f; // Stop just short of the surface.
		if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params))
		{
			continue;
		}

		// Brighter in the centre of the beam and closer to the light.
		const float ConeFactor = FMath::GetMappedRangeValueClamped(FVector2f(CosOuter, CosInner), FVector2f(0.3f, 1.f), CosAngle);
		const float RangeFactor = 1.f - FMath::Square(Distance / Range);
		Trace->NotifyLitByUV(ConeFactor * RangeFactor);
	}
}
