// Fallen Brothers - Ghost (vengeful spirit).

#include "Paranormal/FBGhost.h"

#include "Combat/FBHealthComponent.h"
#include "Equipment/FBEquipmentComponent.h"
#include "Equipment/FBFlashlight.h"
#include "Paranormal/FBParanormalSourceComponent.h"
#include "Paranormal/FBParanormalSubsystem.h"
#include "Physics/FBPhysicsProp.h"
#include "Player/FBBodycamComponent.h"
#include "UI/FBHUD.h"
#include "Save/FBSaveSubsystem.h"
#include "Interaction/FBDoor.h"

#include "NavigationPath.h"
#include "NavigationSystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBGhost::AFBGhost()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Placeholder: tall pale figure (origin on the floor).
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(0.45f, 0.35f, 1.6f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, 85.f));
	Body->SetCastShadow(false);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetStaticMesh(Sphere.Object);
	Head->SetRelativeScale3D(FVector(0.24f));
	Head->SetRelativeLocation(FVector(0.f, 0.f, 178.f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetCastShadow(false);

	if (BasicMaterial.Succeeded())
	{
		Body->SetMaterial(0, BasicMaterial.Object);
		Head->SetMaterial(0, BasicMaterial.Object);
	}

	// Incorporeal: nothing collides, but it "feels" physics objects passing through it.
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Overlap);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetGenerateOverlapEvents(true);

	EMFSource = CreateDefaultSubobject<UFBParanormalSourceComponent>(TEXT("EMFSource"));
	EMFSource->SetupAttachment(Body);
	EMFSource->EMFStrength = 5.f;
	EMFSource->Radius = 600.f;
}

void AFBGhost::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

	if (UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.65f, 0.75f));
	}
	if (UMaterialInstanceDynamic* MID = Head->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.7f, 0.75f, 0.8f));
	}

	Body->OnComponentBeginOverlap.AddDynamic(this, &AFBGhost::OnBodyOverlap);

	SetState(EFBGhostState::Dormant);
	ScheduleNextManifest(5.f);
	NextThrowTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(ThrowIntervalMin, ThrowIntervalMax);
}

// ------------------------------------------------------------------ State helpers

void AFBGhost::SetState(EFBGhostState NewState)
{
	State = NewState;
	StateStartTime = GetWorld()->GetTimeSeconds();
	StareTime = 0.f;

	const bool bVisible = (NewState == EFBGhostState::Manifested || NewState == EFBGhostState::Banished);
	SetVisible(bVisible);
	Body->SetCollisionEnabled(NewState == EFBGhostState::Manifested ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	// EMF: strong when manifested, faint while dormant (you can still find its haunt), none when gone.
	switch (NewState)
	{
	case EFBGhostState::Manifested: EMFSource->SetSourceActive(true);  EMFSource->EMFStrength = 5.f; break;
	case EFBGhostState::Dormant:    EMFSource->SetSourceActive(true);  EMFSource->EMFStrength = 2.5f; break;
	default:                        EMFSource->SetSourceActive(false); break;
	}
}

void AFBGhost::SetVisible(bool bVisible)
{
	Body->SetVisibility(bVisible);
	Head->SetVisibility(bVisible);
}

void AFBGhost::ScheduleNextManifest(float MinDelay)
{
	const float Interval = FMath::FRandRange(ManifestIntervalMin, ManifestIntervalMax) * IntervalMultiplier;
	NextManifestTime = GetWorld()->GetTimeSeconds() + FMath::Max(MinDelay, Interval);
}

void AFBGhost::Notify(const FText& Message) const
{
	AFBHUD::ShowNotificationFor(UGameplayStatics::GetPlayerPawn(this, 0), Message, 2.5f);
}

void AFBGhost::FlickerPlayerLights(APawn* Player, float Amount, float Duration) const
{
	if (!Player)
	{
		return;
	}
	if (const UFBEquipmentComponent* Equipment = Player->FindComponentByClass<UFBEquipmentComponent>())
	{
		if (AFBFlashlight* Light = Equipment->GetFlashlight())
		{
			Light->AddInterference(Amount, Duration);
		}
	}
}

// ------------------------------------------------------------------ Tick

void AFBGhost::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	switch (State)
	{
	case EFBGhostState::Dormant:
		if (Player && Now >= NextManifestTime && FVector::Dist(Player->GetActorLocation(), HomeLocation) <= ActivationRadius)
		{
			Manifest();
		}
		TryPoltergeist(Player);
		break;

	case EFBGhostState::Manifested:
		TickManifested(DeltaSeconds, Player);
		TryPoltergeist(Player);
		break;

	case EFBGhostState::Dispersed:
		if (Now - StateStartTime >= DisperseTime)
		{
			SetActorLocation(HomeLocation);
			SetState(EFBGhostState::Dormant);
			ScheduleNextManifest(3.f);
		}
		break;

	case EFBGhostState::Banished:
	{
		// Rise and shrink away, then disappear for good.
		const float T = FMath::Clamp((Now - BanishTime) / 2.5f, 0.f, 1.f);
		AddActorWorldOffset(FVector(0.f, 0.f, 60.f * DeltaSeconds));
		SetActorScale3D(FVector(FMath::Max(0.01f, 1.f - T)));
		SetVisible(FMath::FRand() > T * 0.8f); // Flicker out
		if (T >= 1.f)
		{
			Destroy();
		}
		break;
	}
	}
}

void AFBGhost::Manifest()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	// Appear somewhere around the player, on their floor level.
	const float Angle = FMath::FRandRange(0.f, 360.f);
	const float Dist = FMath::FRandRange(ManifestDistanceMin, ManifestDistanceMax);
	const FVector Offset = FRotator(0.f, Angle, 0.f).Vector() * Dist;

	FVector Feet = Player->GetActorLocation();
	if (const ACharacter* Character = Cast<ACharacter>(Player))
	{
		Feet.Z -= Character->GetSimpleCollisionHalfHeight();
	}
	SetActorLocation(Feet + Offset);

	SetState(EFBGhostState::Manifested);
	FlickerPlayerLights(Player, 1.f, 1.2f);

	if (bFirstManifest)
	{
		bFirstManifest = false;
		Notify(NSLOCTEXT("FB", "GhostFirst", "Your breath fogs in the air. Something is here."));
	}
}

void AFBGhost::Vanish(float MinDelay)
{
	SetActorLocation(HomeLocation); // Back to its haunt (faint EMF stays there).
	SetState(EFBGhostState::Dormant);
	ScheduleNextManifest(MinDelay);
}

void AFBGhost::TickManifested(float DeltaSeconds, APawn* Player)
{
	const float Now = GetWorld()->GetTimeSeconds();

	if (!Player || Now - StateStartTime > MaxManifestTime)
	{
		Vanish(4.f);
		return;
	}

	// Creepy flicker: drops out for a frame now and then.
	SetVisible(FMath::FRand() > 0.06f);

	const FVector MyLoc = GetActorLocation();
	FVector ToPlayer = Player->GetActorLocation() - MyLoc;
	ToPlayer.Z = 0.f;
	const float Dist = ToPlayer.Size();

	// Face the player.
	if (Dist > 1.f)
	{
		SetActorRotation(FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f));
	}

	// Reached the player: attack.
	if (Dist <= AttackRange)
	{
		AttackPlayer(Player);
		return;
	}

	// Being stared at? Fade away.
	if (const AController* Controller = Player->GetController())
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		const FVector ToMe = (Head->GetComponentLocation() - ViewLoc).GetSafeNormal();
		const bool bLookedAt = FVector::DotProduct(ViewRot.Vector(), ToMe) > 0.985f; // ~10 degrees

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GhostStare), false, Player);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, Head->GetComponentLocation(), ECC_Visibility, Params);

		StareTime = (bLookedAt && !bBlocked) ? StareTime + DeltaSeconds : FMath::Max(0.f, StareTime - DeltaSeconds);
		if (StareTime >= StareVanishTime)
		{
			Vanish(6.f);
			return;
		}
	}

	// Where are its feet going? Straight at you, or along the corridors (navmesh).
	FVector Feet = Player->GetActorLocation();
	if (const ACharacter* Character = Cast<ACharacter>(Player))
	{
		Feet.Z -= Character->GetSimpleCollisionHalfHeight();
	}
	const FVector MoveTarget = GetMoveTarget(MyLoc, Feet);

	FVector MoveDir = MoveTarget - MyLoc;
	MoveDir.Z = 0.f;
	MoveDir = MoveDir.GetSafeNormal();
	const FVector NewLoc = MyLoc + MoveDir * GlideSpeed * SpeedMultiplier * DeltaSeconds;

	// Never across salt.
	const UFBParanormalSubsystem* Subsystem = GetWorld()->GetSubsystem<UFBParanormalSubsystem>();
	if (Subsystem && Subsystem->IsBlockedBySalt(MyLoc, NewLoc))
	{
		if (Now - LastSaltMessage > 6.f)
		{
			LastSaltMessage = Now;
			Notify(NSLOCTEXT("FB", "GhostSalt", "It stops at the salt line..."));
		}
		return;
	}

	if (bWalksAroundWalls)
	{
		OpenDoorsAhead(MoveDir);
	}

	// Follow the floor height smoothly (stairs etc.).
	SetActorLocation(FVector(NewLoc.X, NewLoc.Y, FMath::FInterpTo(MyLoc.Z, MoveTarget.Z, DeltaSeconds, 3.f)));
}

FVector AFBGhost::GetMoveTarget(const FVector& MyLoc, const FVector& PlayerFeet)
{
	if (!bWalksAroundWalls)
	{
		return PlayerFeet;
	}

	// Re-plan the route a few times a second.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextPathTime)
	{
		NextPathTime = Now + 0.4f;
		PathPoints.Reset();
		PathIndex = 1;
		if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, MyLoc, PlayerFeet))
		{
			if (Path->IsValid())
			{
				PathPoints = Path->PathPoints;
			}
		}
	}

	// Skip waypoints we've reached.
	while (PathPoints.IsValidIndex(PathIndex) && FVector::Dist2D(MyLoc, PathPoints[PathIndex]) < 30.f)
	{
		++PathIndex;
	}

	// No route (no navmesh?) -> fall back to drifting straight at you.
	return PathPoints.IsValidIndex(PathIndex) ? PathPoints[PathIndex] : PlayerFeet;
}

void AFBGhost::OpenDoorsAhead(const FVector& MoveDir)
{
	const FVector MyLoc = GetActorLocation();
	for (TActorIterator<AFBDoor> It(GetWorld()); It; ++It)
	{
		AFBDoor* Door = *It;
		if (!Door || Door->IsOpen())
		{
			continue;
		}
		const FVector ToDoor = Door->GetActorLocation() - MyLoc;
		if (ToDoor.Size2D() < 130.f && FVector::DotProduct(MoveDir, ToDoor.GetSafeNormal2D()) > 0.2f)
		{
			// Ghosts don't care about locks. The door just... swings open.
			Door->SetOpen(true, this);
		}
	}
}

void AFBGhost::AttackPlayer(APawn* Player)
{
	if (UFBHealthComponent* Health = Player->FindComponentByClass<UFBHealthComponent>())
	{
		Health->ApplyDamage(AttackDamage, this);
	}

	if (ACharacter* Character = Cast<ACharacter>(Player))
	{
		FVector Push = Player->GetActorLocation() - GetActorLocation();
		Push.Z = 0.f;
		Character->LaunchCharacter(Push.GetSafeNormal() * AttackKnockback + FVector(0.f, 0.f, 200.f), true, true);
	}

	if (UFBBodycamComponent* Bodycam = Player->FindComponentByClass<UFBBodycamComponent>())
	{
		Bodycam->AddKick(FRotator(8.f, FMath::FRandRange(-6.f, 6.f), FMath::FRandRange(-10.f, 10.f)));
	}

	FlickerPlayerLights(Player, 1.f, 2.f);
	Vanish(5.f);
}

// ------------------------------------------------------------------ Poltergeist

void AFBGhost::TryPoltergeist(APawn* Player)
{
	if (!bThrowsObjects || !Player)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextThrowTime)
	{
		return;
	}
	NextThrowTime = Now + FMath::FRandRange(ThrowIntervalMin, ThrowIntervalMax) * IntervalMultiplier;

	// Only when the player is around its haunt.
	const FVector Center = (State == EFBGhostState::Manifested) ? GetActorLocation() : HomeLocation;
	if (FVector::Dist(Player->GetActorLocation(), Center) > ActivationRadius)
	{
		return;
	}

	// Pick a random light prop near the ghost.
	TArray<AFBPhysicsProp*> Candidates;
	for (TActorIterator<AFBPhysicsProp> It(GetWorld()); It; ++It)
	{
		AFBPhysicsProp* Prop = *It;
		UPrimitiveComponent* Mesh = Prop ? Prop->GetMesh() : nullptr;
		if (Mesh && Mesh->IsSimulatingPhysics() && Mesh->GetMass() < 30.f
			&& FVector::Dist(Prop->GetActorLocation(), Center) <= ThrowGrabRange
			&& FVector::Dist(Prop->GetActorLocation(), Player->GetActorLocation()) > 150.f)
		{
			Candidates.Add(Prop);
		}
	}
	if (Candidates.Num() == 0)
	{
		return;
	}

	AFBPhysicsProp* Chosen = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	UPrimitiveComponent* Mesh = Chosen->GetMesh();

	// Lift slightly, then hurl toward the player's chest.
	const FVector Target = Player->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	const FVector Dir = (Target - Mesh->GetComponentLocation()).GetSafeNormal();
	Mesh->SetPhysicsLinearVelocity(Dir * ThrowSpeed + FVector(0.f, 0.f, 150.f));
	Mesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 360.f);

	FlickerPlayerLights(Player, 0.7f, 0.8f);
}

// ------------------------------------------------------------------ Damage

void AFBGhost::OnBodyOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                             int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (State != EFBGhostState::Manifested || !OtherActor || !OtherComp || !OtherComp->IsSimulatingPhysics())
	{
		return;
	}

	const IFBImpactSource* Source = Cast<IFBImpactSource>(OtherActor);
	if (!Source || OtherComp->GetPhysicsLinearVelocity().Size() < MinImpactSpeed)
	{
		return;
	}

	FFBHitInfo Hit;
	Hit.Kind = Source->GetImpactKind();
	Hit.Amount = Source->GetImpactMassKg();
	Hit.Causer = OtherActor;
	Hit.Location = OtherComp->GetComponentLocation();
	ApplyHit(Hit);
}

void AFBGhost::ApplyHit(const FFBHitInfo& Hit)
{
	if (State != EFBGhostState::Manifested)
	{
		return; // Can't hurt what isn't there.
	}

	if (Weaknesses.Contains(Hit.Kind))
	{
		SetState(EFBGhostState::Dispersed);
		Notify(NSLOCTEXT("FB", "GhostDispersed2", "The apparition shrieks and scatters!"));
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastNoEffectMessage > 3.f)
	{
		LastNoEffectMessage = Now;
		Notify(NSLOCTEXT("FB", "GhostNoEffect2", "It passed right through..."));
	}
}

void AFBGhost::Banish()
{
	if (State == EFBGhostState::Banished)
	{
		return;
	}
	// Appear one last time where the remains burn... and burn away.
	UFBSaveSubsystem::NotifyActorRemoved(this);
	BanishTime = GetWorld()->GetTimeSeconds();
	SetState(EFBGhostState::Banished);
}

void AFBGhost::Enrage()
{
	SpeedMultiplier = 1.6f;
	IntervalMultiplier = 0.45f;
	if (State == EFBGhostState::Dormant)
	{
		NextManifestTime = GetWorld()->GetTimeSeconds() + 1.5f;
	}
	Notify(NSLOCTEXT("FB", "GhostEnraged", "The temperature drops. It knows what you're doing."));
}
