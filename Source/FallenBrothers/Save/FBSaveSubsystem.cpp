// Fallen Brothers - Save subsystem.

#include "Save/FBSaveSubsystem.h"

#include "Save/FBSaveGame.h"

#include "Combat/FBHealthComponent.h"
#include "Companion/FBCompanion.h"
#include "Interaction/FBDoor.h"
#include "Inventory/FBInventoryComponent.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Mission/FBCampaignSubsystem.h"
#include "Mission/FBMissionManager.h"
#include "UI/FBHUD.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace FBSave
{
	static const FString SlotName = TEXT("FallenBrothers_Slot0");
	constexpr int32 UserIndex = 0;
}

UFBSaveSubsystem* UFBSaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UFBSaveSubsystem>() : nullptr;
}

bool UFBSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(FBSave::SlotName, FBSave::UserIndex);
}

void UFBSaveSubsystem::DeleteSave()
{
	if (HasSave())
	{
		UGameplayStatics::DeleteGameInSlot(FBSave::SlotName, FBSave::UserIndex);
	}
	PendingLoad = nullptr;
	RemovedActors.Reset();
	UnlockedDoors.Reset();
}

FString UFBSaveSubsystem::GetSaveSummary() const
{
	if (!HasSave())
	{
		return FString();
	}
	const UFBSaveGame* Save = Cast<UFBSaveGame>(UGameplayStatics::LoadGameFromSlot(FBSave::SlotName, FBSave::UserIndex));
	if (!Save)
	{
		return FString();
	}
	const FString Where = Save->LocationLabel.IsEmpty() ? Save->LevelName : Save->LocationLabel;
	return FString::Printf(TEXT("%s   %s"), *Where, *Save->SaveTime.ToString(TEXT("%d.%m.%Y  %H:%M")));
}

void UFBSaveSubsystem::NotifyActorRemoved(const AActor* Actor)
{
	if (UFBSaveSubsystem* Self = Get(Actor))
	{
		Self->RemovedActors.Add(Actor->GetFName());
	}
}

void UFBSaveSubsystem::NotifyDoorUnlocked(const AActor* Door)
{
	if (UFBSaveSubsystem* Self = Get(Door))
	{
		Self->UnlockedDoors.Add(Door->GetFName());
	}
}

// ------------------------------------------------------------------ Save

bool UFBSaveSubsystem::SaveGame(const UObject* WorldContext, bool bQuiet)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	APawn* Player = UGameplayStatics::GetPlayerPawn(WorldContext, 0);
	if (!World || !Player)
	{
		return false;
	}

	const UFBHealthComponent* Health = Player->FindComponentByClass<UFBHealthComponent>();
	if (Health && Health->IsDead())
	{
		return false; // Never save a dead player.
	}

	UFBSaveGame* Save = Cast<UFBSaveGame>(UGameplayStatics::CreateSaveGameObject(UFBSaveGame::StaticClass()));
	Save->LevelName = UGameplayStatics::GetCurrentLevelName(WorldContext, true);
	Save->SaveTime = FDateTime::Now();
	if (const AFBMissionManager* Mission = AFBMissionManager::Find(WorldContext))
	{
		Save->LocationLabel = FString::Printf(TEXT("%s - %s"), *Mission->GetMissionTitle().ToString(), *Mission->GetTownName().ToString());
	}

	Save->PlayerLocation = Player->GetActorLocation();
	if (const AController* Controller = Player->GetController())
	{
		Save->ControlRotation = Controller->GetControlRotation();
	}
	Save->PlayerHealthPercent = Health ? Health->GetHealthPercent() : 1.f;

	if (const UFBInventoryComponent* Inventory = Player->FindComponentByClass<UFBInventoryComponent>())
	{
		Save->Inventory = Inventory->GetItems();
	}
	if (const UFBCaseJournalComponent* Journal = Player->FindComponentByClass<UFBCaseJournalComponent>())
	{
		Save->JournalEntries = Journal->GetEntries();
		Save->Clues = Journal->GetClueArray();
	}
	if (const UFBCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UFBCampaignSubsystem>())
	{
		Save->CompletedMissions = Campaign->GetCompletedMissions().Array();
	}

	Save->RemovedActors = RemovedActors.Array();
	Save->UnlockedDoors = UnlockedDoors.Array();

	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, FBSave::SlotName, FBSave::UserIndex);
	if (!bQuiet)
	{
		AFBHUD::ShowNotificationFor(Player, bOk ? NSLOCTEXT("FB", "Saved", "Game saved.") : NSLOCTEXT("FB", "SaveFailed", "Save failed!"), 2.f);
	}
	return bOk;
}

// ------------------------------------------------------------------ Load

bool UFBSaveSubsystem::LoadGame(const UObject* WorldContext)
{
	if (!HasSave())
	{
		AFBHUD::ShowNotificationFor(UGameplayStatics::GetPlayerPawn(WorldContext, 0), NSLOCTEXT("FB", "NoSave", "No save found."), 2.f);
		return false;
	}

	UFBSaveGame* Save = Cast<UFBSaveGame>(UGameplayStatics::LoadGameFromSlot(FBSave::SlotName, FBSave::UserIndex));
	if (!Save)
	{
		return false;
	}

	// Always reload the level so the world starts clean, then apply the save on top.
	PendingLoad = Save;
	UGameplayStatics::OpenLevel(WorldContext, FName(*Save->LevelName));
	return true;
}

void UFBSaveSubsystem::OnLevelStarted(UWorld* World)
{
	if (PendingLoad)
	{
		UFBSaveGame* Save = PendingLoad;
		PendingLoad = nullptr;
		ApplyLoad(World, Save);
	}
	else
	{
		// A fresh start of this level (new case, or restart without a save).
		RemovedActors.Reset();
		UnlockedDoors.Reset();
	}
}

void UFBSaveSubsystem::ApplyLoad(UWorld* World, const UFBSaveGame* Save)
{
	RemovedActors = TSet<FName>(Save->RemovedActors);
	UnlockedDoors = TSet<FName>(Save->UnlockedDoors);

	if (UFBCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UFBCampaignSubsystem>())
	{
		Campaign->SetCompletedMissions(TSet<FName>(Save->CompletedMissions));
	}

	// World: remove what was already taken / killed, unlock doors.
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (RemovedActors.Contains(Actor->GetFName()))
		{
			ToDestroy.Add(Actor);
		}
		else if (AFBDoor* Door = Cast<AFBDoor>(Actor))
		{
			if (UnlockedDoors.Contains(Door->GetFName()))
			{
				Door->SetLocked(false);
			}
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		Actor->Destroy();
	}

	// Player.
	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player)
	{
		return;
	}
	Player->TeleportTo(Save->PlayerLocation, FRotator(0.f, Save->ControlRotation.Yaw, 0.f));
	if (AController* Controller = Player->GetController())
	{
		Controller->SetControlRotation(Save->ControlRotation);
	}
	if (UFBInventoryComponent* Inventory = Player->FindComponentByClass<UFBInventoryComponent>())
	{
		Inventory->SetItems(Save->Inventory);
	}
	if (UFBCaseJournalComponent* Journal = Player->FindComponentByClass<UFBCaseJournalComponent>())
	{
		Journal->RestoreState(Save->JournalEntries, Save->Clues);
	}
	if (UFBHealthComponent* Health = Player->FindComponentByClass<UFBHealthComponent>())
	{
		Health->Revive(Save->PlayerHealthPercent);
	}

	// The brother comes with you.
	for (TActorIterator<AFBCompanion> It(World); It; ++It)
	{
		It->TeleportTo(Save->PlayerLocation - Player->GetActorForwardVector() * 200.f, Player->GetActorRotation());
	}

	AFBHUD::ShowNotificationFor(Player,
		FText::Format(NSLOCTEXT("FB", "Loaded", "Loaded save from {0}"), FText::AsDateTime(Save->SaveTime)), 2.5f);
}
