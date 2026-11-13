// Fallen Brothers - Main menu game mode + player controller.
//
// MainMenu map: World Settings > GameMode Override = FBMenuGameMode.
// No player character is spawned. If the map has a Camera Actor, the menu looks through it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "UI/FBMenu.h"
#include "FBMenuGameMode.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class FALLENBROTHERS_API AFBMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFBMenuGameMode();
};

UCLASS()
class FALLENBROTHERS_API AFBMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void CreateMenuInput();
	void UseMenuCamera();
	FFBMenu* GetMenu() const;

	void OnUp()      { if (FFBMenu* M = GetMenu()) M->Navigate(-1); }
	void OnDown()    { if (FFBMenu* M = GetMenu()) M->Navigate(1); }
	void OnLeft()    { if (FFBMenu* M = GetMenu()) M->Adjust(-1); }
	void OnRight()   { if (FFBMenu* M = GetMenu()) M->Adjust(1); }
	void OnConfirm() { if (FFBMenu* M = GetMenu()) M->Confirm(); }
	void OnBack()    { if (FFBMenu* M = GetMenu()) M->Back(); }
	void OnClick()   { if (FFBMenu* M = GetMenu()) M->Click(); }

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MenuContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> UpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DownAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LeftAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ConfirmAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BackAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ClickAction;
};
