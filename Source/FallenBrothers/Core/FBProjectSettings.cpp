// Fallen Brothers - Project-wide settings.

#include "Core/FBProjectSettings.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UFBProjectSettings::UFBProjectSettings()
{
	CategoryName = TEXT("Game");
}

void UFBProjectSettings::OpenMainMenu(const UObject* WorldContext)
{
	const UFBProjectSettings* Settings = GetDefault<UFBProjectSettings>();
	if (!Settings->MainMenuLevel.IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(WorldContext, Settings->MainMenuLevel);
	}
	else
	{
		UGameplayStatics::OpenLevel(WorldContext, Settings->MainMenuLevelName);
	}
}

void UFBProjectSettings::OpenFirstCase(const UObject* WorldContext)
{
	const UFBProjectSettings* Settings = GetDefault<UFBProjectSettings>();
	if (!Settings->FirstCaseLevel.IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(WorldContext, Settings->FirstCaseLevel);
	}
	else
	{
		UGameplayStatics::OpenLevel(WorldContext, Settings->FirstCaseLevelName);
	}
}
