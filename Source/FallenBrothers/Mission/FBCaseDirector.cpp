// Fallen Brothers - Case director (day investigation -> evening confrontation). See FBCaseDirector.h.

#include "Mission/FBCaseDirector.h"

#include "Mission/FBMissionManager.h"
#include "Paranormal/FBGhost.h"
#include "Paranormal/FBGraveSite.h"
#include "Paranormal/FBLastSeenSpot.h"
#include "Paranormal/FBBoundRelic.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Investigation/FBInvestigationTypes.h"
#include "Inventory/FBInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/PackageName.h"
#include "Town/FBAtmosphere.h"
#include "Town/FBTownsfolk.h"
#include "Companion/FBCompanion.h"
#include "UI/FBHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	constexpr float GDayHour = 10.5f;     // overcast late morning
	constexpr float GNightHour = 21.65f;  // "That night", 21:40
	const FName GNightClue = TEXT("Case_Night");
}

UFBCaseDirector* UFBCaseDirector::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFBCaseDirector>() : nullptr;
}

bool UFBCaseDirector::IsEvening(const UWorld* World)
{
	const UFBCaseDirector* D = World ? World->GetSubsystem<UFBCaseDirector>() : nullptr;
	return D && D->bActive && D->Phase == EFBCasePhase::Evening;
}

bool UFBCaseDirector::IsCaseDay(const UWorld* World)
{
	UFBCaseDirector* D = World ? World->GetSubsystem<UFBCaseDirector>() : nullptr;
	if (!D)
	{
		return false;
	}
	D->EnsureStarted();
	return D->bActive && D->Phase == EFBCasePhase::Day;
}

bool UFBCaseDirector::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UFBCaseDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	EnsureStarted();
	if (bActive)
	{
		InWorld.GetTimerManager().SetTimer(CheckTimer, FTimerDelegate::CreateUObject(this, &UFBCaseDirector::Check), 0.5f, true, 1.f);
	}
}

void UFBCaseDirector::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::Deinitialize();
}

void UFBCaseDirector::EnsureStarted()
{
	UWorld* World = GetWorld();
	if (bStarted || !World)
	{
		return;
	}
	bStarted = true;
	bActive = bool(TActorIterator<AFBGhost>(World)) || bool(TActorIterator<AFBGraveSite>(World)) || bool(TActorIterator<AFBMissionManager>(World));
	if (!bActive)
	{
		return;
	}
	bNightLevel = World->GetMapName().Contains(TEXT("Night"));
	if (bNightLevel)
	{
		// The evening's own level: dark from the first frame. The player's journal arrives in Check().
		Phase = EFBCasePhase::Day; // until the carry-over is restored
		for (TActorIterator<AFBAtmosphere> It(World); It; ++It)
		{
			It->bDayNightCycle = false;
			It->MoonBrightness = 0.06f;
			It->FogAmount = 0.75f;
			It->SetMood(EFBMood::FoggyNight);
			It->SetTimeOfDay(GNightHour);
		}
		UE_LOG(LogTemp, Log, TEXT("FBCaseDirector: night level started."));
		return;
	}
	Phase = EFBCasePhase::Day;
	ApplyDay();
	SpawnLastSeenSpots();
	UE_LOG(LogTemp, Log, TEXT("FBCaseDirector: case day started."));
}

APawn* UFBCaseDirector::GetPlayer() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void UFBCaseDirector::ApplyDay()
{
	for (TActorIterator<AFBAtmosphere> It(GetWorld()); It; ++It)
	{
		It->bDayNightCycle = false; // the story moves the clock, not a timer
		It->MoonBrightness = 1.f;
		It->FogAmount = 1.f;
		It->SetMood(EFBMood::OvercastDay);
		It->SetTimeOfDay(GDayHour);
	}
	SetGhostsAwake(false);
}

void UFBCaseDirector::SetGhostsAwake(bool bAwake)
{
	// By day the ghost is only a presence: the EMF still twitches near its haunt, but it doesn't show or attack.
	for (TActorIterator<AFBGhost> It(GetWorld()); It; ++It)
	{
		It->SetActorTickEnabled(bAwake);
		It->SetActorHiddenInGame(!bAwake);
		It->SetActorEnableCollision(bAwake);
	}
}

void UFBCaseDirector::Check()
{
	APawn* Player = GetPlayer();
	const UFBCaseJournalComponent* Journal = Player ? Player->FindComponentByClass<UFBCaseJournalComponent>() : nullptr;
	if (!Journal || Phase != EFBCasePhase::Day)
	{
		return;
	}
	if (bNightLevel && !bCarryChecked)
	{
		bCarryChecked = true;
		UFBCaseCarryOver* Carry = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UFBCaseCarryOver>() : nullptr;
		UFBCaseJournalComponent* J = Player->FindComponentByClass<UFBCaseJournalComponent>();
		if (Carry && Carry->bPending && J)
		{
			Carry->bPending = false;
			J->RestoreState(Carry->Entries, Carry->Clues);
			if (UFBInventoryComponent* Inv = Player->FindComponentByClass<UFBInventoryComponent>())
			{
				Inv->SetItems(Carry->Items);
			}
		}
		else if (J)
		{
			// Opened directly (testing / a save without the day): everything the day would have given.
			J->AddClues(EveningClues);
		}
		if (J)
		{
			J->AddClue(GNightClue);
		}
		ApplyEvening(true);
		NightIntro();
		return;
	}
	if (Journal->HasClue(GNightClue))
	{
		ApplyEvening(true); // a save from the evening
		return;
	}
	if (Phase == EFBCasePhase::Evening)
	{
		return;
	}
	// All three families questioned -> one clue for the objective list.
	if (!Journal->HasClue(TEXT("Clue_Families")) && Journal->HasAllClues(TArray<FName>{ FName(TEXT("Clue_FamWhitmore")), FName(TEXT("Clue_FamDecker")), FName(TEXT("Clue_FamBarlow")) }))
	{
		if (UFBCaseJournalComponent* J = Player->FindComponentByClass<UFBCaseJournalComponent>())
		{
			J->AddClue(TEXT("Clue_Families"));
		}
	}
	if (!Journal->HasClue(TEXT("Clue_Trail")) && Journal->HasAllClues(TArray<FName>{ FName(TEXT("Clue_SpotDecker")), FName(TEXT("Clue_SpotBarlow")), FName(TEXT("Clue_SpotWhitmore")) }))
	{
		if (UFBCaseJournalComponent* J = Player->FindComponentByClass<UFBCaseJournalComponent>())
		{
			J->AddClue(TEXT("Clue_Trail"));
			FFBJournalEntry Entry;
			Entry.Id = TEXT("Note_Trail");
			Entry.Title = NSLOCTEXT("FB", "TrailTitle", "The three readings");
			Entry.Text = NSLOCTEXT("FB", "TrailText", "Mill. Road. Treeline. Put the three readings on the map and they make a straight line - and every drag mark points the same way. He isn't keeping them at the mill. He's taking them somewhere. Toward the churchyard.");
			J->AddEntry(Entry, true);
		}
	}
	if (!Journal->HasAllClues(EveningClues))
	{
		return;
	}
	if (!bToldToWait)
	{
		bToldToWait = true;
		AFBHUD::ShowNotificationFor(Player, NSLOCTEXT("FB", "WaitForDark", "Nothing more to do in daylight. Head back to the motel and wait for dark."), 6.f);
		for (TActorIterator<AFBCompanion> It(GetWorld()); It; ++It)
		{
			It->Bark({ NSLOCTEXT("FB", "ZackWaitDark", "We know who he is and where they put him. We're not digging up a grave in broad daylight. Motel. We go after dark.") }, TEXT("WaitDark"), 0.f);
			break;
		}
	}
	if (FVector::Dist2D(Player->GetActorLocation(), WaitPoint) < WaitRadius)
	{
		StartDusk();
	}
}

void UFBCaseDirector::StartDusk()
{
	Phase = EFBCasePhase::Dusk;
	UWorld* World = GetWorld();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->SetIgnoreMoveInput(true);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 2.f, FLinearColor::Black, false, true);
		}
	}
	World->GetTimerManager().SetTimer(DuskTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!bNightLevel && FPackageName::DoesPackageExist(NightLevelPath))
		{
			TravelToNight();
		}
		else
		{
			ApplyEvening(false);
		}
	}), 2.3f, false);
}

void UFBCaseDirector::ApplyEvening(bool bInstant)
{
	UWorld* World = GetWorld();
	Phase = EFBCasePhase::Evening;
	for (TActorIterator<AFBAtmosphere> It(World); It; ++It)
	{
		It->bDayNightCycle = false;
		// Classic horror dark: almost no moon, thin ground fog (thick fog glows under the street lamps and lights the sky). You see what the flashlight shows and nothing else.
		It->MoonBrightness = 0.06f;
		It->FogAmount = 0.75f;
		It->SetMood(EFBMood::FoggyNight);
		It->SetTimeOfDay(GNightHour);
	}
	SendTownHome();
	SetGhostsAwake(true);
	for (TActorIterator<AFBLastSeenSpot> It(World); It; ++It)
	{
		if (!It->bNightSpot)
		{
			It->SetQuiet(true);
		}
	}
	// What binds him: the key ring, in the heart of his haunt (where the ghost was placed), at hand height.
	if (!bool(TActorIterator<AFBBoundRelic>(World)))
	{
		const UFBCaseJournalComponent* J = GetPlayer() ? GetPlayer()->FindComponentByClass<UFBCaseJournalComponent>() : nullptr;
		if (!J || !J->HasClue(TEXT("Clue_KeyRing")))
		{
			for (TActorIterator<AFBGhost> It(World); It; ++It)
			{
				FVector At = It->GetActorLocation();
				FHitResult Floor;
				if (World->LineTraceSingleByChannel(Floor, At + FVector(0.f, 0.f, 50.f), At - FVector(0.f, 0.f, 600.f), ECC_WorldStatic))
				{
					At = Floor.ImpactPoint + FVector(0.f, 0.f, 110.f);
				}
				World->SpawnActor<AFBBoundRelic>(AFBBoundRelic::StaticClass(), At, FRotator::ZeroRotator);
				UE_LOG(LogTemp, Log, TEXT("FBCaseDirector: bound relic at %s"), *At.ToCompactString());
				break;
			}
		}
	}

	APawn* Player = GetPlayer();
	if (UFBCaseJournalComponent* Journal = Player ? Player->FindComponentByClass<UFBCaseJournalComponent>() : nullptr)
	{
		Journal->AddClue(GNightClue);
	}
	if (bInstant)
	{
		return;
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (AFBHUD* HUD = Cast<AFBHUD>(PC->GetHUD()))
		{
			HUD->ShowTitleCard(FText::GetEmpty(), NSLOCTEXT("FB", "ThatNight", "That night"), NSLOCTEXT("FB", "NightTime", "21:40"), 4.f);
		}
	}
	World->GetTimerManager().SetTimer(FadeInTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetIgnoreMoveInput(false);
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 2.5f, FLinearColor::Black, false, false);
			}
		}
		for (TActorIterator<AFBCompanion> It(GetWorld()); It; ++It)
		{
			It->Bark({ NSLOCTEXT("FB", "ZackNight", "Mill first. If those keys are still hanging in there, salt and fire on the bones won't hold him. Keys, then the grave. Stay in my light.") }, TEXT("NightStart"), 0.f);
			break;
		}
	}), 3.5f, false);
}

void UFBCaseDirector::SendTownHome()
{
	// After dark the streets empty: the story people stay where the case needs them, one walker is left.
	TArray<AFBTownsfolk*> Walkers;
	for (TActorIterator<AFBTownsfolk> It(GetWorld()); It; ++It)
	{
		if (It->FBStoryPreset != TEXT("MotelClerk")) // the clerk is on the night shift; everyone else goes home
		{
			Walkers.Add(*It);
		}
	}
	const APawn* Player = GetPlayer();
	int32 Keep = INDEX_NONE;
	float Best = 0.f;
	for (int32 I = 0; I < Walkers.Num(); ++I)
	{
		// Keep one wanderer who is not right next to the player (nobody vanishes in front of you).
		const float D = Player ? FVector::Dist2D(Walkers[I]->GetActorLocation(), Player->GetActorLocation()) : 0.f;
		if (Walkers[I]->FBStoryPreset.IsNone() && D > 3000.f && D > Best)
		{
			Best = D;
			Keep = I;
		}
	}
	for (int32 I = 0; I < Walkers.Num(); ++I)
	{
		if (I != Keep)
		{
			Walkers[I]->Destroy();
		}
	}
}

void UFBCaseDirector::SpawnLastSeenSpots()
{
	UWorld* World = GetWorld();
	if (TActorIterator<AFBLastSeenSpot>(World))
	{
		return; // placed by hand in the level: leave them
	}
	// Spread wide on a line from the ghost's haunt (the mill) to the grave: at the mill, a third of the way, two thirds.
	FVector Haunt = FVector::ZeroVector, Grave = FVector::ZeroVector;
	bool bHaunt = false, bGrave = false;
	for (TActorIterator<AFBGhost> It(World); It; ++It) { Haunt = It->GetActorLocation(); bHaunt = true; break; }
	for (TActorIterator<AFBGraveSite> It(World); It; ++It) { Grave = It->GetActorLocation(); bGrave = true; break; }
	if (!bHaunt || !bGrave)
	{
		return;
	}
	const FVector Line = Grave - Haunt;
	const FVector Side = FVector(-Line.Y, Line.X, 0.f).GetSafeNormal();

	struct FSpot { const TCHAR* Id; const TCHAR* Clue; const TCHAR* Person; const TCHAR* Reading; float Along; float Aside; };
	const FSpot Spots[] =
	{
		{ TEXT("Spot_Decker"), TEXT("Clue_SpotDecker"), TEXT("Ruth Ann Decker"),
		  TEXT("Outside the mill. The reader pegs at 4 and the air drops ten degrees inside one step. A lens cap in the weeds, and under it two grooves in the dirt, heel-width, running away from the building. She didn't walk out of here. She was pulled."),
		  0.04f, 700.f },
		{ TEXT("Spot_Barlow"), TEXT("Clue_SpotBarlow"), TEXT("Hank Barlow"),
		  TEXT("Off the mill road. EMF 4, steady. A steel thermos crushed flat like a beer can, frost on it in the middle of the day. The grass is dead in a strip a yard wide, and the strip keeps going - same direction as the marks at the mill."),
		  0.36f, -500.f },
		{ TEXT("Spot_Whitmore"), TEXT("Clue_SpotWhitmore"), TEXT("Dale Whitmore"),
		  TEXT("The treeline. Truck keys in the mud, the fob still blinking. EMF 4 and climbing the further I walk. One word scratched into the bark at knee height, by someone lying on the ground: CRANE. The drag marks end here. Ahead, through the trees, there's a stone wall."),
		  0.68f, 450.f },
	};
	int32 Made = 0;
	for (const FSpot& S : Spots)
	{
		// Find open ground near the wanted point: straight down from high up, the first hit must be walkable and low.
		for (int32 Try = 0; Try < 8; ++Try)
		{
			const FVector P = Haunt + Line * (S.Along + Try * 0.02f) + Side * (S.Aside + Try * 180.f);
			FHitResult Hit;
			const FVector From(P.X, P.Y, FMath::Max(Haunt.Z, Grave.Z) + 6000.f);
			if (!World->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, 20000.f), ECC_WorldStatic) || Hit.ImpactNormal.Z < 0.8f)
			{
				continue;
			}
			// Under a roof / on a roof: something else right above or far below the hit means it isn't open ground.
			FHitResult Below;
			if (World->LineTraceSingleByChannel(Below, Hit.ImpactPoint - FVector(0.f, 0.f, 30.f), Hit.ImpactPoint - FVector(0.f, 0.f, 900.f), ECC_WorldStatic)
				&& Below.Distance > 120.f)
			{
				continue; // we hit a roof, there is a floor under it
			}
			const FTransform T(FRotator::ZeroRotator, Hit.ImpactPoint + FVector(0.f, 0.f, 45.f));
			if (AFBLastSeenSpot* Spot = World->SpawnActorDeferred<AFBLastSeenSpot>(AFBLastSeenSpot::StaticClass(), T))
			{
				Spot->SpotId = FName(S.Id);
				Spot->Person = FText::FromString(S.Person);
				Spot->Reading = FText::FromString(S.Reading);
				Spot->GrantsClues = { FName(S.Clue) };
				Spot->FinishSpawning(T);
				++Made;
				UE_LOG(LogTemp, Log, TEXT("FBCaseDirector: last-seen spot %s at %s"), S.Id, *T.GetLocation().ToCompactString());
			}
			break;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("FBCaseDirector: %d last-seen spots (haunt %s, grave %s)."), Made, *Haunt.ToCompactString(), *Grave.ToCompactString());
}

void UFBCaseDirector::TravelToNight()
{
	APawn* Player = GetPlayer();
	UFBCaseCarryOver* Carry = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UFBCaseCarryOver>() : nullptr;
	if (Player && Carry)
	{
		if (UFBCaseJournalComponent* J = Player->FindComponentByClass<UFBCaseJournalComponent>())
		{
			J->AddClue(GNightClue);
			Carry->Entries = J->GetEntries();
			Carry->Clues = J->GetClueArray();
		}
		if (const UFBInventoryComponent* Inv = Player->FindComponentByClass<UFBInventoryComponent>())
		{
			Carry->Items = Inv->GetItems();
		}
		Carry->bPending = true;
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetIgnoreMoveInput(false);
	}
	UGameplayStatics::OpenLevel(this, FName(*NightLevelPath));
}

void UFBCaseDirector::NightIntro()
{
	// Black screen, "That night", then the dark.
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 5.f, FLinearColor::Black, false, false);
	}
	if (AFBHUD* HUD = Cast<AFBHUD>(PC->GetHUD()))
	{
		HUD->ShowTitleCard(FText::GetEmpty(), NSLOCTEXT("FB", "ThatNight", "That night"), NSLOCTEXT("FB", "NightPlace", "Old Mill Road  -  21:40"), 5.f);
	}
	GetWorld()->GetTimerManager().SetTimer(FadeInTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		for (TActorIterator<AFBCompanion> It(GetWorld()); It; ++It)
		{
			It->Bark({ NSLOCTEXT("FB", "ZackNight", "Mill first. If those keys are still hanging in there, salt and fire on the bones won't hold him. Keys, then the grave. Stay in my light.") }, TEXT("NightStart"), 0.f);
			break;
		}
	}), 4.f, false);
}
