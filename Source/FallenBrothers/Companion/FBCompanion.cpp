// Fallen Brothers - AI brother (Zack).

#include "Companion/FBCompanion.h"

#include "Core/FBRealModels.h"

#include "Combat/FBHealthComponent.h"
#include "Core/FBNoiseSubsystem.h"
#include "Creatures/FBCreature.h"
#include "Interaction/FBDoor.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Paranormal/FBGhost.h"
#include "UI/FBHUD.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
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

AFBCompanion::AFBCompanion()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(36.f, 92.f);
	// Block the interaction trace so you can look at him and press F.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 480.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	GetMesh()->SetHiddenInGame(true); // Placeholder shapes until the real model.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(0.48f, 0.36f, 1.55f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -14.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(GetCapsuleComponent());
	Head->SetStaticMesh(Sphere.Object);
	Head->SetRelativeScale3D(FVector(0.24f));
	Head->SetRelativeLocation(FVector(0.f, 0.f, 77.f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (BasicMaterial.Succeeded())
	{
		Body->SetMaterial(0, BasicMaterial.Object);
		Head->SetMaterial(0, BasicMaterial.Object);
	}

	MuzzleFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlash"));
	MuzzleFlash->SetupAttachment(GetCapsuleComponent());
	MuzzleFlash->SetRelativeLocation(FVector(60.f, 15.f, 40.f));
	MuzzleFlash->IntensityUnits = ELightUnits::Candelas;
	MuzzleFlash->SetIntensity(800.f);
	MuzzleFlash->SetLightColor(FLinearColor(1.f, 0.75f, 0.45f));
	MuzzleFlash->SetAttenuationRadius(1200.f);
	MuzzleFlash->SetVisibility(false);

	Health = CreateDefaultSubobject<UFBHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 120.f;

	BuildDefaultLines();
}

void AFBCompanion::BeginPlay()
{
	Super::BeginPlay();

	// Zack's real body (if imported): /Game/FallenBrothers/Characters/char_zack.
	// Realistic Renderpeople scan (Nathan) first; the old stylised Zack body only if Nathan isn't imported.
	if (!FBRealModels::DressCharacter(this, GetCapsuleComponent(), TEXT("char_nathan"), 182.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()))
	{
		FBRealModels::DressCharacter(this, GetCapsuleComponent(), TEXT("char_zack"), 182.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
	}

	if (UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), JacketColor);
	}
	if (UMaterialInstanceDynamic* MID = Head->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.56f, 0.45f));
	}

	BodyDefaultLocation = Body->GetRelativeLocation();
	BodyDefaultRotation = Body->GetRelativeRotation();
	HeadDefaultLocation = Head->GetRelativeLocation();

	Health->OnDeath.AddDynamic(this, &AFBCompanion::HandleDowned);

	NextIdleChatter = GetWorld()->GetTimeSeconds() + FMath::FRandRange(25.f, 45.f);
}

AAIController* AFBCompanion::GetAI() const
{
	return Cast<AAIController>(GetController());
}

// ------------------------------------------------------------------ Lines

void AFBCompanion::BuildDefaultLines()
{
	auto L = [](const TCHAR* S) { return FText::FromString(S); };

	IdleLines = {
		L(TEXT("This town's too quiet. I hate quiet.")),
		L(TEXT("If there's a diner here, I'm getting pie. Non-negotiable.")),
		L(TEXT("You smell that? Sulfur... or the motel carpet. Hard to tell.")),
		L(TEXT("Remember when Dad made us sleep in the car for a week? Good times.")),
		L(TEXT("Stay sharp, Chris.")),
		L(TEXT("Every town's got a secret. This one's got a body count."))
	};
	GhostLines = {
		L(TEXT("Cold spot! It's here!")),
		L(TEXT("Chris, behind you!")),
		L(TEXT("Salt it! Salt it now!")),
		L(TEXT("You seeing this? Tell me you're seeing this."))
	};
	CreatureLines = {
		L(TEXT("What the hell is THAT?!")),
		L(TEXT("It's coming - move, move!")),
		L(TEXT("Hit it with the light! It hates the light!")),
		L(TEXT("Silver, Chris! Nothing else is gonna put it down!"))
	};
	CombatLines = {
		L(TEXT("Eat salt!")),
		L(TEXT("Got it!")),
		L(TEXT("Reloading - cover me!"))
	};
	PlayerHurtLines = {
		L(TEXT("Chris! You good?")),
		L(TEXT("Stay on your feet, man!")),
		L(TEXT("Hey! Hang in there!"))
	};
	ClueLines = {
		L(TEXT("Huh. That's something.")),
		L(TEXT("Write that down.")),
		L(TEXT("Okay, that's creepy. Keep digging."))
	};
	KillLines = {
		L(TEXT("...Is it dead? Poke it. No, YOU poke it.")),
		L(TEXT("That's one less.")),
		L(TEXT("Nice. Remind me never to make you mad."))
	};
	FriendlyFireLines = {
		L(TEXT("WATCH IT!")),
		L(TEXT("Hey! Wrong monster!")),
		L(TEXT("Seriously?!"))
	};
	HoldLines = {
		L(TEXT("I'll hold here.")),
		L(TEXT("Fine. I'll guard the... nothing. Go.")),
		L(TEXT("Shout if you need me."))
	};
	FollowLines = {
		L(TEXT("Right behind you.")),
		L(TEXT("Lead the way.")),
		L(TEXT("Yeah, yeah. Coming."))
	};
	DownedLines = {
		L(TEXT("Chris... little help...")),
		L(TEXT("I'm down! I'm down!"))
	};
	RevivedLines = {
		L(TEXT("Thanks. I owe you. Again.")),
		L(TEXT("Ow. Okay. I'm good. Mostly."))
	};
	CaseSolvedLines = {
		L(TEXT("Salted and burned. Let's get out of this town.")),
		L(TEXT("Another one done. Drinks are on you."))
	};
}

bool AFBCompanion::Bark(const TArray<FText>& Lines, FName Key, float Cooldown, float Chance)
{
	if (Lines.Num() == 0 || FMath::FRand() > Chance)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float* Next = NextBarkTime.Find(Key);
	if ((Next && Now < *Next) || Now - LastLineTime < 3.5f)
	{
		return false; // Too soon after the last line.
	}

	NextBarkTime.Add(Key, Now + Cooldown);
	LastLineTime = Now;
	NextIdleChatter = Now + FMath::FRandRange(45.f, 90.f);

	AFBHUD::ShowSubtitleFor(this, CompanionName, Lines[FMath::RandRange(0, Lines.Num() - 1)], 3.5f);
	return true;
}

// ------------------------------------------------------------------ Interaction

FText AFBCompanion::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	switch (State)
	{
	case EFBCompanionState::Downed: return FText::Format(NSLOCTEXT("FB", "CompRevive", "Help {0} up"), CompanionName);
	case EFBCompanionState::Hold:   return FText::Format(NSLOCTEXT("FB", "CompFollow", "{0}: follow me"), CompanionName);
	default:                        return FText::Format(NSLOCTEXT("FB", "CompHold", "{0}: wait here"), CompanionName);
	}
}

void AFBCompanion::Interact_Implementation(AActor* InteractingActor)
{
	LastLineTime = -100.f; // Always answer when spoken to.

	switch (State)
	{
	case EFBCompanionState::Downed:
		Revive();
		break;

	case EFBCompanionState::Hold:
		State = EFBCompanionState::Follow;
		Bark(FollowLines, TEXT("Command"), 0.f);
		break;

	default:
		State = EFBCompanionState::Hold;
		if (AAIController* AI = GetAI())
		{
			AI->StopMovement();
		}
		Bark(HoldLines, TEXT("Command"), 0.f);
		break;
	}
}

// ------------------------------------------------------------------ Damage / downed

void AFBCompanion::ApplyHit(const FFBHitInfo& Hit)
{
	// The player's own shots: complain, but no damage (no accidental brother-killing).
	Bark(FriendlyFireLines, TEXT("FriendlyFire"), 3.f);
}

void AFBCompanion::HandleDowned()
{
	State = EFBCompanionState::Downed;

	if (AAIController* AI = GetAI())
	{
		AI->StopMovement();
	}
	GetCharacterMovement()->DisableMovement();

	// Placeholder "lying on the ground".
	Body->SetRelativeLocation(FVector(0.f, 0.f, -75.f));
	Body->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Head->SetRelativeLocation(FVector(-85.f, 0.f, -75.f));

	LastLineTime = -100.f;
	Bark(DownedLines, TEXT("Downed"), 0.f);
}

void AFBCompanion::Revive()
{
	Health->Revive(ReviveHealthPercent);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	Body->SetRelativeLocation(BodyDefaultLocation);
	Body->SetRelativeRotation(BodyDefaultRotation);
	Head->SetRelativeLocation(HeadDefaultLocation);

	State = EFBCompanionState::Follow;
	Bark(RevivedLines, TEXT("Revived"), 0.f);
}

void AFBCompanion::HandlePlayerHealthChanged(float NewHealth, float Delta)
{
	if (Delta < 0.f && !IsDowned())
	{
		Bark(PlayerHurtLines, TEXT("PlayerHurt"), 10.f, 0.8f);
	}
}

void AFBCompanion::HandlePlayerJournalUpdated()
{
	if (!IsDowned())
	{
		Bark(ClueLines, TEXT("Clue"), 40.f, 0.5f);
	}
}

// ------------------------------------------------------------------ Tick

void AFBCompanion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlashTimeLeft > 0.f)
	{
		FlashTimeLeft -= DeltaSeconds;
		if (FlashTimeLeft <= 0.f)
		{
			MuzzleFlash->SetVisibility(false);
		}
	}

	if (IsDowned())
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	// Listen to the player's health and journal (once - the player spawns after us sometimes).
	if (!bBoundToPlayer)
	{
		bBoundToPlayer = true;
		if (UFBHealthComponent* PlayerHealth = Player->FindComponentByClass<UFBHealthComponent>())
		{
			PlayerHealth->OnHealthChanged.AddDynamic(this, &AFBCompanion::HandlePlayerHealthChanged);
		}
		if (UFBCaseJournalComponent* Journal = Player->FindComponentByClass<UFBCaseJournalComponent>())
		{
			Journal->OnJournalUpdated.AddDynamic(this, &AFBCompanion::HandlePlayerJournalUpdated);
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Fight whatever is close.
	if (Now >= NextFireTime)
	{
		if (AActor* Target = FindCombatTarget(Player))
		{
			ShootAt(Target);
		}
	}

	if (State == EFBCompanionState::Follow)
	{
		TickFollow(Player);
	}

	if (Now >= NextDoorCheck)
	{
		NextDoorCheck = Now + 0.25f;
		OpenDoorsAhead();
	}

	if (Now >= NextWorldCheck)
	{
		NextWorldCheck = Now + 0.5f;
		CheckWorldForBarks(Player);
	}

	if (Now >= NextIdleChatter)
	{
		Bark(IdleLines, TEXT("Idle"), 0.f);
		NextIdleChatter = Now + FMath::FRandRange(50.f, 100.f);
	}
}

void AFBCompanion::TickFollow(APawn* Player)
{
	const float Dist = FVector::Dist(Player->GetActorLocation(), GetActorLocation());

	// Lost somewhere? Pop back in behind the player.
	if (Dist > TeleportDistance)
	{
		const FVector Behind = Player->GetActorLocation() - Player->GetActorForwardVector() * 200.f;
		TeleportTo(Behind, Player->GetActorRotation());
		return;
	}

	GetCharacterMovement()->MaxWalkSpeed = (Dist > CatchUpDistance) ? RunSpeed : WalkSpeed;

	AAIController* AI = GetAI();
	const float Now = GetWorld()->GetTimeSeconds();
	if (AI && Dist > FollowDistance + 60.f && Now >= NextFollowRepath && AI->GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		NextFollowRepath = Now + 0.5f;
		AI->MoveToActor(Player, FollowDistance);
	}
}

// ------------------------------------------------------------------ Combat

bool AFBCompanion::HasLineOfSight(const AActor* Target, const FVector& AimPoint) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CompanionAim), false, this);
	Params.AddIgnoredActor(Target);
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	return !GetWorld()->LineTraceSingleByChannel(Hit, GetPawnViewLocation(), AimPoint, ECC_Visibility, Params);
}

AActor* AFBCompanion::FindCombatTarget(const APawn* Player) const
{
	const FVector MyLoc = GetActorLocation();
	AActor* Best = nullptr;
	float BestDist = CombatRange;

	for (TActorIterator<AFBGhost> It(GetWorld()); It; ++It)
	{
		AFBGhost* Ghost = *It;
		if (Ghost && Ghost->GetState() == EFBGhostState::Manifested)
		{
			const FVector Aim = Ghost->GetActorLocation() + FVector(0.f, 0.f, 120.f);
			const float D = FVector::Dist(MyLoc, Aim);
			if (D < BestDist && HasLineOfSight(Ghost, Aim))
			{
				Best = Ghost;
				BestDist = D;
			}
		}
	}

	for (TActorIterator<AFBCreature> It(GetWorld()); It; ++It)
	{
		AFBCreature* Creature = *It;
		if (!Creature || Creature->IsDead())
		{
			continue;
		}
		const float D = FVector::Dist(MyLoc, Creature->GetActorLocation());
		const bool bThreat = Creature->GetState() == EFBCreatureState::Chase || D < 800.f;
		if (bThreat && D < BestDist && HasLineOfSight(Creature, Creature->GetActorLocation()))
		{
			Best = Creature;
			BestDist = D;
		}
	}
	return Best;
}

void AFBCompanion::ShootAt(AActor* Target)
{
	const float Now = GetWorld()->GetTimeSeconds();
	NextFireTime = Now + FireCooldown + FMath::FRandRange(0.f, 0.6f);

	// Face the target.
	FVector To = Target->GetActorLocation() - GetActorLocation();
	To.Z = 0.f;
	SetActorRotation(To.Rotation());

	MuzzleFlash->SetVisibility(true);
	FlashTimeLeft = 0.06f;
	UFBNoiseSubsystem::ReportNoise(this, GetActorLocation(), 2.f, this);

	if (FMath::FRand() <= HitChance)
	{
		if (IFBDamageable* Damageable = Cast<IFBDamageable>(Target))
		{
			FFBHitInfo Info;
			Info.Amount = ShotDamage;
			Info.Kind = AmmoKind;
			Info.Causer = this;
			Info.Location = Target->GetActorLocation();
			Damageable->ApplyHit(Info);
		}
	}

	Bark(CombatLines, TEXT("Combat"), 12.f, 0.4f);
}

// ------------------------------------------------------------------ World awareness

void AFBCompanion::CheckWorldForBarks(APawn* Player)
{
	const FVector MyLoc = GetActorLocation();

	for (TActorIterator<AFBGhost> It(GetWorld()); It; ++It)
	{
		if (*It && It->GetState() == EFBGhostState::Manifested && FVector::Dist(MyLoc, It->GetActorLocation()) < 1500.f)
		{
			Bark(GhostLines, TEXT("Ghost"), 20.f);
			break;
		}
	}

	for (TActorIterator<AFBCreature> It(GetWorld()); It; ++It)
	{
		AFBCreature* Creature = *It;
		if (!Creature)
		{
			continue;
		}
		if (Creature->IsDead())
		{
			if (!AnnouncedKills.Contains(TWeakObjectPtr<AActor>(Creature)) && FVector::Dist(MyLoc, Creature->GetActorLocation()) < 3000.f)
			{
				AnnouncedKills.Add(TWeakObjectPtr<AActor>(Creature));
				LastLineTime = -100.f;
				Bark(KillLines, TEXT("Kill"), 0.f);
			}
		}
		else if (Creature->GetState() == EFBCreatureState::Chase && FVector::Dist(MyLoc, Creature->GetActorLocation()) < 2000.f)
		{
			Bark(CreatureLines, TEXT("Creature"), 15.f);
		}
	}

	if (!bAnnouncedCaseSolved)
	{
		if (const UFBCaseJournalComponent* Journal = Player->FindComponentByClass<UFBCaseJournalComponent>())
		{
			if (Journal->HasClue(TEXT("Case_Solved")))
			{
				bAnnouncedCaseSolved = true;
				LastLineTime = -100.f;
				Bark(CaseSolvedLines, TEXT("CaseSolved"), 0.f);
			}
		}
	}
}

void AFBCompanion::OpenDoorsAhead()
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
