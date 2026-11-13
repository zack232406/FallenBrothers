// Fallen Brothers - Project-wide settings.
//
// Editor: Project Settings > Game > Fallen Brothers.
// Pick which map is the main menu and which map New Game starts on.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FBProjectSettings.generated.h"

class UWorld;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Fallen Brothers"))
class FALLENBROTHERS_API UFBProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFBProjectSettings();

	/** Map with the main menu. */
	UPROPERTY(Config, EditAnywhere, Category = "Levels")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	/** Map that "New Game" opens (case 1). */
	UPROPERTY(Config, EditAnywhere, Category = "Levels")
	TSoftObjectPtr<UWorld> FirstCaseLevel;

	/** Used when MainMenuLevel is empty. */
	UPROPERTY(Config, EditAnywhere, Category = "Levels|Fallback")
	FName MainMenuLevelName = TEXT("MainMenu");

	/** Used when FirstCaseLevel is empty. */
	UPROPERTY(Config, EditAnywhere, Category = "Levels|Fallback")
	FName FirstCaseLevelName = TEXT("testMap");

	static void OpenMainMenu(const UObject* WorldContext);
	static void OpenFirstCase(const UObject* WorldContext);
};
