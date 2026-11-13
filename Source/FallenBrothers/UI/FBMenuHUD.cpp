// Fallen Brothers - Main menu HUD.

#include "UI/FBMenuHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

void AFBMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	StartTime = FPlatformTime::Seconds();
	Menu.Open(EFBMenuPage::Main, GetOwningPlayerController());
}

void AFBMenuHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float S = FMath::Max(0.5f, H / 1080.f);
	const double Now = FPlatformTime::Seconds();
	const float T = static_cast<float>(Now - StartTime);

	// Darken the left side where the text sits (the menu camera's scene shows on the right).
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), 0.f, 0.f, W, H);
	const int32 Bands = 12;
	for (int32 i = 0; i < Bands; ++i)
	{
		const float A = 0.45f * (1.f - static_cast<float>(i) / Bands);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, A), i * (W * 0.05f), 0.f, W * 0.05f, H);
	}

	// Bodycam touch: blinking REC + timestamp.
	UFont* Font = GEngine->GetMediumFont();
	if (FMath::Fmod(static_cast<float>(Now), 1.f) < 0.6f)
	{
		DrawRect(FLinearColor(0.9f, 0.05f, 0.05f, 0.9f), W - 260.f * S, 34.f * S, 14.f * S, 14.f * S);
	}
	DrawText(TEXT("REC"), FLinearColor(1.f, 1.f, 1.f, 0.8f), W - 238.f * S, 30.f * S, Font, 1.2f * S);
	DrawText(FDateTime::Now().ToString(TEXT("%Y-%m-%d  %H:%M:%S")), FLinearColor(1.f, 1.f, 1.f, 0.6f), W - 260.f * S, 58.f * S, Font, 1.f * S);

	Menu.Draw(this, FVector2D(W, H), S);

	DrawText(TEXT("v0.1  skeleton build"), FLinearColor(1.f, 1.f, 1.f, 0.25f), W - 230.f * S, H - 70.f * S, Font, 0.9f * S);

	// Fade in from black when the menu opens.
	const float Black = 1.f - FMath::Clamp(T / 1.5f, 0.f, 1.f);
	if (Black > 0.f)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, Black), 0.f, 0.f, W, H);
	}
}
