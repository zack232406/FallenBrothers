// Fallen Brothers - Physical creature.

#include "Creatures/FBCreature.h"

#include "Core/FBNoiseSubsystem.h"
#include "Save/FBSaveSubsystem.h"
#include "Companion/FBCompanion.h"
#include "Combat/FBHealthComponent.h"
#include "Equipment/FBEquipmentComponent.h"
#include "Equipment/FBFlashlight.h"
#include "Interaction/FBDoor.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Player/FBBodycamComponent.h"
#include "UI/FBHUD.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBCreature::AFBCreature()
{
	PrimaryActorTick.bCanEverTick = true;

	// ~2 m tall.
	GetCapsuleComponent()->InitCapsuleSize(34.f, 100.f);

	// Spawned / placed creatures get an AI controller automatically.
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 420.f, 0.f);
	Move->MaxWalkSpeed = WanderSpeed;

	GetMesh()->SetHiddenInGame(true); // Placeholder shapes until we have a real model.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Placeholder body: tall, thin, hunched (capsule centre is 100 cm above the floor).
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(0.32f, 0.26f, 1.75f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -12.f));
	Body->SetRelativeRotation(FRotator(-8.f, 0.f, 0.f)); // Leaning forward
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(GetCapsuleComponent());
	Head->SetStaticMesh(Sphere.Object);
	Head->SetRelativeScale3D(FVector(0.24f, 0.2f, 0.3f));
	Head->SetRelativeLocation(FVector(18.f, 0.f, 82.f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (BasicMaterial.Succeeded())
	{
		Body->SetMaterial(0, BasicMaterial.Object);
		Head->SetMaterial(0, BasicMaterial.Object);
	}
}

void AFBCreature::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();
	Health = MaxHealth;

	if (UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.11f, 0.1f));
	}
	if (UMaterialInstanceDynamic* MID = Head->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.52f, 0.48f));
	}

	if (UFBNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFBNoiseSubsystem>())
	{
		NoiseHandle = Noise->OnNoise.AddUObject(this, &AFBCreature::HandleNoise);
	}

	SetState(EFBCreatureState::Wander);
}

void AFBCreature::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UFBNoiseSubsystem* Noise = World->GetSubsystem<UFBNoiseSubsystem>())
		{
			Noise->OnNoise.Remove(NoiseHandle);
		}
	}
	Super::EndPlay(EndPlayReason);
}

APawn* AFBCreature::GetChaseTarget(APawn* Player)
{
	if (APawn* Target = ChaseTarget.Get())
	{
		const AFBCompanion* Brother = Cast<AFBCompanion>(Target);
		if (!Brother || !Brother->IsDowned())
		{
			return Target;
		}
		ChaseTarget.Reset(); // He's down - go for the player.
	}
	return Player;
}

AAIController* AFBCreature::GetAI() const
{
	return Cast<AAIController>(GetController());
}

void AFBCreature::Notify(const FText& Message) const
{
	AFBHUD::ShowNotificationFor(UGameplayStatics::GetPlayerPawn(this, 0), Message, 2.5f);
}

AFBFlashlight* AFBCreature::GetChestLight(const APawn* Player)
{
	const UFBEquipmentComponent* Equipment = Player ? Player->FindComponentByClass<UFBEquipmentComponent>() : nullptr;
	return Equipment ? Equipment->GetFlashlight() : nullptr;
}

// ------------------------------------------------------------------ State machine

void AFBCreature::SetState(EFBCreatureState NewState)
{
	State = NewState;
	StateStartTime = GetWorld()->GetTimeSeconds();

	float Speed = WanderSpeed;
	switch (NewState)
	{
	case EFBCreatureState::Investigate: Speed = InvestigateSpeed; break;
	case EFBCreatureState::Chase:       Speed = ChaseSpeed; break;
	case EFBCreatureState::Flee:        Speed = FleeSpeed; break;
	default: break;
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	if (AAIController* AI = GetAI())
	{
		AI->StopMovement();
	}

	OnEnterState(NewState);
}

void AFBCreature::OnEnterState(EFBCreatureState NewState)
{
	const float Now = GetWorld()->GetTimeSeconds();
	AAIController* AI = GetAI();

	switch (NewState)
	{
	case EFBCreatureState::Wander:
		WanderPauseUntil = Now + FMath::FRandRange(1.f, 3.f);
		break;

	case EFBCreatureState::Investigate:
		InvestigateArriveTime = -1.f;
		if (AI)
		{
			AI->MoveToLocation(InvestigateLocation, 80.f, true, true, /*bProjectDestinationToNavigation*/ true);
		}
		break;

	case EFBCreatureState::Chase:
		NextChaseRepath = 0.f;
		break;

	case EFBCreatureState::Flee:
	{
		// Run to the reachable spot furthest from the player.
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (AI && NavSys && Player)
		{
			FVector Best = GetActorLocation();
			float BestDist = 0.f;
			for (int32 i = 0; i < 8; ++i)
			{
				FNavLocation Candidate;
				if (NavSys->GetRandomReachablePointInRadius(GetActorLocation(), 1500.f, Candidate))
				{
					const float D = FVector::Dist(Candidate.Location, Player->GetActorLocation());
					if (D > BestDist)
					{
						BestDist = D;
						Best = Candidate.Location;
					}
				}
			}
			AI->MoveToLocation(Best, 50.f);
		}
		break;
	}

	case EFBCreatureState::Stagger:
		break;

	default:
		break;
	}
}

void AFBCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (State == EFBCreatureState::Dead)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UFBHealthComponent* PlayerHealth = Player ? Player->FindComponentByClass<UFBHealthComponent>() : nullptr;
	if (PlayerHealth && PlayerHealth->IsDead())
	{
		Player = nullptr; // Nothing left to hunt.
	}

	// --- Sight (5 times a second is plenty). Hunts the player AND the brother.
	if (Now >= NextSenseTime)
	{
		NextSenseTime = Now + 0.2f;

		TArray<APawn*> Candidates;
		if (Player)
		{
			Candidates.Add(Player);
		}
		for (TActorIterator<AFBCompanion> It(GetWorld()); It; ++It)
		{
			if (*It && !It->IsDowned())
			{
				Candidates.Add(*It);
			}
		}

		APawn* Seen = nullptr;
		float SeenDist = TNumericLimits<float>::Max();
		for (APawn* Candidate : Candidates)
		{
			const float D = FVector::Dist(Candidate->GetActorLocation(), GetActorLocation());
			if (D < SeenDist && CanSeePlayer(Candidate))
			{
				Seen = Candidate;
				SeenDist = D;
			}
		}

		if (Seen)
		{
			ChaseTarget = Seen;
			LastSeenTime = Now;
			LastKnownPlayerLocation = Seen->GetActorLocation();

			if (State == EFBCreatureState::Wander || State == EFBCreatureState::Investigate)
			{
				if (!bAnnouncedSpotted)
				{
					bAnnouncedSpotted = true;
					Notify(NSLOCTEXT("FB", "CreatureSpotted", "Something tall unfolds out of the dark... it's seen you. RUN."));
				}
				SetState(EFBCreatureState::Chase);
			}
		}
	}

	// --- Light: hold your flashlight on it and it recoils.
	if (bAfraidOfLight && Player && State != EFBCreatureState::Flee && State != EFBCreatureState::Stagger)
	{
		LitTime = IsLitByPlayer(Player) ? LitTime + DeltaSeconds : FMath::Max(0.f, LitTime - DeltaSeconds);
		if (LitTime >= LightTolerance)
		{
			LitTime = 0.f;
			if (!bAnnouncedLight)
			{
				bAnnouncedLight = true;
				Notify(NSLOCTEXT("FB", "CreatureLight", "It shrieks and shields its eyes - it hates the light!"));
			}
			SetState(EFBCreatureState::Flee);
		}
	}

	switch (State)
	{
	case EFBCreatureState::Wander:      TickWander(); break;
	case EFBCreatureState::Investigate: TickInvestigate(DeltaSeconds); break;
	case EFBCreatureState::Chase:       TickChase(DeltaSeconds, GetChaseTarget(Player)); break;
	case EFBCreatureState::Flee:        TickFlee(); break;
	case EFBCreatureState::Stagger:     TickStagger(); break;
	default: break;
	}

	if (bOpensDoors && Now >= NextDoorCheck)
	{
		NextDoorCheck = Now + 0.25f;
		OpenNearbyDoors();
	}
}

void AFBCreature::TickWander()
{
	AAIController* AI = GetAI();
	if (!AI || AI->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		return; // Still walking somewhere.
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (WanderPauseUntil <= 0.f)
	{
		WanderPauseUntil = Now + FMath::FRandRange(2.f, 5.f); // Stand still a moment, listening.
		return;
	}
	if (Now < WanderPauseUntil)
	{
		return;
	}
	WanderPauseUntil = 0.f;

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Point;
		if (NavSys->GetRandomReachablePointInRadius(HomeLocation, WanderRadius, Point))
		{
			AI->MoveToLocation(Point.Location, 50.f);
		}
	}
}

void AFBCreature::TickInvestigate(float DeltaSeconds)
{
	AAIController* AI = GetAI();
	if (!AI || AI->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}

	// Arrived (or couldn't get there): look around for a few seconds.
	const float Now = GetWorld()->GetTimeSeconds();
	if (InvestigateArriveTime < 0.f)
	{
		InvestigateArriveTime = Now;
	}
	AddActorWorldRotation(FRotator(0.f, 70.f * DeltaSeconds * FMath::Sin((Now - InvestigateArriveTime) * 1.5f), 0.f));

	if (Now - InvestigateArriveTime > 3.5f)
	{
		SetState(EFBCreatureState::Wander);
	}
}

void AFBCreature::TickChase(float DeltaSeconds, APawn* Player)
{
	const float Now = GetWorld()->GetTimeSeconds();
	AAIController* AI = GetAI();
	if (!Player || !AI)
	{
		SetState(EFBCreatureState::Wander);
		return;
	}

	// Lost you? Go search where it last saw you.
	if (Now - LastSeenTime > LoseSightTime)
	{
		InvestigateLocation = LastKnownPlayerLocation;
		SetState(EFBCreatureState::Investigate);
		return;
	}

	const float Dist = FVector::Dist(Player->GetActorLocation(), GetActorLocation());
	if (Dist <= AttackRange)
	{
		AI->StopMovement();
		FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.f;
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), ToPlayer.Rotation(), DeltaSeconds, 10.f));
		TryAttack(Player);
		return;
	}

	// (Re)start following. MoveToActor keeps tracking the player while it runs.
	if (Now >= NextChaseRepath && AI->GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		NextChaseRepath = Now + 0.3f;
		AI->MoveToActor(Player, AttackRange * 0.6f);
	}
}

void AFBCreature::TickFlee()
{
	if (GetWorld()->GetTimeSeconds() - StateStartTime >= FleeTime)
	{
		// It doesn't give up: it comes back to where it last saw you.
		InvestigateLocation = LastKnownPlayerLocation.IsZero() ? HomeLocation : LastKnownPlayerLocation;
		SetState(EFBCreatureState::Investigate);
	}
}

void AFBCreature::TickStagger()
{
	if (GetWorld()->GetTimeSeconds() - StateStartTime >= StaggerTime)
	{
		// Furious now: goes straight for you.
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			LastSeenTime = GetWorld()->GetTimeSeconds();
			LastKnownPlayerLocation = Player->GetActorLocation();
		}
		SetState(EFBCreatureState::Chase);
	}
}

// ------------------------------------------------------------------ Senses

bool AFBCreature::CanSeePlayer(APawn* Player) const
{
	const FVector Eyes = Head->GetComponentLocation();
	const FVector Target = Player->GetPawnViewLocation();
	const FVector To = Target - Eyes;
	const float Dist = To.Size();

	float Range = SightRange;
	if (const ACharacter* PlayerChar = Cast<ACharacter>(Player))
	{
		if (PlayerChar->bIsCrouched)
		{
			Range *= 0.6f; // Low and quiet = harder to spot.
		}
	}
	if (const AFBFlashlight* Light = GetChestLight(Player))
	{
		if (Light->IsLightOn())
		{
			Range *= 1.5f; // Your light gives you away in the dark.
		}
	}
	if (Dist > Range)
	{
		return false;
	}

	// Very close: it senses you even behind it.
	if (Dist > 250.f)
	{
		const float Cos = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));
		if (FVector::DotProduct(GetActorForwardVector(), To / Dist) < Cos)
		{
			return false;
		}
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CreatureSight), false, this);
	Params.AddIgnoredActor(Player);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Target, ECC_Visibility, Params);
}

bool AFBCreature::IsLitByPlayer(APawn* Player) const
{
	const UFBEquipmentComponent* Equipment = Player->FindComponentByClass<UFBEquipmentComponent>();
	if (!Equipment)
	{
		return false;
	}

	// Chest flashlight, or a light held in hand (UV light counts too).
	const AFBFlashlight* Lights[2] = { Equipment->GetFlashlight(), Cast<AFBFlashlight>(Equipment->GetActiveEquipment()) };
	for (const AFBFlashlight* Light : Lights)
	{
		if (Light && (Light->IsPointLit(Head->GetComponentLocation(), this) || Light->IsPointLit(Body->GetComponentLocation(), this)))
		{
			return true;
		}
	}
	return false;
}

void AFBCreature::HandleNoise(const FVector& Location, float Loudness, AActor* NoiseInstigator)
{
	if (State == EFBCreatureState::Dead || State == EFBCreatureState::Flee || State == EFBCreatureState::Stagger || NoiseInstigator == this)
	{
		return;
	}
	if (FVector::Dist(Location, GetActorLocation()) > HearingRange * Loudness)
	{
		return; // Too far / too quiet.
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (State == EFBCreatureState::Chase && Now - LastSeenTime < LoseSightTime)
	{
		return; // Busy hunting you.
	}

	InvestigateLocation = Location;
	if (State == EFBCreatureState::Investigate)
	{
		// New sound while searching: turn toward it.
		InvestigateArriveTime = -1.f;
		if (AAIController* AI = GetAI())
		{
			AI->MoveToLocation(InvestigateLocation, 80.f, true, true, true);
		}
	}
	else
	{
		SetState(EFBCreatureState::Investigate);
	}
}

// ------------------------------------------------------------------ Actions

void AFBCreature::TryAttack(APawn* Player)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown)
	{
		return;
	}
	LastAttackTime = Now;

	if (UFBHealthComponent* PlayerHealth = Player->FindComponentByClass<UFBHealthComponent>())
	{
		PlayerHealth->ApplyDamage(AttackDamage, this);
	}
	if (ACharacter* PlayerChar = Cast<ACharacter>(Player))
	{
		FVector Push = Player->GetActorLocation() - GetActorLocation();
		Push.Z = 0.f;
		PlayerChar->LaunchCharacter(Push.GetSafeNormal() * AttackKnockback + FVector(0.f, 0.f, 150.f), true, true);
	}
	if (UFBBodycamComponent* Bodycam = Player->FindComponentByClass<UFBBodycamComponent>())
	{
		Bodycam->AddKick(FRotator(10.f, FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(-15.f, 15.f)));
	}
}

void AFBCreature::OpenNearbyDoors()
{
	const FVector MyLoc = GetActorLocation();
	for (TActorIterator<AFBDoor> It(GetWorld()); It; ++It)
	{
		AFBDoor* Door = *It;
		if (!Door || Door->IsOpen() || Door->IsLocked())
		{
			continue;
		}
		const FVector ToDoor = Door->GetActorLocation() - MyLoc;
		if (ToDoor.Size2D() < 140.f && FVector::DotProduct(GetActorForwardVector(), ToDoor.GetSafeNormal2D()) > 0.2f)
		{
			Door->SetOpen(true, this);
		}
	}
}

void AFBCreature::ApplyHit(const FFBHitInfo& Hit)
{
	if (State == EFBCreatureState::Dead)
	{
		return;
	}

	// Knocked back either way.
	const FVector From = Hit.Causer ? Hit.Causer->GetActorLocation() : Hit.Location;
	FVector Away = GetActorLocation() - From;
	Away.Z = 0.f;
	LaunchCharacter(Away.GetSafeNormal() * 350.f, true, false);

	if (Weaknesses.Contains(Hit.Kind))
	{
		Health -= Hit.Amount;
		if (Health <= 0.f)
		{
			Die(Hit);
			return;
		}
		Notify(NSLOCTEXT("FB", "CreatureWounded", "It screams - the wound smokes!"));
	}
	else
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastNoEffectMessage > 4.f)
		{
			LastNoEffectMessage = Now;
			Notify(NSLOCTEXT("FB", "CreatureNoEffect", "It staggers... but it isn't hurt. That's not going to kill it."));
		}
	}

	SetState(EFBCreatureState::Stagger);
}

void AFBCreature::Die(const FFBHitInfo& KillingHit)
{
	State = EFBCreatureState::Dead;
	UFBSaveSubsystem::NotifyActorRemoved(this); // Stays dead after loading.

	if (AAIController* AI = GetAI())
	{
		AI->StopMovement();
	}
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Collapse: the placeholder body becomes a physics object and falls over.
	Head->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	const FVector From = KillingHit.Causer ? KillingHit.Causer->GetActorLocation() : KillingHit.Location;
	Body->AddImpulse((GetActorLocation() - From).GetSafeNormal() * 300.f + FVector(0.f, 0.f, 100.f), NAME_None, true);

	Notify(FText::Format(NSLOCTEXT("FB", "CreatureDead", "{0} collapses. It's dead."), CreatureName));

	if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (UFBCaseJournalComponent* Journal = Player->FindComponentByClass<UFBCaseJournalComponent>())
		{
			FFBJournalEntry Entry;
			Entry.Id = FName(*FString::Printf(TEXT("Killed_%s"), *GetName()));
			Entry.Title = FText::Format(NSLOCTEXT("FB", "CreatureKilledTitle", "Killed: {0}"), CreatureName);
			Entry.Text = NSLOCTEXT("FB", "CreatureKilledText", "Silver did what nothing else could. It's over - for this one.");
			Entry.Type = EFBJournalEntryType::Note;
			Journal->AddEntry(Entry, false);
			Journal->AddClue(TEXT("Creature_Killed"));
		}
	}
}
