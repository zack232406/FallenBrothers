// Fallen Brothers - Game mode. Spawns our C++ player character automatically.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FBGameMode.generated.h"

UCLASS()
class FALLENBROTHERS_API AFBGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFBGameMode();

	/** Like the engine's, but never gives up: a blocked PlayerStart is still used (the pawn gets nudged free). */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
