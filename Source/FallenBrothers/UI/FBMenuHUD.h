// Fallen Brothers - Main menu HUD (used by AFBMenuGameMode in the MainMenu map).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/FBMenu.h"
#include "FBMenuHUD.generated.h"

UCLASS()
class FALLENBROTHERS_API AFBMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	FFBMenu& GetMenu() { return Menu; }

private:
	FFBMenu Menu;
	double StartTime = 0.0;
};
