// Fallen Brothers - Game mode.

#include "Core/FBGameMode.h"

#include "Player/FBPlayerCharacter.h"
#include "UI/FBHUD.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

AFBGameMode::AFBGameMode()
{
	DefaultPawnClass = AFBPlayerCharacter::StaticClass();
	HUDClass = AFBHUD::StaticClass();
}

AActor* AFBGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* Start = Super::ChoosePlayerStart_Implementation(Player))
	{
		return Start;
	}
	// Every start was rejected as "inside something" (e.g. a parked car got generated next to it). Rather than
	// dropping the player at the world origin, use the first one anyway; spawning adjusts the pawn out of the overlap.
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogGameMode, Warning, TEXT("FB: all PlayerStarts blocked, using %s anyway."), *It->GetName());
		return *It;
	}
	return nullptr;
}
