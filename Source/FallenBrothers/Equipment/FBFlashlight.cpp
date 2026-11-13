// Fallen Brothers - Flashlight.

#include "Equipment/FBFlashlight.h"

#include "Paranormal/FBParanormalSubsystem.h"

#include "Components/SpotLightComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "UI/FBHUD.h"

AFBFlashlight::AFBFlashlight()
{
	// Real model (if imported): /Game/FallenBrothers/Items/item_flashlight
	ModelName = TEXT("item_flashlight");
	ModelLength = 24.f;

	DisplayName = NSLOCTEXT("FB", "Flashlight", "Flashlight");
	// Chest-mounted, slightly right and below the camera.
	HoldOffset = FVector(10.f, 14.f, -28.f);
	AimLagSpeed = 10.f;

	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetMobility(EComponentMobility::Movable);
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->SetIntensity(0.f);
	Light->SetVisibility(false);
	Light->SetCastShadows(true);
	Light->VolumetricScatteringIntensity = 0.25f; // Visible beam in fog without whiting out the screen.
}

void AFBFlashlight::SetLightOn(bool bOn)
{
	if (bOn && bUsesBattery && Battery <= 0.f)
	{
		bOn = false; // Dead battery.
	}

	bLightOn = bOn;
	Light->SetVisibility(bLightOn);

	// Keep ticking while on, even for the always-equipped body light.
	SetActorTickEnabled(bEquipped || bLightOn);
}

void AFBFlashlight::OnUnequipped()
{
	SetLightOn(false);
	Super::OnUnequipped();
}

void AFBFlashlight::AddInterference(float Amount, float Duration)
{
	InterferenceAmount = FMath::Max(InterferenceAmount, FMath::Clamp(Amount, 0.f, 1.f));
	InterferenceEndTime = FMath::Max(InterferenceEndTime, GetWorld()->GetTimeSeconds() + Duration);
}

void AFBFlashlight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); // Follow the camera.
	UpdateLight(DeltaSeconds);
}

void AFBFlashlight::UpdateLight(float DeltaSeconds)
{
	if (!bLightOn)
	{
		return;
	}

	// Battery drain.
	if (bUsesBattery && BatteryLifeSeconds > 0.f)
	{
		Battery = FMath::Max(0.f, Battery - DeltaSeconds / BatteryLifeSeconds);
		if (Battery <= 0.f)
		{
			SetLightOn(false);
			return;
		}
	}

	// Paranormal activity nearby -> flicker.
	if (bReactsToParanormal)
	{
		if (const UFBParanormalSubsystem* Subsystem = GetWorld()->GetSubsystem<UFBParanormalSubsystem>())
		{
			const float EMF = Subsystem->GetEMFLevelAt(GetActorLocation());
			if (EMF >= ParanormalFlickerEMF)
			{
				AddInterference(FMath::GetMappedRangeValueClamped(FVector2f(ParanormalFlickerEMF, 5.f), FVector2f(0.4f, 1.f), EMF), 0.25f);
			}
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now > InterferenceEndTime)
	{
		InterferenceAmount = 0.f;
	}

	// Low battery: dim + flicker.
	float BatteryMul = 1.f;
	float LowBatteryFlicker = 0.f;
	if (bUsesBattery && Battery < LowBatteryThreshold && LowBatteryThreshold > 0.f)
	{
		const float T = Battery / LowBatteryThreshold; // 1 -> 0
		BatteryMul = FMath::Lerp(0.3f, 1.f, T);
		LowBatteryFlicker = (1.f - T) * 0.6f;
	}

	const float FlickerStrength = FMath::Max(InterferenceAmount, LowBatteryFlicker);
	float FlickerMul = 1.f;
	if (FlickerStrength > 0.f)
	{
		NoiseTime += DeltaSeconds * 9.f;
		const float N = FMath::PerlinNoise1D(NoiseTime);           // ~ -1..1
		const float Dropout = (N < -0.25f) ? 0.05f : (0.75f + 0.25f * N); // short blackouts
		FlickerMul = FMath::Lerp(1.f, Dropout, FlickerStrength);
	}

	Light->SetLightColor(LightColor);
	Light->SetIntensity(LightIntensity * BatteryMul * FlickerMul);
	Light->SetAttenuationRadius(Range);
	Light->SetInnerConeAngle(InnerConeAngle);
	Light->SetOuterConeAngle(OuterConeAngle);
}

void AFBFlashlight::DrawEquipmentHUD(AFBHUD* HUD, float UIScale)
{
	if (!HUD || !HUD->HasCanvas())
	{
		return;
	}

	const float W = 70.f * UIScale;
	const float H = 10.f * UIScale;
	float X, Y;

	if (bBodyMounted)
	{
		// Small "LIGHT" battery bar, bottom-left.
		X = 40.f * UIScale;
		Y = HUD->GetScreenSize().Y - 45.f * UIScale;
		HUD->DrawText(TEXT("LIGHT"), FLinearColor(1.f, 1.f, 1.f, bLightOn ? 0.8f : 0.35f), X, Y - 24.f * UIScale, nullptr, 1.0f * UIScale);
	}
	else
	{
		// Hand-held (UV): name + bar bottom-right.
		Super::DrawEquipmentHUD(HUD, UIScale);
		X = HUD->GetScreenSize().X - 40.f * UIScale - W;
		Y = HUD->GetScreenSize().Y - 40.f * UIScale;
	}

	const FLinearColor BarColor = Battery < LowBatteryThreshold ? FLinearColor(0.9f, 0.2f, 0.1f, 0.85f) : FLinearColor(1.f, 1.f, 1.f, 0.7f);
	HUD->DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.4f), X, Y, W, H);
	HUD->DrawRect(BarColor, X, Y, W * Battery, H);
}

bool AFBFlashlight::IsPointLit(const FVector& Point, const AActor* IgnoreActor) const
{
	if (!bLightOn)
	{
		return false;
	}

	const FVector Origin = GetActorLocation();
	const FVector To = Point - Origin;
	const float Dist = To.Size();
	if (Dist > Range * 0.9f || Dist < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	if (FVector::DotProduct(GetActorForwardVector(), To / Dist) < FMath::Cos(FMath::DegreesToRadians(OuterConeAngle)))
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FlashlightLit), false, this);
	Params.AddIgnoredActor(GetOwner());
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}
	return !GetWorld()->LineTraceSingleByChannel(Hit, Origin, Point, ECC_Visibility, Params);
}
