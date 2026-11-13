// Fallen Brothers - Poltergeist room.

#include "Paranormal/FBPoltergeist.h"

#include "Core/FBNoiseSubsystem.h"
#include "Equipment/FBEquipmentComponent.h"
#include "Equipment/FBFlashlight.h"
#include "Interaction/FBDoor.h"
#include "Paranormal/FBGhost.h"
#include "Paranormal/FBParanormalSourceComponent.h"
#include "Physics/FBPhysicsLib.h"
#include "Physics/FBTraps.h"
#include "Player/FBBodycamComponent.h"
#include "UI/FBHUD.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AFBPoltergeist::AFBPoltergeist()
{
	EMFSource = CreateDefaultSubobject<UFBParanormalSourceComponent>(TEXT("EMFSource"));
	EMFSource->SetupAttachment(Area);
	EMFSource->EMFStrength = 1.f;
	EMFSource->Radius = 900.f;
}

void AFBPoltergeist::BeginPlay()
{
	Super::BeginPlay();
	BaseEMF = EMFSource->EMFStrength;
	bHadGhost = IsValid(LinkedGhost);
	EMFSource->Radius = FMath::Max(EMFSource->Radius, GetBoxExtent().Size2D() + 300.f);
	NextEventTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(2.f, 4.f);
}

bool AFBPoltergeist::IsActiveNow(APawn* Player) const
{
	if (!Player)
	{
		return false;
	}
	// The ghost behind it is gone for good: the house is finally quiet.
	if (bHadGhost && !IsValid(LinkedGhost))
	{
		return false;
	}
	if (IsValid(LinkedGhost) && LinkedGhost->GetState() == EFBGhostState::Banished)
	{
		return false;
	}
	return !bOnlyWhenPlayerInside || IsInside(Player->GetActorLocation());
}

void AFBPoltergeist::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bActive = IsActiveNow(Player);

	UpdateMotion(Now, Player);

	// EMF spike fades back to the faint background level.
	if (Now > EMFPulseEnd)
	{
		EMFSource->EMFStrength = bActive ? BaseEMF : 0.f;
	}

	const bool bInside = Player && IsInside(Player->GetActorLocation());
	if (bInside && !bPlayerWasInside && bScareOnFirstEnter && !bScared && bActive)
	{
		bScared = true;
		Burst();
		AFBHUD::ShowNotificationFor(Player, NSLOCTEXT("FB", "PolterEnter", "The air turns ice cold. Everything in the room starts to move."), 3.f);
	}
	bPlayerWasInside = bInside;

	if (!bActive || Now < NextEventTime)
	{
		return;
	}

	float Rate = FMath::Lerp(0.6f, 2.f, Activity);
	if (IsValid(LinkedGhost) && LinkedGhost->GetState() == EFBGhostState::Manifested)
	{
		Rate *= 2.f;
	}
	NextEventTime = Now + FMath::FRandRange(EventIntervalMin, EventIntervalMax) / Rate;
	DoRandomEvent(Player);
}

void AFBPoltergeist::Burst()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (bRattle)        EventRattle(12, 2.5f);
	if (bSlamDoors)     EventDoors();
	if (bFlickerLights) EventLights(Player, 5.f);
	if (bSwingLamps)    EventLamps();
	if (bLevitate)      EventLevitate(Player);

	if (Player)
	{
		if (UFBBodycamComponent* Cam = Player->FindComponentByClass<UFBBodycamComponent>())
		{
			Cam->AddKick(FRotator(3.f, FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-4.f, 4.f)));
		}
	}
	UFBNoiseSubsystem::ReportNoise(this, GetActorLocation(), 1.2f, this);
}

void AFBPoltergeist::DoRandomEvent(APawn* Player)
{
	// Weighted pick among the enabled events.
	struct FOption { int32 Id; float Weight; };
	TArray<FOption, TInlineAllocator<6>> Options;
	if (bRattle)        Options.Add({ 0, 3.f });
	if (bLevitate)      Options.Add({ 1, 2.f });
	if (bThrow)         Options.Add({ 2, 1.f + Activity });
	if (bSlamDoors)     Options.Add({ 3, 1.5f });
	if (bFlickerLights) Options.Add({ 4, 1.5f });
	if (bSwingLamps)    Options.Add({ 5, 1.f });

	float Total = 0.f;
	for (const FOption& O : Options) Total += O.Weight;
	if (Total <= 0.f)
	{
		return;
	}

	float Roll = FMath::FRandRange(0.f, Total);
	int32 Pick = Options.Last().Id;
	for (const FOption& O : Options)
	{
		if (Roll < O.Weight) { Pick = O.Id; break; }
		Roll -= O.Weight;
	}

	switch (Pick)
	{
	case 0: EventRattle(FMath::RandRange(2, 5), FMath::FRandRange(0.8f, 2.f)); break;
	case 1: EventLevitate(Player); break;
	case 2: EventThrow(Player); break;
	case 3: EventDoors(); break;
	case 4: EventLights(Player, 4.f); break;
	case 5: EventLamps(); break;
	default: break;
	}
}

// ------------------------------------------------------------------ Events

void AFBPoltergeist::GetMovableBodies(TArray<UPrimitiveComponent*>& Out) const
{
	GetBodiesInside(Out);
	Out.RemoveAll([this](const UPrimitiveComponent* C)
	{
		if (C->GetMass() > MaxLiftMassKg || FFBPhysicsLib::IsHeldByPlayer(this, C))
		{
			return true;
		}
		// Already busy?
		for (const FLevitating& L : Levitating) if (L.Comp.Get() == C) return true;
		for (const FThrown& T : Thrown)         if (T.Comp.Get() == C) return true;
		return false;
	});
}

void AFBPoltergeist::EventRattle(int32 Count, float Duration)
{
	TArray<UPrimitiveComponent*> Bodies;
	GetMovableBodies(Bodies);
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Count && Bodies.Num() > 0; ++i)
	{
		UPrimitiveComponent* C = Bodies[FMath::RandRange(0, Bodies.Num() - 1)];
		Bodies.RemoveSingleSwap(C);
		FRattling R;
		R.Comp = C;
		R.EndTime = Now + Duration * FMath::FRandRange(0.7f, 1.2f);
		Rattling.Add(R);
	}
	UFBNoiseSubsystem::ReportNoise(this, GetActorLocation(), 0.4f, this);
}

void AFBPoltergeist::EventLevitate(APawn* Player)
{
	TArray<UPrimitiveComponent*> Bodies;
	GetMovableBodies(Bodies);
	if (Bodies.Num() == 0)
	{
		return;
	}
	UPrimitiveComponent* C = Bodies[FMath::RandRange(0, Bodies.Num() - 1)];

	FLevitating L;
	L.Comp = C;
	L.Start = C->GetComponentLocation();
	L.StartTime = GetWorld()->GetTimeSeconds();
	L.Phase = FMath::FRandRange(0.f, 6.28f);
	L.bThrowAfter = bThrow && Player && FMath::FRand() < LevitateThrowChance;
	Levitating.Add(L);

	C->SetEnableGravity(false);
	C->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * FMath::FRandRange(20.f, 60.f));
	C->WakeAllRigidBodies();
	EventLights(Player, 3.f);
}

void AFBPoltergeist::EventThrow(APawn* Player)
{
	if (!Player)
	{
		return;
	}
	TArray<UPrimitiveComponent*> Bodies;
	GetMovableBodies(Bodies);
	Bodies.RemoveAll([Player](const UPrimitiveComponent* C)
	{
		return FVector::Dist(C->GetComponentLocation(), Player->GetActorLocation()) < 200.f;
	});
	if (Bodies.Num() > 0)
	{
		ThrowAt(Bodies[FMath::RandRange(0, Bodies.Num() - 1)], Player);
	}
}

void AFBPoltergeist::ThrowAt(UPrimitiveComponent* Comp, APawn* Player)
{
	if (!Comp || !Player)
	{
		return;
	}
	Comp->SetEnableGravity(true);
	const FVector Target = Player->GetActorLocation() + FVector(0.f, 0.f, 40.f) + FMath::VRand() * 40.f;
	const FVector Dir = (Target - Comp->GetComponentLocation()).GetSafeNormal();
	Comp->SetPhysicsLinearVelocity(Dir * ThrowSpeed + FVector(0.f, 0.f, 120.f));
	Comp->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 500.f);

	FThrown T;
	T.Comp = Comp;
	T.EndTime = GetWorld()->GetTimeSeconds() + 1.5f;
	Thrown.Add(T);

	if (UFBEquipmentComponent* Eq = Player->FindComponentByClass<UFBEquipmentComponent>())
	{
		if (AFBFlashlight* Light = Eq->GetFlashlight())
		{
			Light->AddInterference(0.6f, 0.6f);
		}
	}
	UFBNoiseSubsystem::ReportNoise(this, Comp->GetComponentLocation(), 0.6f, this);
}

void AFBPoltergeist::EventDoors()
{
	for (TActorIterator<AFBDoor> It(GetWorld()); It; ++It)
	{
		AFBDoor* Door = *It;
		if (!IsInside(Door->GetActorLocation()) || FMath::FRand() < 0.3f)
		{
			continue;
		}
		if (Door->IsOpen())
		{
			Door->SetOpen(false, this); // SLAM
		}
		else if (!Door->IsLocked())
		{
			Door->SetOpen(true, this);
		}
	}
	UFBNoiseSubsystem::ReportNoise(this, GetActorLocation(), 0.9f, this);
}

void AFBPoltergeist::EventLights(APawn* Player, float Strength)
{
	EMFSource->EMFStrength = FMath::Max(BaseEMF, Strength); // street lamps flicker, EMF reader spikes
	EMFPulseEnd = GetWorld()->GetTimeSeconds() + 1.5f;

	if (Player)
	{
		if (UFBEquipmentComponent* Eq = Player->FindComponentByClass<UFBEquipmentComponent>())
		{
			if (AFBFlashlight* Light = Eq->GetFlashlight())
			{
				Light->AddInterference(FMath::Clamp(Strength / 5.f, 0.2f, 1.f), 1.2f);
			}
		}
	}
}

void AFBPoltergeist::EventLamps()
{
	for (TActorIterator<AFBSwingingLamp> It(GetWorld()); It; ++It)
	{
		if (IsInside(It->GetActorLocation()))
		{
			It->Push(FMath::VRand().GetSafeNormal2D() * FMath::FRandRange(150.f, 350.f));
		}
	}
}

// ------------------------------------------------------------------ Motion

void AFBPoltergeist::UpdateMotion(float Now, APawn* Player)
{
	// Rattling: small hops and twitches.
	for (int32 i = Rattling.Num() - 1; i >= 0; --i)
	{
		FRattling& R = Rattling[i];
		UPrimitiveComponent* C = R.Comp.Get();
		if (!C || Now > R.EndTime || FFBPhysicsLib::IsHeldByPlayer(this, C))
		{
			Rattling.RemoveAtSwap(i);
			continue;
		}
		if (Now >= R.NextKick)
		{
			R.NextKick = Now + FMath::FRandRange(0.06f, 0.16f);
			const FVector Kick(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(40.f, 90.f));
			C->AddImpulse(Kick, NAME_None, true);
			C->AddAngularImpulseInDegrees(FMath::VRand() * 150.f, NAME_None, true);
		}
	}

	// Levitating: rise, float, slowly turn - then drop or get thrown.
	for (int32 i = Levitating.Num() - 1; i >= 0; --i)
	{
		FLevitating& L = Levitating[i];
		UPrimitiveComponent* C = L.Comp.Get();
		if (!C)
		{
			Levitating.RemoveAtSwap(i);
			continue;
		}
		if (FFBPhysicsLib::IsHeldByPlayer(this, C))
		{
			C->SetEnableGravity(true); // you tore it out of the ghost's grip
			Levitating.RemoveAtSwap(i);
			continue;
		}

		const float T = (Now - L.StartTime) / FMath::Max(0.5f, LevitateTime);
		if (T >= 1.f)
		{
			C->SetEnableGravity(true);
			if (L.bThrowAfter && Player)
			{
				ThrowAt(C, Player);
			}
			Levitating.RemoveAtSwap(i);
			continue;
		}

		const float Rise = FMath::InterpEaseOut(0.f, 1.f, FMath::Min(T * 3.f, 1.f), 2.f);
		const FVector Wobble(FMath::Sin(Now * 1.7f + L.Phase) * 8.f, FMath::Cos(Now * 1.3f + L.Phase) * 8.f, FMath::Sin(Now * 2.3f + L.Phase) * 5.f);
		const FVector Target = L.Start + FVector(0.f, 0.f, LevitateHeight * Rise) + Wobble;
		C->SetPhysicsLinearVelocity((Target - C->GetComponentLocation()) * 4.f);
	}

	// Thrown: hurts whoever it smashes into.
	for (int32 i = Thrown.Num() - 1; i >= 0; --i)
	{
		FThrown& T = Thrown[i];
		UPrimitiveComponent* C = T.Comp.Get();
		if (!C || Now > T.EndTime)
		{
			Thrown.RemoveAtSwap(i);
			continue;
		}
		if (FFBPhysicsLib::HurtCharactersTouching(C, 350.f, ThrowDamageScale, this, T.AlreadyHit))
		{
			if (Player && T.AlreadyHit.Contains(Player))
			{
				if (UFBBodycamComponent* Cam = Player->FindComponentByClass<UFBBodycamComponent>())
				{
					Cam->AddKick(FRotator(6.f, FMath::FRandRange(-5.f, 5.f), FMath::FRandRange(-8.f, 8.f)));
				}
			}
		}
	}
}
