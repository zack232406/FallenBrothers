// Fallen Brothers - Player settings (mouse, camera motion, FOV, brightness, graphics).
//
// Gameplay settings are stored in their own save slot ("FallenBrothers_Settings"),
// separate from the game save, so New Game never resets them.
// Graphics quality / window mode use Unreal's own GameUserSettings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FBSettingsSubsystem.generated.h"

USTRUCT()
struct FFBUserSettings
{
	GENERATED_BODY()

	UPROPERTY() float MouseSensitivity = 1.f;   // 0.1 - 3
	UPROPERTY() bool bInvertY = false;
	UPROPERTY() float CameraMotion = 1.f;       // 0 - 1.5: head bob / handheld shake strength
	UPROPERTY() float FieldOfView = 100.f;      // 80 - 115
	UPROPERTY() float Brightness = 0.f;         // -2 - +2 exposure stops
	/** Set once the first-run graphics default (High) has been applied; after that the player's choice stands. */
	UPROPERTY() bool bDefaultQualityApplied = false;
};

UCLASS()
class FALLENBROTHERS_API UFBSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FFBUserSettings Settings;
};

UCLASS()
class FALLENBROTHERS_API UFBSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UFBSettingsSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FFBUserSettings& GetSettings() const { return Settings; }

	/** Store, write to disk and tell everyone (the player re-applies them). */
	void SetSettings(const FFBUserSettings& NewSettings);

	/** Graphics quality the game starts with on a new install (High: smooth on 8 GB cards, still looks good). */
	static constexpr int32 DefaultQualityLevel = 2;

	/** 0 Low, 1 Medium, 2 High, 3 Epic, 4 Cinematic. */
	int32 GetQualityLevel() const;
	void SetQualityLevel(int32 Level);

	/** 0 Fullscreen, 1 Borderless, 2 Windowed. */
	int32 GetWindowMode() const;
	void SetWindowMode(int32 Mode);

	FSimpleMulticastDelegate OnSettingsChanged;

private:
	FFBUserSettings Settings;
};
