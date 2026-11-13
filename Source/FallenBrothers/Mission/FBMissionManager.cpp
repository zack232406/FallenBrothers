// Fallen Brothers - Mission manager.

#include "Mission/FBMissionManager.h"

#include "Companion/FBCompanion.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Mission/FBCampaignSubsystem.h"
#include "UI/FBHUD.h"
#include "Save/FBSaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AFBMissionManager::AFBMissionManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f; // Twice a second is plenty for objectives.

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BuildDemoMission();
}

AFBMissionManager* AFBMissionManager::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AFBMissionManager> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

AFBHUD* AFBMissionManager::GetHUD() const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	return PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr;
}

// ---------------------------------------------------------------------------
// Demo: case 1 of 13.
// ---------------------------------------------------------------------------
void AFBMissionManager::BuildDemoMission()
{
	MissionTitle = FText::FromString(TEXT("The Weeping Mill"));
	TownName = FText::FromString(TEXT("Harlow Falls"));
	ArrivalLine = FText::FromString(TEXT("Harlow Falls. Three missing, everybody's scared, nobody's talking. Our kind of town."));

	auto Add = [this](const TCHAR* Id, const TCHAR* Text, const TCHAR* Clue, bool bOptional = false)
	{
		FFBObjective O;
		O.Id = Id;
		O.Text = FText::FromString(Text);
		O.CompleteWhenClue = Clue;
		O.bOptional = bOptional;
		Objectives.Add(O);
	};

	Add(TEXT("Obj_Gear"),     TEXT("Get the gear out of the trunk"),                TEXT("Clue_Gear"));
	Add(TEXT("Obj_CheckIn"),  TEXT("Check in at the motel office"),                 TEXT("Clue_MotelRoom"));
	Add(TEXT("Obj_Arrive"),   TEXT("Find out what's happening in Harlow Falls"),    TEXT("Clue_OldMill"));
	Add(TEXT("Obj_Fire"),     TEXT("Learn what happened at the mill in 1952"),      TEXT("Clue_MillFire"));
	Add(TEXT("Obj_Identify"), TEXT("Identify the spirit"),                          TEXT("Clue_Crane"));
	Add(TEXT("Obj_Weakness"), TEXT("Find out how to stop it"),                      TEXT("Clue_Weakness"));
	Add(TEXT("Obj_Families"), TEXT("Question the families of the three missing"),   TEXT("Clue_Families"));
	Add(TEXT("Obj_Trail"),    TEXT("Take EMF readings where the three were last seen"), TEXT("Clue_Trail"));
	Add(TEXT("Obj_Death"),    TEXT("Find out how Elias Crane really died"),         TEXT("Clue_CraneDeath"));
	Add(TEXT("Obj_Pattern"),  TEXT("Work out why he took those three"),             TEXT("Clue_Bloodline"), true);
	Add(TEXT("Obj_Grave"),    TEXT("Find Elias Crane's grave"),                     TEXT("Clue_CraneGrave"));
	Add(TEXT("Obj_Night"),    TEXT("Wait for dark at the motel"),                   TEXT("Case_Night"));
	Add(TEXT("Obj_Keys"),     TEXT("Take Crane's key ring from the mill"),          TEXT("Clue_KeyRing"));
Add(TEXT("Obj_Found"),    TEXT("Find the three he took"),                       TEXT("Clue_FoundAll"), true);
		Add(TEXT("Obj_Creature"), TEXT("Deal with whatever is hunting in the dark"),     TEXT("Creature_Killed"), true);
	Add(TEXT("Obj_Burn"),     TEXT("Salt and burn the remains"),                    TEXT("Case_Solved"));
}

// ------------------------------------------------------------------ Flow

void AFBMissionManager::BeginPlay()
{
	Super::BeginPlay();

	// Already finished this case earlier in the session? Nothing to announce.
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UFBCampaignSubsystem* Campaign = GI->GetSubsystem<UFBCampaignSubsystem>())
		{
			if (Campaign->IsMissionComplete(MissionId))
			{
				UE_LOG(LogTemp, Log, TEXT("Mission %s was already completed - replaying."), *MissionId.ToString());
			}
		}
	}

	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(IntroTimer, this, &AFBMissionManager::ShowIntro, 1.f, false);
	Timers.SetTimer(LineTimer, this, &AFBMissionManager::SayArrivalLine, 6.5f, false);
	Timers.SetTimer(ObjectiveTimer, this, &AFBMissionManager::ShowFirstObjective, 7.5f, false);
}

void AFBMissionManager::ShowIntro()
{
	if (AFBHUD* HUD = GetHUD())
	{
		HUD->ShowTitleCard(
			FText::Format(NSLOCTEXT("FB", "CaseNumber", "CASE {0} / {1}"), FText::AsNumber(MissionNumber), FText::AsNumber(TotalMissions)),
			MissionTitle,
			TownName,
			5.f);
	}
}

void AFBMissionManager::SayArrivalLine()
{
	if (ArrivalLine.IsEmpty())
	{
		return;
	}
	for (TActorIterator<AFBCompanion> It(GetWorld()); It; ++It)
	{
		It->Bark({ ArrivalLine }, TEXT("Arrival"), 0.f);
		return;
	}
}

void AFBMissionManager::ShowFirstObjective()
{
	bIntroDone = true;
	bAutosavePending = true; // Checkpoint on arrival.
	AnnouncedObjectiveIndex = INDEX_NONE; // Tick announces the current one.
}

int32 AFBMissionManager::GetCurrentObjectiveIndex() const
{
	for (int32 i = 0; i < Objectives.Num(); ++i)
	{
		if (!Objectives[i].bOptional && !CompletedObjectives.Contains(Objectives[i].Id))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FText AFBMissionManager::GetCurrentObjectiveText() const
{
	const int32 Index = GetCurrentObjectiveIndex();
	return Objectives.IsValidIndex(Index) ? Objectives[Index].Text : FText::GetEmpty();
}

void AFBMissionManager::GetVisibleObjectives(TArray<FFBObjectiveStatus>& Out) const
{
	Out.Reset();
	const int32 Current = GetCurrentObjectiveIndex();
	const int32 Last = (Current == INDEX_NONE) ? Objectives.Num() - 1 : Current;

	for (int32 i = 0; i < Objectives.Num(); ++i)
	{
		const FFBObjective& O = Objectives[i];
		const bool bDone = CompletedObjectives.Contains(O.Id);
		// Show everything up to the current objective, plus any optional one already done.
		if (i <= Last || bDone)
		{
			FFBObjectiveStatus S;
			S.Text = O.Text;
			S.bDone = bDone;
			S.bOptional = O.bOptional;
			Out.Add(S);
		}
	}
}

void AFBMissionManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMissionComplete)
	{
		return;
	}

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UFBCaseJournalComponent* Journal = Player ? Player->FindComponentByClass<UFBCaseJournalComponent>() : nullptr;
	if (!Journal)
	{
		return;
	}

	AFBHUD* HUD = GetHUD();

	// Tick off objectives whose clue the player now has.
	for (const FFBObjective& O : Objectives)
	{
		if (!CompletedObjectives.Contains(O.Id) && !O.CompleteWhenClue.IsNone() && Journal->HasClue(O.CompleteWhenClue))
		{
			CompletedObjectives.Add(O.Id);
			if (bIntroDone)
			{
				if (HUD)
				{
					HUD->ShowNotification(FText::Format(NSLOCTEXT("FB", "ObjectiveDone", "Objective complete: {0}"), O.Text), 3.f);
				}
				bAutosavePending = true;
			}
		}
	}

	if (!bIntroDone)
	{
		return;
	}

	if (bAutosavePending)
	{
		bAutosavePending = false;
		if (UFBSaveSubsystem* Saves = UFBSaveSubsystem::Get(this))
		{
			Saves->SaveGame(this, /*bQuiet*/ true);
		}
	}

	// All required objectives done?
	const int32 Current = GetCurrentObjectiveIndex();
	if (Current == INDEX_NONE)
	{
		CompleteMission();
		return;
	}

	// New current objective -> show it.
	if (Current != AnnouncedObjectiveIndex)
	{
		AnnouncedObjectiveIndex = Current;
		if (HUD)
		{
			HUD->ShowObjective(Objectives[Current].Text);
		}
	}
}

void AFBMissionManager::CompleteMission()
{
	bMissionComplete = true;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UFBCampaignSubsystem* Campaign = GI->GetSubsystem<UFBCampaignSubsystem>())
		{
			Campaign->MarkMissionComplete(MissionId);
		}
	}

	if (AFBHUD* HUD = GetHUD())
	{
		HUD->ShowObjective(FText::GetEmpty());
		HUD->ShowTitleCard(
			FText::Format(NSLOCTEXT("FB", "CaseClosedTop", "CASE {0} / {1}"), FText::AsNumber(MissionNumber), FText::AsNumber(TotalMissions)),
			NSLOCTEXT("FB", "CaseClosed", "CASE CLOSED"),
			MissionTitle,
			EndDelay - 1.f);
	}

	GetWorldTimerManager().SetTimer(TravelTimer, this, &AFBMissionManager::TravelToNextCase, EndDelay, false);
}

void AFBMissionManager::TravelToNextCase()
{
	if (NextLevelName.IsNone())
	{
		if (AFBHUD* HUD = GetHUD())
		{
			HUD->ShowTitleCard(FText::GetEmpty(), NSLOCTEXT("FB", "DemoEnd", "END OF DEMO"),
			                   NSLOCTEXT("FB", "DemoEndSub", "The road goes on. Thanks for playing."), 8.f);
		}
		return;
	}

	UGameplayStatics::OpenLevel(this, NextLevelName);
}
