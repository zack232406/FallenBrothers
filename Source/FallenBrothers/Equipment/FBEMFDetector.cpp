// Fallen Brothers - EMF detector.

#include "Equipment/FBEMFDetector.h"

#include "Paranormal/FBParanormalSubsystem.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "UI/FBHUD.h"

AFBEMFDetector::AFBEMFDetector()
{
	// Real model (if imported): /Game/FallenBrothers/Items/item_emf
	ModelName = TEXT("item_emf");
	ModelLength = 16.f;

	DisplayName = NSLOCTEXT("FB", "EMFDetector", "EMF Reader");
	HoldOffset = FVector(30.f, 18.f, -20.f);
}

void AFBEMFDetector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	float Target = 0.f;
	if (bPoweredOn)
	{
		if (const UFBParanormalSubsystem* Subsystem = GetWorld()->GetSubsystem<UFBParanormalSubsystem>())
		{
			Target = Subsystem->GetEMFLevelAt(GetActorLocation());
		}

		// Background noise: occasionally flickers to level 1 even when nothing is around.
		NoiseTime += DeltaSeconds * 3.f;
		Target += FMath::Max(0.f, FMath::PerlinNoise1D(NoiseTime)) * Jitter * 2.f;
	}

	SmoothedLevel = FMath::FInterpTo(SmoothedLevel, Target, DeltaSeconds, ResponseSpeed);
	DisplayedLevel = FMath::Clamp(FMath::FloorToInt(SmoothedLevel + 0.15f), 0, 5);
}

void AFBEMFDetector::DrawEquipmentHUD(AFBHUD* HUD, float UIScale)
{
	if (!HUD || !HUD->HasCanvas())
	{
		return;
	}

	Super::DrawEquipmentHUD(HUD, UIScale); // Name

	// 5 LEDs: green, green, yellow, orange, red.
	static const FLinearColor LedColors[5] =
	{
		FLinearColor(0.2f, 0.9f, 0.2f),
		FLinearColor(0.5f, 0.95f, 0.15f),
		FLinearColor(1.0f, 0.85f, 0.1f),
		FLinearColor(1.0f, 0.45f, 0.05f),
		FLinearColor(1.0f, 0.1f, 0.05f)
	};

	const float Led = 14.f * UIScale;
	const float Gap = 6.f * UIScale;
	const float TotalW = 5.f * Led + 4.f * Gap;
	const float StartX = HUD->GetScreenSize().X - 40.f * UIScale - TotalW;
	const float Y = HUD->GetScreenSize().Y - 40.f * UIScale;

	const int32 Level = GetDisplayedLevel();
	for (int32 i = 0; i < 5; ++i)
	{
		const bool bLit = bPoweredOn && Level >= i + 1;
		FLinearColor C = LedColors[i];
		C.A = bLit ? 0.95f : 0.15f;
		HUD->DrawRect(C, StartX + i * (Led + Gap), Y, Led, Led);
	}

	if (!bPoweredOn)
	{
		DrawRightAlignedText(HUD, TEXT("OFF"), StartX - 10.f * UIScale, Y - 4.f * UIScale, FLinearColor(1.f, 1.f, 1.f, 0.5f), 1.0f * UIScale);
	}
}
