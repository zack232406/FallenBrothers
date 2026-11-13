// Fallen Brothers - Player settings.

#include "Core/FBSettingsSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

namespace FBSettingsSlot
{
	static const FString SlotName = TEXT("FallenBrothers_Settings");
	constexpr int32 UserIndex = 0;
}

UFBSettingsSubsystem* UFBSettingsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UFBSettingsSubsystem>() : nullptr;
}

void UFBSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameplayStatics::DoesSaveGameExist(FBSettingsSlot::SlotName, FBSettingsSlot::UserIndex))
	{
		if (const UFBSettingsSave* Save = Cast<UFBSettingsSave>(UGameplayStatics::LoadGameFromSlot(FBSettingsSlot::SlotName, FBSettingsSlot::UserIndex)))
		{
			Settings = Save->Settings;
		}
	}

	// First run: start on High instead of whatever the engine picked (it tends to pick Epic, which with Lumen,
	// ray tracing and Nanite foliage runs out of video memory on 8 GB cards). Only once - the menu choice stands.
	if (!Settings.bDefaultQualityApplied)
	{
		SetQualityLevel(DefaultQualityLevel);
		Settings.bDefaultQualityApplied = true;
		SetSettings(Settings);
	}
}

void UFBSettingsSubsystem::SetSettings(const FFBUserSettings& NewSettings)
{
	Settings = NewSettings;
	Settings.MouseSensitivity = FMath::Clamp(Settings.MouseSensitivity, 0.1f, 3.f);
	Settings.CameraMotion = FMath::Clamp(Settings.CameraMotion, 0.f, 1.5f);
	Settings.FieldOfView = FMath::Clamp(Settings.FieldOfView, 80.f, 115.f);
	Settings.Brightness = FMath::Clamp(Settings.Brightness, -2.f, 2.f);

	if (UFBSettingsSave* Save = Cast<UFBSettingsSave>(UGameplayStatics::CreateSaveGameObject(UFBSettingsSave::StaticClass())))
	{
		Save->Settings = Settings;
		UGameplayStatics::SaveGameToSlot(Save, FBSettingsSlot::SlotName, FBSettingsSlot::UserIndex);
	}

	OnSettingsChanged.Broadcast();
}

// ------------------------------------------------------------------ Graphics (Unreal's GameUserSettings)

int32 UFBSettingsSubsystem::GetQualityLevel() const
{
	const UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const int32 Level = GS ? GS->GetOverallScalabilityLevel() : 3;
	return Level < 0 ? 3 : Level; // -1 = custom mix -> show as Epic
}

void UFBSettingsSubsystem::SetQualityLevel(int32 Level)
{
	if (UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GS->SetOverallScalabilityLevel(FMath::Clamp(Level, 0, 4));
		GS->ApplyNonResolutionSettings();
		GS->SaveSettings();
	}
}

int32 UFBSettingsSubsystem::GetWindowMode() const
{
	const UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GS)
	{
		return 0;
	}
	switch (GS->GetFullscreenMode())
	{
	case EWindowMode::Fullscreen:         return 0;
	case EWindowMode::WindowedFullscreen: return 1;
	default:                              return 2;
	}
}

void UFBSettingsSubsystem::SetWindowMode(int32 Mode)
{
	UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GS)
	{
		return;
	}

	const EWindowMode::Type NewMode =
		(Mode == 0) ? EWindowMode::Fullscreen :
		(Mode == 1) ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
	GS->SetFullscreenMode(NewMode);

	// In the editor this would resize the editor's play window - only store it there.
	if (!GIsEditor)
	{
		GS->ApplyResolutionSettings(false);
	}
	GS->SaveSettings();
}
