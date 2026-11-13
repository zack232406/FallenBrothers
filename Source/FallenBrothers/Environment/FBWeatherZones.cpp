// Fallen Brothers - Water, ice and wind areas.

#include "Environment/FBWeatherZones.h"

#include "Core/FBNoiseSubsystem.h"
#include "Physics/FBPhysicsLib.h"
#include "Physics/FBTraps.h"
#include "Player/FBBodycamComponent.h"
#include "UI/FBHUD.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FVector FeetOf(const ACharacter* C)
	{
		return C->GetActorLocation() - FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}

	/** Slows a walking character down to Multiplier x its max speed. */
	void LimitSpeed(UCharacterMovementComponent* Move, float Multiplier)
	{
		const float Max = Move->GetMaxSpeed() * Multiplier;
		const FVector V = Move->Velocity;
		const FVector2D H(V.X, V.Y);
		if (H.SizeSquared() > Max * Max)
		{
			const FVector2D N = H.GetSafeNormal() * Max;
			Move->Velocity = FVector(N.X, N.Y, V.Z);
		}
	}

	bool IsBig(const ACharacter* C)
	{
		return C->GetCapsuleComponent()->GetScaledCapsuleRadius() > 45.f; // creatures are bigger than a man
	}
}

// ====================================================================== Water

AFBWater::AFBWater()
{
	Area->ShapeColor = FColor(40, 120, 255);

	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	Surface->SetupAttachment(Area);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	Surface->SetStaticMesh(Plane.Object);
}

float AFBWater::GetSurfaceZ() const
{
	return Area->GetComponentLocation().Z + GetBoxExtent().Z;
}

void AFBWater::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector E = Area->GetUnscaledBoxExtent();
	Surface->SetRelativeLocation(FVector(0.f, 0.f, E.Z));
	Surface->SetRelativeScale3D(FVector(E.X * 2.f / 100.f, E.Y * 2.f / 100.f, 1.f));
	FFBPhysicsLib::ApplyLook(Surface, WaterMaterial, FLinearColor(0.02f, 0.05f, 0.06f));
}

namespace
{
	// Small wind waves on a lake: three crossing swells, a few centimetres high (surface height offset, cm).
	float LakeWaveAt(const FVector2D& P, float T)
	{
		return 2.6f * FMath::Sin(P.X * 0.0021f + P.Y * 0.0009f + T * 1.15f)
			+ 1.6f * FMath::Sin(P.X * -0.0013f + P.Y * 0.0031f + T * 1.6f)
			+ 0.9f * FMath::Sin(P.X * 0.0047f - P.Y * 0.0038f + T * 2.4f);
	}

	// People in deep water (swimming / treading water). Remembered so they walk again when they get out.
	TSet<TWeakObjectPtr<ACharacter>> GSwimmers;
}

void AFBWater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float SurfaceZ = GetSurfaceZ();
	const float Now = GetWorld()->GetTimeSeconds();

	// Floating, sinking, drifting, rocking on the waves. Buoyancy is worked out at four points under each
	// body, so a plank or a boat tilts and rolls with the swell instead of bobbing like a cork.
	TArray<UPrimitiveComponent*> Bodies;
	GetBodiesInside(Bodies);
	for (UPrimitiveComponent* C : Bodies)
	{
		const FBoxSphereBounds& B = C->Bounds;
		const FVector E = B.BoxExtent;
		const float Bottom = B.Origin.Z - E.Z;
		const float VolumeM3 = 8.f * E.X * E.Y * E.Z * 1.0e-6f * 0.6f; // bounds overestimate the real shape
		const float Mass = C->GetMass();

		float SubmergedSum = 0.f;
		for (const FVector2D& K : { FVector2D(0.6f, 0.6f), FVector2D(-0.6f, 0.6f), FVector2D(0.6f, -0.6f), FVector2D(-0.6f, -0.6f) })
		{
			const FVector P(B.Origin.X + K.X * E.X, B.Origin.Y + K.Y * E.Y, Bottom);
			const float Water = SurfaceZ + LakeWaveAt(FVector2D(P.X, P.Y), Now);
			const float Sub = FMath::Clamp((Water - P.Z) / FMath::Max(1.f, E.Z * 2.f), 0.f, 1.f);
			if (Sub > 0.f)
			{
				// Archimedes: 1000 kg/m³ of displaced water, a quarter of the body per point.
				C->AddForceAtLocation(FVector(0.f, 0.f, 1000.f * VolumeM3 * 0.25f * Sub * 980.f), FVector(P.X, P.Y, FMath::Min(Water, B.Origin.Z)));
				SubmergedSum += Sub * 0.25f;
			}
		}
		if (SubmergedSum <= 0.f)
		{
			continue;
		}

		// Water drag (grows with speed, like real water) + the current + a slow drift with the wind.
		if (!FFBPhysicsLib::IsHeldByPlayer(this, C))
		{
			const FVector Drift = Flow + FVector(8.f, -4.f, 0.f);
			const FVector Rel = Drift - C->GetPhysicsLinearVelocity();
			const float Quadratic = 1.f + Rel.Size() / 300.f;
			C->AddForce(Rel * Mass * Drag * SubmergedSum * Quadratic);
			C->SetPhysicsAngularVelocityInDegrees(C->GetPhysicsAngularVelocityInDegrees() * FMath::Max(0.f, 1.f - Drag * 0.6f * DeltaSeconds * SubmergedSum));
		}

		// Something hitting the water fast makes a splash (heard by the creatures).
		if (FMath::Abs(C->GetPhysicsLinearVelocity().Z) > 350.f && FMath::Abs(Bottom - SurfaceZ) < E.Z * 2.f + 30.f && Now >= NextSplash)
		{
			NextSplash = Now + 0.3f;
			UFBNoiseSubsystem::ReportNoise(this, FVector(B.Origin.X, B.Origin.Y, SurfaceZ), FMath::Clamp(Mass / 40.f, 0.3f, 1.f), nullptr);
		}
	}

	// Wading, then swimming once the water is chest deep: slow, heavy, the head stays at the surface and
	// bobs with the waves. Back on your feet when the bottom comes up again.
	TArray<ACharacter*> Characters;
	GetCharactersInside(Characters);
	TSet<TWeakObjectPtr<ACharacter>> StillIn;
	for (ACharacter* C : Characters)
	{
		UCharacterMovementComponent* Move = C->GetCharacterMovement();
		if (!Move)
		{
			continue;
		}
		const FVector Feet = FeetOf(C);
		const float Water = SurfaceZ + LakeWaveAt(FVector2D(Feet.X, Feet.Y), Now);
		const float Depth = Water - Feet.Z;
		const bool bWasSwimming = GSwimmers.Contains(C);

		// Is the bottom still in reach under the feet?
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(FBWaterBottom), false, C);
		const bool bBottom = GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, 20.f), Feet - FVector(0.f, 0.f, 40.f), ECC_WorldStatic, Q);
		const float BottomDepth = bBottom ? Water - Hit.ImpactPoint.Z : 1000.f;

		if ((!bWasSwimming && Depth > 160.f) || (bWasSwimming && BottomDepth > 145.f))
		{
			StillIn.Add(C);
			if (Move->MovementMode != MOVE_Flying)
			{
				Move->SetMovementMode(MOVE_Flying);
			}
			Move->MaxFlySpeed = 160.f;
			Move->BrakingDecelerationFlying = 400.f;
			// Keep the head just above the water: feet about 1.4 m under the surface.
			const float TargetFeet = Water - 140.f;
			Move->Velocity.Z = FMath::Clamp((TargetFeet - Feet.Z) * 3.f, -120.f, 120.f);
			if (!Flow.IsNearlyZero())
			{
				Move->Velocity += Flow * 0.5f * DeltaSeconds;
			}
			if (Move->Velocity.Size2D() > 60.f && Now >= NextSplash)
			{
				NextSplash = Now + 0.6f;
				UFBNoiseSubsystem::ReportNoise(this, Feet + FVector(0.f, 0.f, 140.f), 0.5f, C);
			}
			continue;
		}

		if (bWasSwimming && Move->MovementMode == MOVE_Flying)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
		if (Depth <= 5.f)
		{
			continue;
		}
		const float F = FMath::Clamp(Depth / 100.f, 0.f, 1.f); // 1 = waist deep
		LimitSpeed(Move, FMath::Lerp(1.f, WadeSpeedMultiplier, F));
		if (!Flow.IsNearlyZero())
		{
			Move->AddForce(Flow * Move->Mass * 0.6f * F);
		}

		if (Move->Velocity.Size2D() > 100.f && Now >= NextSplash)
		{
			NextSplash = Now + 0.45f;
			UFBNoiseSubsystem::ReportNoise(this, Feet, 0.3f + 0.4f * F, C);
		}
	}

	// Out of the water while still swimming (left the lake): walk again.
	for (const TWeakObjectPtr<ACharacter>& W : GSwimmers)
	{
		if (ACharacter* C = W.Get())
		{
			if (!StillIn.Contains(W) && C->GetCharacterMovement() && C->GetCharacterMovement()->MovementMode == MOVE_Flying && !Characters.Contains(C))
			{
				C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
	}
	GSwimmers = MoveTemp(StillIn);
}

// ====================================================================== Ice

AFBIce::AFBIce()
{
	Area->ShapeColor = FColor(170, 230, 255);
	Area->SetBoxExtent(FVector(800.f, 800.f, 50.f));
}

void AFBIce::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AFBIce::Build()
{

	TArray<UActorComponent*> Old;
	GetComponents(Old);
	for (UActorComponent* C : Old)
	{
		if (C && C->CreationMethod == EComponentCreationMethod::UserConstructionScript)
		{
			C->DestroyComponent();
		}
	}
	Tiles.Reset();

	UStaticMesh* Cube = FFBPhysicsLib::BasicShape(TEXT("Cube"));
	const FVector E = Area->GetUnscaledBoxExtent();
	const FVector Scale = Area->GetComponentScale();
	const float Z = E.Z - IceThickness * 0.5f / FMath::Max(0.01f, Scale.Z);

	// One sheet - or a grid of tiles that can break off separately.
	const float Tile = bThinIce ? FMath::Max(60.f, TileSize) : FMath::Max(E.X, E.Y) * 2.f + 1.f;
	const int32 NX = FMath::Max(1, FMath::CeilToInt(E.X * 2.f * Scale.X / Tile));
	const int32 NY = FMath::Max(1, FMath::CeilToInt(E.Y * 2.f * Scale.Y / Tile));
	const float SX = E.X * 2.f / NX;
	const float SY = E.Y * 2.f / NY;

	for (int32 X = 0; X < NX; ++X)
	{
		for (int32 Y = 0; Y < NY; ++Y)
		{
			UStaticMeshComponent* T = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transactional);
			T->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			T->SetStaticMesh(Cube);
			T->SetMobility(EComponentMobility::Movable);
			T->SetupAttachment(Area);
			T->SetRelativeLocation(FVector(-E.X + SX * (X + 0.5f), -E.Y + SY * (Y + 0.5f), Z));
			T->SetRelativeScale3D(FVector(SX / 100.f, SY / 100.f, IceThickness / 100.f / FMath::Max(0.01f, Scale.Z)));
			T->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			T->SetCollisionObjectType(ECC_WorldDynamic); // shootable
			T->RegisterComponent();
			FFBPhysicsLib::ApplyLook(T, IceMaterial, FLinearColor(0.55f, 0.68f, 0.75f));
			Tiles.Add(T);
		}
	}

	// Props slide on it.
	SlipperyMaterial = NewObject<UPhysicalMaterial>(this);
	SlipperyMaterial->Friction = 0.03f;
	SlipperyMaterial->StaticFriction = 0.03f;
	SlipperyMaterial->bOverrideFrictionCombineMode = true;
	SlipperyMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
	for (UStaticMeshComponent* T : Tiles)
	{
		T->SetPhysMaterialOverride(SlipperyMaterial);
	}
}

void AFBIce::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void AFBIce::Restore(ACharacter* C, const FSavedMovement& Saved) const
{
	if (UCharacterMovementComponent* Move = C ? C->GetCharacterMovement() : nullptr)
	{
		Move->GroundFriction = Saved.GroundFriction;
		Move->BrakingDecelerationWalking = Saved.BrakingDeceleration;
		Move->MaxAcceleration = Saved.MaxAcceleration;
	}
}

void AFBIce::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TPair<TWeakObjectPtr<ACharacter>, FSavedMovement>& P : OnIce)
	{
		Restore(P.Key.Get(), P.Value);
	}
	OnIce.Reset();
	Super::EndPlay(EndPlayReason);
}

void AFBIce::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* C = *It;
		UCharacterMovementComponent* Move = C->GetCharacterMovement();
		if (!Move)
		{
			continue;
		}

		// Standing on one of our ice tiles?
		const UPrimitiveComponent* Floor = Move->CurrentFloor.HitResult.GetComponent();
		const bool bOnIce = Move->IsMovingOnGround() && Floor && Floor->GetOwner() == this;

		FSavedMovement* Saved = OnIce.Find(C);
		if (bOnIce && !Saved)
		{
			FSavedMovement S;
			S.GroundFriction = Move->GroundFriction;
			S.BrakingDeceleration = Move->BrakingDecelerationWalking;
			S.MaxAcceleration = Move->MaxAcceleration;
			OnIce.Add(C, S);
			Move->GroundFriction = Grip;
			Move->BrakingDecelerationWalking = 60.f;
			Move->MaxAcceleration = S.MaxAcceleration * 0.3f;
		}
		else if (!bOnIce && Saved)
		{
			Restore(C, *Saved);
			OnIce.Remove(C);
			Strain.Remove(C);
		}

		if (!bOnIce || !bThinIce)
		{
			continue;
		}

		// Thin ice: standing still is the worst thing you can do.
		const bool bMoving = Move->Velocity.Size2D() > 60.f;
		const float Weight = IsBig(C) ? 3.f : 1.f;
		float& S = Strain.FindOrAdd(C);
		S += DeltaSeconds * Weight * (bMoving ? 0.35f : 1.f) / FMath::Max(0.2f, BreakTime);

		if (S > 0.4f && Now >= NextCrack)
		{
			NextCrack = Now + FMath::FRandRange(0.4f, 0.9f);
			UFBNoiseSubsystem::ReportNoise(this, FeetOf(C), 0.5f, this);
			if (C == Player)
			{
				if (UFBBodycamComponent* Cam = C->FindComponentByClass<UFBBodycamComponent>())
				{
					Cam->AddKick(FRotator(0.5f, 0.f, FMath::FRandRange(-1.f, 1.f)));
				}
				AFBHUD::ShowNotificationFor(C, NSLOCTEXT("FB", "IceCrack", "The ice cracks under your feet..."), 1.5f);
			}
		}
		if (S >= 1.f)
		{
			BreakAt(FeetOf(C), Weight > 1.f ? 330.f : 220.f);
			S = 0.f;
		}
	}
}

void AFBIce::ApplyHit(const FFBHitInfo& Hit)
{
	if (bThinIce && Hit.Amount >= 15.f)
	{
		BreakAt(Hit.Location, 130.f);
	}
}

void AFBIce::BreakAt(const FVector& Where, float Radius)
{
	if (!bThinIce)
	{
		return;
	}
	bool bAny = false;
	for (int32 i = Tiles.Num() - 1; i >= 0; --i)
	{
		UStaticMeshComponent* T = Tiles[i];
		if (!T || FVector::Dist2D(T->GetComponentLocation(), Where) > Radius)
		{
			continue;
		}
		T->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		T->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // you go through
		T->SetSimulatePhysics(true);
		T->SetMassOverrideInKg(NAME_None, 40.f, true);
		T->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 40.f);
		T->SetPhysicsLinearVelocity(FVector(0.f, 0.f, -60.f));
		Tiles.RemoveAtSwap(i);
		bAny = true;
	}
	if (bAny)
	{
		UFBNoiseSubsystem::ReportNoise(this, Where, 1.3f, this);
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			if (FVector::Dist(Player->GetActorLocation(), Where) < 1500.f)
			{
				AFBHUD::ShowNotificationFor(Player, NSLOCTEXT("FB", "IceBreak", "The ice gives way!"), 2.f);
			}
		}
	}
}

// ====================================================================== Wind

AFBWind::AFBWind()
{
	Area->ShapeColor = FColor(200, 200, 200);
	Area->SetBoxExtent(FVector(2000.f, 2000.f, 800.f));
}

void AFBWind::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	// Gusts: smooth random swells, and the direction wanders a little.
	const float Noise = FMath::PerlinNoise1D(Time * GustFrequency);
	const float Gust = FMath::Max(0.1f, 1.f + Gustiness * Noise * 1.4f);
	const float Wander = FMath::PerlinNoise1D(Time * 0.07f + 37.f) * 20.f * Gustiness;
	const FVector Dir = FRotator(0.f, GetActorRotation().Yaw + Wander, 0.f).Vector();
	CurrentWind = Dir * WindSpeed * Gust * 100.f; // m/s -> cm/s

	// Real air drag: F = 1/2 * air density * area * (relative speed)²
	TArray<UPrimitiveComponent*> Bodies;
	GetBodiesInside(Bodies);
	for (UPrimitiveComponent* C : Bodies)
	{
		if (FFBPhysicsLib::IsHeldByPlayer(this, C))
		{
			continue;
		}
		const FVector E = C->Bounds.BoxExtent;
		const float AreaM2 = (4.f * (E.Y * E.Z + E.X * E.Z + E.X * E.Y) / 3.f) * 1.0e-4f;
		const FVector Rel = (CurrentWind - C->GetPhysicsLinearVelocity()) / 100.f; // m/s
		const float ForceN = 0.5f * 1.2f * AreaM2 * Rel.SizeSquared();
		C->AddForce(Rel.GetSafeNormal() * ForceN * 100.f); // N -> kg·cm/s²
	}

	// Gusts shove people around.
	const float Shove = FMath::Max(0.f, Gust - 0.9f) * (WindSpeed / 20.f);
	if (Shove > 0.f)
	{
		TArray<ACharacter*> Characters;
		GetCharactersInside(Characters);
		for (ACharacter* C : Characters)
		{
			if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
			{
				Move->AddForce(Dir * CharacterPush * Shove * Move->Mass);
			}
		}
	}

	// Hanging lamps sway.
	for (TActorIterator<AFBSwingingLamp> It(GetWorld()); It; ++It)
	{
		if (IsInside(It->GetActorLocation()))
		{
			It->Push(Dir * WindSpeed * Gust * 2.f * DeltaSeconds);
		}
	}
}
