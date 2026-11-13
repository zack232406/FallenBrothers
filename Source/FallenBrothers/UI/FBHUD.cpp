// Fallen Brothers - HUD (pure C++, drawn with Canvas).

#include "UI/FBHUD.h"

#include "Interaction/FBInteractionComponent.h"
#include "Equipment/FBEquipmentBase.h"
#include "Equipment/FBEquipmentComponent.h"
#include "Equipment/FBFlashlight.h"
#include "Inventory/FBInventoryComponent.h"
#include "Physics/FBGrabComponent.h"
#include "Combat/FBHealthComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void AFBHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	if (bDeathScreen)
	{
		DrawDeathScreen();
		return;
	}

	DrawHealthOverlay();

	if (bShowTimestamp)
	{
		DrawTimestamp();
	}

	// Pause menu on top of a darkened, frozen frame.
	if (PauseMenu.IsOpen())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		PauseMenu.Draw(this, GetScreenSize(), GetUIScale());
		return;
	}

	// Full-screen screens replace the normal HUD.
	if (ActiveScreen == EFBScreen::Journal)
	{
		DrawJournalScreen();
		DrawNotification();
		return;
	}
	if (ActiveScreen == EFBScreen::Computer)
	{
		DrawComputerScreen();
		DrawNotification();
		return;
	}
	if (ActiveScreen == EFBScreen::Dialogue)
	{
		DrawDialogueScreen();
		DrawNotification();
		return;
	}

	DrawCrosshairAndPrompt();
	DrawObjective();
	DrawTitleCard();
	DrawSubtitle();
	DrawEquipment();
	if (bInventoryOpen)
	{
		DrawInventory();
	}
	DrawNotification();
}

void AFBHUD::DrawInventory()
{
	const APawn* Pawn = GetOwningPawn();
	const UFBInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UFBInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return;
	}

	const float Scale = GetUIScale();
	UFont* Font = GEngine->GetMediumFont();
	const TArray<FFBInventoryItem>& Items = Inventory->GetItems();

	const float X = 40.f * Scale;
	const float LineH = 30.f * Scale;
	const float W = 380.f * Scale;
	const float H = (70.f * Scale) + FMath::Max(1, Items.Num()) * LineH;
	const float Y = Canvas->ClipY * 0.5f - H * 0.5f;

	// Background
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, W, H);

	const FString Title = FString::Printf(TEXT("INVENTORY   %d/%d"), Items.Num(), Inventory->GetMaxSlots());
	DrawText(Title, FLinearColor(1.f, 1.f, 1.f, 0.9f), X + 16.f * Scale, Y + 14.f * Scale, Font, 1.15f * Scale);

	float LineY = Y + 52.f * Scale;
	if (Items.Num() == 0)
	{
		DrawText(TEXT("Empty"), FLinearColor(1.f, 1.f, 1.f, 0.4f), X + 16.f * Scale, LineY, Font, 1.f * Scale);
		return;
	}

	for (const FFBInventoryItem& Item : Items)
	{
		const FString Name = Item.Quantity > 1
			? FString::Printf(TEXT("%s  x%d"), *Item.DisplayName.ToString(), Item.Quantity)
			: Item.DisplayName.ToString();
		DrawText(Name, FLinearColor(1.f, 1.f, 1.f, 0.85f), X + 16.f * Scale, LineY, Font, 1.f * Scale);

		const FString Cat = StaticEnum<EFBItemCategory>()->GetDisplayNameTextByValue(static_cast<int64>(Item.Category)).ToString().ToUpper();
		float CW = 0.f, CH = 0.f;
		GetTextSize(Cat, CW, CH, Font, 0.8f * Scale);
		DrawText(Cat, FLinearColor(1.f, 1.f, 1.f, 0.35f), X + W - 16.f * Scale - CW, LineY + 3.f * Scale, Font, 0.8f * Scale);

		LineY += LineH;
	}
}

void AFBHUD::DrawEquipment()
{
	const APawn* Pawn = GetOwningPawn();
	const UFBEquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UFBEquipmentComponent>() : nullptr;
	if (!Equipment)
	{
		return;
	}

	const float Scale = GetUIScale();
	if (AFBFlashlight* Flashlight = Equipment->GetFlashlight())
	{
		Flashlight->DrawEquipmentHUD(this, Scale);
	}
	if (AFBEquipmentBase* Active = Equipment->GetActiveEquipment())
	{
		Active->DrawEquipmentHUD(this, Scale);
	}
}

// ------------------------------------------------------------------ Notifications

void AFBHUD::ShowNotification(const FText& Message, float Duration)
{
	NotificationText = Message;
	NotificationEndTime = GetWorld()->GetTimeSeconds() + Duration;
}

void AFBHUD::ShowNotificationFor(const AActor* ContextActor, const FText& Message, float Duration)
{
	const APawn* Pawn = Cast<APawn>(ContextActor);
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (AFBHUD* HUD = PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ShowNotification(Message, Duration);
	}
}

// ------------------------------------------------------------------ Drawing

FVector2D AFBHUD::GetScreenSize() const
{
	return Canvas ? FVector2D(Canvas->ClipX, Canvas->ClipY) : FVector2D::ZeroVector;
}

float AFBHUD::GetUIScale() const
{
	// Designed at 1080p, scales with resolution.
	return Canvas ? FMath::Max(0.5f, Canvas->ClipY / 1080.f) : 1.f;
}

void AFBHUD::DrawCenteredText(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, Font, Scale);
}

void AFBHUD::DrawTimestamp()
{
	const float Scale = GetUIScale();
	UFont* Font = GEngine->GetMediumFont();
	const float X = 40.f * Scale;
	const float Y = 30.f * Scale;

	// Blinking red REC dot (1 second cycle).
	const bool bDotOn = FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 1.0f) < 0.6f;
	if (bDotOn)
	{
		DrawRect(FLinearColor(0.9f, 0.05f, 0.05f, 0.9f), X, Y + 4.f * Scale, 14.f * Scale, 14.f * Scale);
	}
	DrawText(TEXT("REC"), FLinearColor(1.f, 1.f, 1.f, 0.85f), X + 22.f * Scale, Y, Font, 1.2f * Scale);

	const FString Stamp = FString::Printf(TEXT("%s   %s"), *CameraLabel,
		*FDateTime::Now().ToString(TEXT("%Y-%m-%d  %H:%M:%S")));
	DrawText(Stamp, FLinearColor(1.f, 1.f, 1.f, 0.75f), X, Y + 28.f * Scale, Font, 1.0f * Scale);
}

void AFBHUD::DrawCrosshairAndPrompt()
{
	const float Scale = GetUIScale();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;

	FText Prompt;
	bool bCanInteract = false;
	bool bHasFocus = false;

	if (const APawn* Pawn = GetOwningPawn())
	{
		if (const UFBInteractionComponent* Interaction = Pawn->FindComponentByClass<UFBInteractionComponent>())
		{
			bHasFocus = Interaction->GetFocusedPrompt(Prompt, bCanInteract);
		}
	}

	if (bShowCrosshair)
	{
		// Tiny dot, a bit bigger and brighter when looking at something usable.
		const float Size = (bHasFocus ? 6.f : 3.f) * Scale;
		const float Alpha = bHasFocus ? 0.9f : 0.4f;
		DrawRect(FLinearColor(1.f, 1.f, 1.f, Alpha), CX - Size * 0.5f, CY - Size * 0.5f, Size, Size);
	}

	// Physics grab prompt / controls (only when no interactable is focused).
	FText GrabText;
	const UFBGrabComponent* Grab = GetOwningPawn() ? GetOwningPawn()->FindComponentByClass<UFBGrabComponent>() : nullptr;
	const bool bHasGrabPrompt = Grab && Grab->GetPrompt(GrabText);
	if (bHasGrabPrompt && (!bHasFocus || Grab->IsHolding()))
	{
		const float GrabY = CY + (bHasFocus ? 70.f : 40.f) * Scale;
		DrawCenteredText(GrabText.ToString(), CX, GrabY, FLinearColor(1.f, 1.f, 1.f, 0.75f), 1.0f * Scale);
	}

	if (bHasFocus)
	{
		const FString Line = bCanInteract
			? FString::Printf(TEXT("[%s]  %s"), *InteractKeyLabel, *Prompt.ToString())
			: Prompt.ToString();
		const FLinearColor Color = bCanInteract ? FLinearColor(1.f, 1.f, 1.f, 0.9f) : FLinearColor(0.6f, 0.6f, 0.6f, 0.7f);
		DrawCenteredText(Line, CX, CY + 40.f * Scale, Color, 1.1f * Scale);
	}
}

void AFBHUD::DrawNotification()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (NotificationText.IsEmpty() || Now >= NotificationEndTime)
	{
		return;
	}

	// Fade out during the last half second.
	const float Alpha = FMath::Clamp((NotificationEndTime - Now) / 0.5f, 0.f, 1.f);
	const float Scale = GetUIScale();

	TArray<FString> Lines;
	NotificationText.ToString().ParseIntoArrayLines(Lines);

	float Y = Canvas->ClipY * 0.78f;
	for (const FString& L : Lines)
	{
		DrawCenteredText(L, Canvas->ClipX * 0.5f, Y, FLinearColor(1.f, 1.f, 1.f, 0.9f * Alpha), 1.15f * Scale);
		Y += 28.f * Scale;
	}
}

// ------------------------------------------------------------------ Health / death

void AFBHUD::ShowDeathScreen()
{
	bDeathScreen = true;
	DeathScreenTime = GetWorld()->GetRealTimeSeconds();
}

void AFBHUD::DrawHealthOverlay()
{
	const APawn* Pawn = GetOwningPawn();
	const UFBHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UFBHealthComponent>() : nullptr;
	if (!Health)
	{
		return;
	}

	// Constant red tint when hurt + a sharp flash right after a hit. No health bar: it's a bodycam.
	const float Hurt = 1.f - Health->GetHealthPercent();
	const float Flash = FMath::Clamp(1.f - Health->GetTimeSinceDamage() / 0.6f, 0.f, 1.f);
	const float Alpha = FMath::Clamp(Hurt * 0.35f + Flash * 0.4f, 0.f, 0.75f);
	if (Alpha <= 0.01f)
	{
		return;
	}

	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Edge = H * 0.18f;
	const FLinearColor Red(0.5f, 0.f, 0.f, Alpha);
	// Thick red edges (cheap vignette) + light full-screen wash.
	DrawRect(Red, 0.f, 0.f, W, Edge);
	DrawRect(Red, 0.f, H - Edge, W, Edge);
	DrawRect(Red, 0.f, Edge, Edge, H - Edge * 2.f);
	DrawRect(Red, W - Edge, Edge, Edge, H - Edge * 2.f);
	DrawRect(FLinearColor(0.4f, 0.f, 0.f, Alpha * 0.3f), 0.f, 0.f, W, H);
}

void AFBHUD::DrawDeathScreen()
{
	const float Now = GetWorld()->GetRealTimeSeconds();
	const float T = Now - DeathScreenTime;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float S = GetUIScale();

	// Fade to black, with static noise bars for the first second.
	const float Black = FMath::Clamp(T / 1.2f, 0.f, 1.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f + 0.4f * Black), 0.f, 0.f, W, H);

	if (T < 1.5f)
	{
		for (int32 i = 0; i < 40; ++i)
		{
			const float Y = FMath::FRandRange(0.f, H);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, FMath::FRandRange(0.03f, 0.15f) * (1.f - T / 1.5f)), 0.f, Y, W, FMath::FRandRange(1.f, 6.f) * S);
		}
	}

	if (T > 0.8f)
	{
		const bool bBlink = FMath::Fmod(Now, 1.f) < 0.7f;
		if (bBlink)
		{
			DrawCenteredText(TEXT("SIGNAL LOST"), W * 0.5f, H * 0.45f, FLinearColor(1.f, 1.f, 1.f, 0.9f), 2.f * S);
		}
		DrawCenteredText(FString::Printf(TEXT("%s   %s"), *CameraLabel, *FDateTime::Now().ToString(TEXT("%Y-%m-%d  %H:%M:%S"))),
			W * 0.5f, H * 0.45f + 60.f * S, FLinearColor(1.f, 1.f, 1.f, 0.4f), 1.f * S);
	}
}

// ------------------------------------------------------------------ Subtitles

void AFBHUD::ShowSubtitle(const FText& Speaker, const FText& Line, float Duration)
{
	SubtitleSpeaker = Speaker;
	SubtitleText = Line;
	SubtitleEndTime = GetWorld()->GetTimeSeconds() + Duration;
}

void AFBHUD::ShowSubtitleFor(const UObject* WorldContext, const FText& Speaker, const FText& Line, float Duration)
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	if (AFBHUD* HUD = PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ShowSubtitle(Speaker, Line, Duration);
	}
}

void AFBHUD::DrawSubtitle()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (SubtitleText.IsEmpty() || Now >= SubtitleEndTime)
	{
		return;
	}

	const float Alpha = FMath::Clamp((SubtitleEndTime - Now) / 0.4f, 0.f, 1.f);
	const float Scale = GetUIScale();
	UFont* Font = GEngine->GetMediumFont();

	const FString Speaker = SubtitleSpeaker.ToString().ToUpper() + TEXT(":  ");
	const FString Line = SubtitleText.ToString();
	float SW = 0.f, LW = 0.f, H = 0.f;
	GetTextSize(Speaker, SW, H, Font, 1.1f * Scale);
	GetTextSize(Line, LW, H, Font, 1.1f * Scale);

	const float X = Canvas->ClipX * 0.5f - (SW + LW) * 0.5f;
	const float Y = Canvas->ClipY * 0.88f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), X - 14.f * Scale, Y - 6.f * Scale, SW + LW + 28.f * Scale, H + 12.f * Scale);
	DrawText(Speaker, FLinearColor(0.95f, 0.75f, 0.35f, Alpha), X, Y, Font, 1.1f * Scale);
	DrawText(Line, FLinearColor(1.f, 1.f, 1.f, 0.95f * Alpha), X + SW, Y, Font, 1.1f * Scale);
}

// ------------------------------------------------------------------ Title card / objective

void AFBHUD::ShowTitleCard(const FText& Top, const FText& Title, const FText& Bottom, float Duration)
{
	TitleTop = Top;
	TitleMain = Title;
	TitleBottom = Bottom;
	TitleStartTime = GetWorld()->GetTimeSeconds();
	TitleDuration = Duration;
}

void AFBHUD::ShowObjective(const FText& Objective)
{
	ObjectiveText = Objective;
	ObjectiveShownTime = GetWorld()->GetTimeSeconds();
}

void AFBHUD::DrawTitleCard()
{
	const float T = GetWorld()->GetTimeSeconds() - TitleStartTime;
	if (TitleMain.IsEmpty() || T < 0.f || T > TitleDuration)
	{
		return;
	}

	// Fade in over 1 s, out over the last 1 s.
	const float Alpha = FMath::Clamp(FMath::Min(T / 1.f, (TitleDuration - T) / 1.f), 0.f, 1.f);
	const float S = GetUIScale();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.38f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha), 0.f, CY - 50.f * S, Canvas->ClipX, 150.f * S);

	if (!TitleTop.IsEmpty())
	{
		DrawCenteredText(TitleTop.ToString(), CX, CY - 38.f * S, FLinearColor(0.95f, 0.75f, 0.35f, 0.9f * Alpha), 1.f * S);
	}
	DrawCenteredText(TitleMain.ToString().ToUpper(), CX, CY - 8.f * S, FLinearColor(1.f, 1.f, 1.f, Alpha), 2.4f * S);
	if (!TitleBottom.IsEmpty())
	{
		DrawCenteredText(TitleBottom.ToString(), CX, CY + 60.f * S, FLinearColor(1.f, 1.f, 1.f, 0.7f * Alpha), 1.1f * S);
	}
}

void AFBHUD::DrawObjective()
{
	const float T = GetWorld()->GetTimeSeconds() - ObjectiveShownTime;
	constexpr float ShowTime = 7.f;
	if (ObjectiveText.IsEmpty() || T > ShowTime)
	{
		return; // Bodycam: only shown briefly. The full list is in the journal (J).
	}

	const float Alpha = FMath::Clamp(FMath::Min(T / 0.5f, (ShowTime - T) / 1.f), 0.f, 1.f);
	const float S = GetUIScale();
	UFont* Font = GEngine->GetMediumFont();
	const float RightX = Canvas->ClipX - 40.f * S;

	const FString Header = TEXT("NEW OBJECTIVE");
	const FString Line = ObjectiveText.ToString();
	float HW = 0.f, LW = 0.f, H = 0.f;
	GetTextSize(Header, HW, H, Font, 0.85f * S);
	GetTextSize(Line, LW, H, Font, 1.1f * S);

	DrawText(Header, FLinearColor(0.95f, 0.75f, 0.35f, 0.9f * Alpha), RightX - HW, 30.f * S, Font, 0.85f * S);
	DrawText(Line, FLinearColor(1.f, 1.f, 1.f, 0.9f * Alpha), RightX - LW, 54.f * S, Font, 1.1f * S);
}
