// Fallen Brothers - Environmental physics traps.

#include "Physics/FBTraps.h"

#include "Companion/FBCompanion.h"
#include "Core/FBNoiseSubsystem.h"
#include "Physics/FBPhysicsLib.h"
#include "Player/FBBodycamComponent.h"
#include "UI/FBHUD.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor Wood(0.26f, 0.18f, 0.11f);
	const FLinearColor DarkWood(0.14f, 0.1f, 0.07f);
	const FLinearColor Rope(0.35f, 0.3f, 0.2f);
	const FLinearColor Stone(0.3f, 0.29f, 0.27f);
	const FLinearColor Metal(0.08f, 0.08f, 0.08f);

	/** Destroys the parts made by the previous rebuild. */
	void ClearBuiltParts(AActor* Actor)
	{
		TArray<UActorComponent*> Old;
		Actor->GetComponents(Old);
		for (UActorComponent* C : Old)
		{
			if (C && C->CreationMethod == EComponentCreationMethod::UserConstructionScript)
			{
				C->DestroyComponent();
			}
		}
	}

	/** A part made in OnConstruction (remade whenever you change values in Details). */
	UStaticMeshComponent* BuildPart(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm,
	                                const FRotator& Rot, UMaterialInterface* Mat, const FLinearColor& Color)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transactional);
		C->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		C->SetStaticMesh(Mesh);
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(Parent);
		C->SetRelativeLocationAndRotation(Center, Rot);
		C->SetRelativeScale3D(SizeCm / 100.f);
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		C->SetCollisionObjectType(ECC_WorldDynamic); // so shotgun pellets / knives find it
		C->RegisterComponent();
		FFBPhysicsLib::ApplyLook(C, Mat, Color);
		return C;
	}

	/** Turns a fixed part into a loose falling one that doesn't block pawns. */
	void MakeLoose(UStaticMeshComponent* C, float MassKg)
	{
		if (!C)
		{
			return;
		}
		C->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->SetSimulatePhysics(true);
		C->SetMassOverrideInKg(NAME_None, MassKg, true);
		C->WakeAllRigidBodies();
	}

	bool IsFriendly(const UObject* Context, const AActor* A)
	{
		return A == UGameplayStatics::GetPlayerPawn(Context, 0) || A->IsA<AFBCompanion>();
	}

	FVector FeetOf(const ACharacter* C)
	{
		return C->GetActorLocation() - FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}

	void KickCamera(const UObject* Context, float Strength)
	{
		if (const APawn* P = UGameplayStatics::GetPlayerPawn(Context, 0))
		{
			if (UFBBodycamComponent* Cam = P->FindComponentByClass<UFBBodycamComponent>())
			{
				Cam->AddKick(FRotator(Strength, FMath::FRandRange(-Strength, Strength), FMath::FRandRange(-Strength, Strength) * 1.5f));
			}
		}
	}
}

// ====================================================================== Swinging lamp

AFBSwingingLamp::AFBSwingingLamp()
{
	PrimaryActorTick.bCanEverTick = true;

	Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"));
	SetRootComponent(Anchor);

	Cable = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cable"));
	Cable->SetupAttachment(Anchor);
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cable->SetCastShadow(true);
	Cable->SetUsingAbsoluteLocation(true);
	Cable->SetUsingAbsoluteRotation(true);
	Cable->SetUsingAbsoluteScale(true);

	Shade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shade"));
	Shade->SetupAttachment(Anchor);
	Shade->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);

	Joint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Joint"));
	Joint->SetupAttachment(Anchor);
	Joint->SetLinearXLimit(LCM_Locked, 0.f);
	Joint->SetLinearYLimit(LCM_Locked, 0.f);
	Joint->SetLinearZLimit(LCM_Locked, 0.f);
	Joint->SetAngularSwing1Limit(ACM_Limited, 75.f);
	Joint->SetAngularSwing2Limit(ACM_Limited, 75.f);
	Joint->SetAngularTwistLimit(ACM_Limited, 40.f);

	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Shade);
	Light->SetRelativeLocation(FVector(0.f, 0.f, -10.f));
	Light->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Light->SetOuterConeAngle(60.f);
	Light->SetInnerConeAngle(25.f);
	Light->SetAttenuationRadius(1200.f);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Shade);
	Glow->SetAttenuationRadius(250.f);
	Glow->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	LampMesh = ConeMesh.Object;
	Cable->SetStaticMesh(CylMesh.Object);
}

void AFBSwingingLamp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Shade->SetStaticMesh(LampMesh);
	if (LampMesh && LampMesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes")))
	{
		Shade->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.3f)); // placeholder cone shade
	}
	Shade->SetRelativeLocation(FVector(0.f, 0.f, -CableLength - 15.f));
	FFBPhysicsLib::ApplyLook(Shade, LampMaterial, Metal);
	FFBPhysicsLib::ApplyLook(Cable, nullptr, Metal);

	Light->SetIntensity(bOn ? Intensity : 0.f);
	Light->SetLightColor(LightColor);
	Glow->SetIntensity(bOn ? Intensity * 0.15f : 0.f);
	Glow->SetLightColor(LightColor);
	UpdateCable();
}

void AFBSwingingLamp::BeginPlay()
{
	Super::BeginPlay();

	Shade->SetSimulatePhysics(true);
	Shade->SetMassOverrideInKg(NAME_None, 4.f, true);
	Shade->SetLinearDamping(0.05f);
	Shade->SetAngularDamping(0.4f);
	Joint->SetConstrainedComponents(nullptr, NAME_None, Shade, NAME_None); // hangs from the ceiling point
	NextIdlePush = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 3.f);
}

void AFBSwingingLamp::Push(const FVector& Velocity)
{
	Shade->AddImpulse(Velocity, NAME_None, true);
	if (Velocity.Size() > 80.f)
	{
		FlickerUntil = GetWorld()->GetTimeSeconds() + 0.6f; // a hard knock shakes the wiring
	}
}

void AFBSwingingLamp::UpdateCable()
{
	const FVector Top = Anchor->GetComponentLocation();
	const FVector Bottom = Shade->GetComponentLocation() + Shade->GetUpVector() * 12.f;
	const FVector Delta = Top - Bottom;
	const float Len = FMath::Max(1.f, Delta.Size());
	Cable->SetWorldLocationAndRotation((Top + Bottom) * 0.5f, FRotationMatrix::MakeFromZ(Delta / Len).Rotator());
	Cable->SetWorldScale3D(FVector(0.012f, 0.012f, Len / 100.f));
}

void AFBSwingingLamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCable();

	const float Now = GetWorld()->GetTimeSeconds();
	if (IdleSway > 0.f && Now >= NextIdlePush)
	{
		NextIdlePush = Now + FMath::FRandRange(2.f, 5.f);
		Shade->AddImpulse(FMath::VRand().GetSafeNormal2D() * IdleSway, NAME_None, true);
	}

	if (!bOn)
	{
		return;
	}

	// Bad wiring: the harder it swings, the more it flickers.
	float Level = 1.f;
	if (bFlickers)
	{
		const float Speed = Shade->GetPhysicsLinearVelocity().Size();
		if (Now < FlickerUntil || (Speed > 120.f && FMath::FRand() < DeltaSeconds * 3.f) || FMath::FRand() < DeltaSeconds * 0.05f)
		{
			if (Now >= NextFlicker)
			{
				NextFlicker = Now + FMath::FRandRange(0.03f, 0.12f);
				FlickerUntil = FMath::Max(FlickerUntil, Now + FMath::FRandRange(0.1f, 0.4f));
			}
			Level = FMath::FRand() < 0.5f ? FMath::FRandRange(0.f, 0.25f) : 1.f;
		}
	}
	Light->SetIntensity(Intensity * Level);
	Glow->SetIntensity(Intensity * 0.15f * Level);
}

// ====================================================================== Collapsing floor

AFBCollapsingFloor::AFBCollapsingFloor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AFBCollapsingFloor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AFBCollapsingFloor::Build()
{
	ClearBuiltParts(this);
	Boards.Reset();

	UStaticMesh* Cube = FFBPhysicsLib::BasicShape(TEXT("Cube"));
	const int32 Count = FMath::Max(1, FMath::CeilToInt(Size.Y / BoardWidth));
	const float W = Size.Y / Count;
	FRandomStream Random(Count * 31 + 5);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Center(Random.FRandRange(-3.f, 3.f), -Size.Y * 0.5f + W * (i + 0.5f), -3.f + Random.FRandRange(-0.6f, 0.f));
		const FRotator Rot(Random.FRandRange(-0.5f, 0.5f), 0.f, Random.FRandRange(-1.f, 1.f));
		const FLinearColor C = FMath::Lerp(DarkWood, Wood, Random.FRand());
		Boards.Add(BuildPart(this, Root, Cube, Center, FVector(Size.X, W - 1.5f, 6.f), Rot, Material, C));
	}
}

void AFBCollapsingFloor::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void AFBCollapsingFloor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();

	// Boards waiting for their turn to drop.
	for (int32 i = FallQueue.Num() - 1; i >= 0; --i)
	{
		if (Now >= FallQueue[i].Time)
		{
			if (UStaticMeshComponent* B = FallQueue[i].Part.Get())
			{
				MakeLoose(B, 6.f);
				B->SetPhysicsLinearVelocity(FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), -FMath::FRandRange(50.f, 200.f)));
				B->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 120.f);
			}
			FallQueue.RemoveAtSwap(i);
		}
	}
	if (bCollapsed)
	{
		return;
	}

	// Who is standing on it?
	float Weight = 0.f;
	FVector WeightCenter = GetActorLocation();
	bool bPlayerOn = false;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		const ACharacter* C = *It;
		const FVector Feet = FeetOf(C);
		const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Feet);
		if (FMath::Abs(Local.X) > Size.X * 0.5f || FMath::Abs(Local.Y) > Size.Y * 0.5f || FMath::Abs(Local.Z) > 30.f)
		{
			continue;
		}
		const bool bFriendly = IsFriendly(this, C);
		const bool bBig = C->GetCapsuleComponent()->GetScaledCapsuleRadius() > 45.f;
		Weight += (bFriendly && !bBig) ? PlayerWeight : CreatureWeight;
		WeightCenter = Feet;
		bPlayerOn |= (C == UGameplayStatics::GetPlayerPawn(this, 0));
	}

	if (Weight > 0.f)
	{
		Strain += Weight * DeltaSeconds / FMath::Max(0.1f, CollapseTime);
	}
	else
	{
		Strain = FMath::Max(0.f, Strain - DeltaSeconds * 0.1f); // recovers slowly - it remembers
	}

	// Creaks and sagging.
	if (Strain > 0.3f && Now >= NextCreak && Weight > 0.f)
	{
		NextCreak = Now + FMath::FRandRange(0.3f, 0.6f);
		UFBNoiseSubsystem::ReportNoise(this, WeightCenter, 0.4f, this);
		for (UStaticMeshComponent* B : Boards)
		{
			if (B && FVector::Dist2D(B->GetComponentLocation(), WeightCenter) < 120.f)
			{
				B->AddRelativeLocation(FVector(0.f, 0.f, -FMath::FRandRange(0.2f, 0.8f) * FMath::Min(Strain, 1.f)));
			}
		}
		if (bPlayerOn)
		{
			KickCamera(this, 0.6f);
			if (!bCreaked)
			{
				AFBHUD::ShowNotificationFor(this, NSLOCTEXT("FB", "FloorCreak", "The floorboards groan under you..."), 2.f);
			}
		}
		bCreaked = true;
	}

	if (Strain >= 1.f)
	{
		Collapse(WeightCenter);
	}
}

void AFBCollapsingFloor::ApplyHit(const FFBHitInfo& Hit)
{
	if (Hit.Amount >= 20.f)
	{
		Collapse(Hit.Location);
	}
}

void AFBCollapsingFloor::Collapse(const FVector& Center)
{
	if (bCollapsed)
	{
		return;
	}
	bCollapsed = true;

	// Gives way from where the weight is, spreading outward.
	const float Now = GetWorld()->GetTimeSeconds();
	for (UStaticMeshComponent* B : Boards)
	{
		if (B)
		{
			const float Dist = FVector::Dist2D(B->GetComponentLocation(), Center);
			FallQueue.Add({ B, Now + Dist / 600.f + FMath::FRandRange(0.f, 0.08f) });
		}
	}
	UFBNoiseSubsystem::ReportNoise(this, Center, 1.2f, this);
	KickCamera(this, 4.f);
	AFBHUD::ShowNotificationFor(this, NSLOCTEXT("FB", "FloorBreak", "The floor gives way!"), 2.f);
}

// ====================================================================== Debris trap

AFBDebrisTrap::AFBDebrisTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	TriggerPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TriggerPoint"));
	TriggerPoint->SetupAttachment(Root);
	TriggerPoint->SetRelativeLocation(FVector(600.f, 0.f, -200.f));
}

void AFBDebrisTrap::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AFBDebrisTrap::Build()
{
	ClearBuiltParts(this);
	Pieces.Reset();
	Supports.Reset();

	UStaticMesh* Cube = FFBPhysicsLib::BasicShape(TEXT("Cube"));
	UStaticMesh* Cyl = FFBPhysicsLib::BasicShape(TEXT("Cylinder"));
	UStaticMesh* Sphere = FFBPhysicsLib::BasicShape(TEXT("Sphere"));
	FRandomStream Random(PieceCount * 13 + int32(Type));

	auto Piece = [&](UStaticMesh* Default, const FVector& Center, const FVector& SizeCm, const FRotator& Rot, const FLinearColor& Color)
	{
		const bool bCustom = PieceMesh != nullptr;
		UStaticMeshComponent* C = BuildPart(this, Root, bCustom ? PieceMesh.Get() : Default, Center, bCustom ? FVector(100.f) : SizeCm, Rot, Material, Color);
		Pieces.Add(C);
	};

	switch (Type)
	{
	case EFBDebrisType::Logs:
	{
		// Pyramid of logs lying across the arrow, two stakes in front hold them.
		const float D = 40.f;
		int32 Placed = 0;
		for (int32 Row = 0; Placed < PieceCount; ++Row)
		{
			const int32 InRow = FMath::Max(1, 4 - Row);
			for (int32 i = 0; i < InRow && Placed < PieceCount; ++i, ++Placed)
			{
				const FVector Center(-D * (i + 0.5f + Row * 0.5f), Random.FRandRange(-15.f, 15.f), D * 0.5f + Row * D * 0.86f);
				Piece(Cyl, Center, FVector(D, D, 320.f), FRotator(0.f, Random.FRandRange(-4.f, 4.f), 90.f), FMath::Lerp(DarkWood, Wood, Random.FRand()));
			}
		}
		Supports.Add(BuildPart(this, Root, Cube, FVector(15.f, -110.f, 45.f), FVector(12.f, 12.f, 90.f), FRotator(-8.f, 0.f, 0.f), nullptr, DarkWood));
		Supports.Add(BuildPart(this, Root, Cube, FVector(15.f, 110.f, 45.f), FVector(12.f, 12.f, 90.f), FRotator(-8.f, 0.f, 0.f), nullptr, DarkWood));
		break;
	}
	case EFBDebrisType::Rocks:
		for (int32 i = 0; i < PieceCount; ++i)
		{
			const float S = Random.FRandRange(55.f, 110.f);
			const FVector Center(-Random.FRandRange(40.f, 220.f), Random.FRandRange(-150.f, 150.f), S * 0.45f + (i % 3) * 30.f);
			Piece(Sphere, Center, FVector(S, S * Random.FRandRange(0.7f, 1.2f), S * Random.FRandRange(0.6f, 0.9f)), FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Stone);
		}
		Supports.Add(BuildPart(this, Root, Cube, FVector(20.f, 0.f, 25.f), FVector(8.f, 340.f, 50.f), FRotator::ZeroRotator, nullptr, DarkWood));
		break;

	case EFBDebrisType::Beams:
		// Actor at the ceiling. Beams across the arrow, held by brackets at their ends.
		for (int32 i = 0; i < PieceCount; ++i)
		{
			const FVector Center(-i * 60.f, 0.f, -20.f);
			Piece(Cube, Center, FVector(25.f, 320.f, 25.f), FRotator(0.f, Random.FRandRange(-5.f, 5.f), 0.f), DarkWood);
		}
		Supports.Add(BuildPart(this, Root, Cube, FVector(-PieceCount * 30.f + 30.f, -170.f, -20.f), FVector(PieceCount * 60.f, 12.f, 40.f), FRotator::ZeroRotator, nullptr, Metal));
		Supports.Add(BuildPart(this, Root, Cube, FVector(-PieceCount * 30.f + 30.f, 170.f, -20.f), FVector(PieceCount * 60.f, 12.f, 40.f), FRotator::ZeroRotator, nullptr, Metal));
		break;
	}
}

void AFBDebrisTrap::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

FText AFBDebrisTrap::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	switch (Type)
	{
	case EFBDebrisType::Logs:  return NSLOCTEXT("FB", "TrapLogs", "Kick out the stakes");
	case EFBDebrisType::Rocks: return NSLOCTEXT("FB", "TrapRocks", "Pull the plank");
	default:                   return NSLOCTEXT("FB", "TrapBeams", "Knock the support loose");
	}
}

void AFBDebrisTrap::ApplyHit(const FFBHitInfo& Hit)
{
	if (Hit.Amount >= 5.f)
	{
		Release();
	}
}

void AFBDebrisTrap::Release()
{
	if (bReleased)
	{
		return;
	}
	bReleased = true;
	ReleaseTime = GetWorld()->GetTimeSeconds();

	const FVector Fwd = GetActorForwardVector();
	for (UStaticMeshComponent* S : Supports)
	{
		MakeLoose(S, 5.f);
		if (S)
		{
			S->SetPhysicsLinearVelocity(Fwd * 250.f + FVector(0.f, 0.f, 150.f));
			S->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 400.f);
		}
	}

	for (UStaticMeshComponent* P : Pieces)
	{
		MakeLoose(P, PieceMassKg);
		if (!P)
		{
			continue;
		}
		switch (Type)
		{
		case EFBDebrisType::Logs:  P->SetPhysicsLinearVelocity(Fwd * FMath::FRandRange(120.f, 220.f)); break;
		case EFBDebrisType::Rocks: P->SetPhysicsLinearVelocity(Fwd * FMath::FRandRange(150.f, 300.f) + FMath::VRand() * 60.f); break;
		case EFBDebrisType::Beams: P->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 60.f); break;
		}
	}

	UFBNoiseSubsystem::ReportNoise(this, GetActorLocation(), 1.5f, this);
	KickCamera(this, 1.5f);
}

void AFBDebrisTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bReleased)
	{
		// A creature walking under it sets it off.
		if (CreatureTriggerRadius > 0.f)
		{
			for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
			{
				if (!IsFriendly(this, *It) && FVector::Dist(It->GetActorLocation(), TriggerPoint->GetComponentLocation()) < CreatureTriggerRadius)
				{
					Release();
					break;
				}
			}
		}
		return;
	}

	// For a few seconds, the falling pieces crush whatever they hit.
	if (GetWorld()->GetTimeSeconds() - ReleaseTime > 8.f)
	{
		SetActorTickEnabled(false);
		return;
	}
	for (UStaticMeshComponent* P : Pieces)
	{
		FFBPhysicsLib::HurtCharactersTouching(P, 250.f, DamageScale, this, AlreadyHit);
	}
}

// ====================================================================== Rope bridge

AFBRopeBridge::AFBRopeBridge()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AFBRopeBridge::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AFBRopeBridge::Build()
{
	ClearBuiltParts(this);
	PlankParts.Reset();
	RopeParts.Reset();

	UStaticMesh* Cube = FFBPhysicsLib::BasicShape(TEXT("Cube"));
	UStaticMesh* Cyl = FFBPhysicsLib::BasicShape(TEXT("Cylinder"));
	FRandomStream Random(int32(Length) + 3);

	auto HeightAt = [this](float X) { return -Sag * FMath::Sin(PI * FMath::Clamp(X / Length, 0.f, 1.f)); };
	auto SlopeAt = [this](float X) { return FMath::RadiansToDegrees(FMath::Atan(-Sag * PI / Length * FMath::Cos(PI * X / Length))); };

	// Planks.
	const float Step = 30.f;
	for (float X = Step * 0.5f; X < Length; X += Step)
	{
		const FRotator Rot(SlopeAt(X), Random.FRandRange(-1.5f, 1.5f), Random.FRandRange(-1.f, 1.f));
		PlankParts.Add(BuildPart(this, Root, Cube, FVector(X, 0.f, HeightAt(X) - 2.5f), FVector(22.f, Width, 5.f), Rot, PlankMaterial,
		                         FMath::Lerp(DarkWood, Wood, Random.FRand())));
	}

	// Hand ropes, in segments following the sag. Shots hit them, walking doesn't.
	const int32 Segments = 12;
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 i = 0; i < Segments; ++i)
		{
			const float X0 = Length * i / Segments;
			const float X1 = Length * (i + 1) / Segments;
			const FVector A(X0, Side * (Width * 0.5f + 8.f), HeightAt(X0) + 90.f);
			const FVector B(X1, Side * (Width * 0.5f + 8.f), HeightAt(X1) + 90.f);
			const FVector D = B - A;
			UStaticMeshComponent* R = BuildPart(this, Root, Cyl, (A + B) * 0.5f, FVector(3.f, 3.f, D.Size()), FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator(), RopeMaterial, Rope);
			R->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			R->SetCollisionResponseToAllChannels(ECR_Ignore);
			R->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			RopeParts.Add(R);
		}
		// End posts (look at one + F to cut the ropes).
		BuildPart(this, Root, Cube, FVector(0.f, Side * (Width * 0.5f + 8.f), 50.f), FVector(16.f, 16.f, 130.f), FRotator::ZeroRotator, PlankMaterial, DarkWood);
		BuildPart(this, Root, Cube, FVector(Length, Side * (Width * 0.5f + 8.f), 50.f), FVector(16.f, 16.f, 130.f), FRotator::ZeroRotator, PlankMaterial, DarkWood);
	}
}

void AFBRopeBridge::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

FText AFBRopeBridge::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	return NSLOCTEXT("FB", "BridgeCut", "Cut the ropes");
}

void AFBRopeBridge::Interact_Implementation(AActor* InteractingActor)
{
	Snap(InteractingActor ? InteractingActor->GetActorLocation() : GetActorLocation());
}

void AFBRopeBridge::ApplyHit(const FFBHitInfo& Hit)
{
	if (Hit.Amount >= 5.f)
	{
		Snap(Hit.Location);
	}
}

void AFBRopeBridge::Snap(const FVector& Where)
{
	if (bSnapped)
	{
		return;
	}
	bSnapped = true;

	for (UStaticMeshComponent* R : RopeParts)
	{
		MakeLoose(R, 1.f);
		if (R)
		{
			R->SetCollisionResponseToAllChannels(ECR_Block);
			R->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}
	}

	// The planks go in a wave starting from where it snapped.
	const float Now = GetWorld()->GetTimeSeconds();
	const float SnapX = GetActorTransform().InverseTransformPositionNoScale(Where).X;
	for (UStaticMeshComponent* P : PlankParts)
	{
		if (P)
		{
			const float X = P->GetRelativeLocation().X;
			Falling.Add({ P, Now + FMath::Abs(X - SnapX) / 900.f + FMath::FRandRange(0.f, 0.1f) });
		}
	}
	UFBNoiseSubsystem::ReportNoise(this, Where, 1.3f, this);
}

void AFBRopeBridge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();

	if (!bSnapped)
	{
		// A creature on the bridge: the ropes can't hold it.
		if (bSnapsUnderCreature)
		{
			for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
			{
				if (IsFriendly(this, *It))
				{
					continue;
				}
				const FVector Local = GetActorTransform().InverseTransformPositionNoScale(FeetOf(*It));
				if (Local.X > 50.f && Local.X < Length - 50.f && FMath::Abs(Local.Y) < Width * 0.5f && FMath::Abs(Local.Z + Sag * 0.5f) < Sag + 80.f)
				{
					Snap(FeetOf(*It));
					break;
				}
			}
		}
		return;
	}

	for (int32 i = Falling.Num() - 1; i >= 0; --i)
	{
		if (Now >= Falling[i].Time)
		{
			if (UStaticMeshComponent* P = Falling[i].Part.Get())
			{
				MakeLoose(P, 3.f);
				P->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 200.f);
			}
			Falling.RemoveAtSwap(i);
		}
	}
	if (Falling.Num() == 0)
	{
		SetActorTickEnabled(false);
	}
}
