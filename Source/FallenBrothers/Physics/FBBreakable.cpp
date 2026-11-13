// Fallen Brothers - Breakable objects.

#include "Physics/FBBreakable.h"

#include "Core/FBNoiseSubsystem.h"
#include "Physics/FBPhysicsLib.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor GlassColor(0.55f, 0.68f, 0.72f);
	const FLinearColor WoodColor(0.28f, 0.19f, 0.11f);
	const FLinearColor FrameColor(0.12f, 0.1f, 0.08f);
	const FLinearColor BottleColor(0.12f, 0.25f, 0.12f);
}

AFBBreakable::AFBBreakable()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

FVector AFBBreakable::GetSize() const
{
	if (!Size.IsNearlyZero())
	{
		return Size;
	}
	switch (Type)
	{
	case EFBBreakableType::Window:    return FVector(8.f, 120.f, 140.f);
	case EFBBreakableType::Barricade: return FVector(6.f, 110.f, 220.f); // fits our FBDoor gaps
	case EFBBreakableType::Crate:     return FVector(60.f, 60.f, 60.f);
	case EFBBreakableType::Bottle:    return FVector(8.f, 8.f, 28.f);
	}
	return FVector(100.f);
}

FLinearColor AFBBreakable::PartColor() const
{
	switch (Type)
	{
	case EFBBreakableType::Window: return GlassColor;
	case EFBBreakableType::Bottle: return BottleColor;
	default:                       return WoodColor;
	}
}

// ------------------------------------------------------------------ Build

UStaticMeshComponent* AFBBreakable::AddPart(UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm, const FRotator& Rot,
                                            bool bBreakable, UMaterialInterface* Mat, const FLinearColor& Color)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transactional);
	C->CreationMethod = EComponentCreationMethod::UserConstructionScript; // remade on every rebuild
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetRelativeLocationAndRotation(Center, Rot);
	C->SetRelativeScale3D(SizeCm / 100.f);
	C->SetCollisionProfileName(IsLoose() ? UCollisionProfile::PhysicsActor_ProfileName : UCollisionProfile::BlockAll_ProfileName);
	C->SetNotifyRigidBodyCollision(bBreakable);
	if (!IsLoose())
	{
		C->SetCollisionObjectType(ECC_WorldDynamic); // so shotgun pellets / knives find it
	}
	C->RegisterComponent();
	FFBPhysicsLib::ApplyLook(C, Mat, Color);
	if (bBreakable)
	{
		Parts.Add(C);
	}
	return C;
}

void AFBBreakable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AFBBreakable::Build()
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
	Parts.Reset();

	const FVector S = GetSize();
	switch (Type)
	{
	case EFBBreakableType::Window:
	{
		// Origin = bottom centre of the opening. Frame stays, the pane breaks.
		const float F = 8.f;
		AddPart(CubeMesh, FVector(0.f, 0.f, F * 0.5f),               FVector(S.X + 4.f, S.Y, F), FRotator::ZeroRotator, false, FrameMaterial, FrameColor);
		AddPart(CubeMesh, FVector(0.f, 0.f, S.Z - F * 0.5f),         FVector(S.X + 4.f, S.Y, F), FRotator::ZeroRotator, false, FrameMaterial, FrameColor);
		AddPart(CubeMesh, FVector(0.f, -S.Y * 0.5f + F * 0.5f, S.Z * 0.5f), FVector(S.X + 4.f, F, S.Z), FRotator::ZeroRotator, false, FrameMaterial, FrameColor);
		AddPart(CubeMesh, FVector(0.f,  S.Y * 0.5f - F * 0.5f, S.Z * 0.5f), FVector(S.X + 4.f, F, S.Z), FRotator::ZeroRotator, false, FrameMaterial, FrameColor);
		AddPart(CubeMesh, FVector(0.f, 0.f, S.Z * 0.5f), FVector(1.5f, S.Y - F * 2.f, S.Z - F * 2.f), FRotator::ZeroRotator, true, Material, GlassColor);
		break;
	}
	case EFBBreakableType::Barricade:
	{
		// Origin = bottom centre of the doorway. Planks across it, a bit crooked.
		FRandomStream Random(GetFName().GetNumber() + 77);
		for (int32 i = 0; i < Planks; ++i)
		{
			const float A = Planks > 1 ? float(i) / (Planks - 1) : 0.5f;
			const float Z = S.Z * FMath::Lerp(0.2f, 0.8f, A);
			const FRotator Rot(0.f, 0.f, Random.FRandRange(-18.f, 18.f));
			AddPart(CubeMesh, FVector(S.X * 0.5f, 0.f, Z), FVector(4.f, S.Y * 1.2f, 18.f), Rot, true, Material, WoodColor);
		}
		break;
	}
	case EFBBreakableType::Crate:
		AddPart(CubeMesh, FVector(0.f, 0.f, S.Z * 0.5f), S, FRotator::ZeroRotator, true, Material, WoodColor);
		break;

	case EFBBreakableType::Bottle:
		AddPart(CylinderMesh, FVector(0.f, 0.f, S.Z * 0.5f), S, FRotator::ZeroRotator, true, Material, BottleColor);
		break;
	}
}

void AFBBreakable::BeginPlay()
{
	Super::BeginPlay();
	Build();

	for (UStaticMeshComponent* Part : Parts)
	{
		if (!Part)
		{
			continue;
		}
		Part->OnComponentHit.AddUniqueDynamic(this, &AFBBreakable::OnPartHit);
		if (IsLoose())
		{
			Part->SetSimulatePhysics(true);
			Part->SetMassOverrideInKg(NAME_None, Type == EFBBreakableType::Crate ? 14.f : 0.6f, true);
		}
	}
}

// ------------------------------------------------------------------ Getting hit

void AFBBreakable::OnPartHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	UStaticMeshComponent* Part = Cast<UStaticMeshComponent>(HitComp);
	if (bBroken || !Part || !Parts.Contains(Part) || OtherActor == this)
	{
		return;
	}

	// A creature charging through.
	if (const ACharacter* C = Cast<ACharacter>(OtherActor))
	{
		if (C != UGameplayStatics::GetPlayerPawn(this, 0) && C->GetVelocity().Size() >= CreatureBreakSpeed)
		{
			BreakAll(Hit.ImpactPoint, C->GetVelocity() * 0.7f);
		}
		return;
	}

	// Something thrown / swung into it.
	float Speed = 0.f;
	FVector Push = -Hit.ImpactNormal * 200.f;
	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		const FVector V = OtherComp->GetPhysicsLinearVelocity();
		Speed = V.Size();
		Push = V * 0.5f;
	}

	// The crate / bottle itself crashing into something.
	if (IsLoose())
	{
		const float SelfDeltaV = NormalImpulse.Size() / FMath::Max(Part->GetMass(), 0.1f);
		const float Threshold = Type == EFBBreakableType::Crate ? BreakSpeed * 1.6f : BreakSpeed;
		if (SelfDeltaV >= Threshold)
		{
			Speed = FMath::Max(Speed, BreakSpeed);
			Push = Part->GetPhysicsLinearVelocity() * 0.4f;
		}
		else if (Speed < Threshold)
		{
			return;
		}
	}

	if (Speed >= BreakSpeed)
	{
		BreakPart(Part, Hit.ImpactPoint, Push);
	}
}

void AFBBreakable::ApplyHit(const FFBHitInfo& Hit)
{
	if (bBroken)
	{
		return;
	}

	FVector Push = GetActorForwardVector() * -250.f;
	if (Hit.Causer)
	{
		Push = (Hit.Location - Hit.Causer->GetActorLocation()).GetSafeNormal() * 300.f;
	}

	switch (Type)
	{
	case EFBBreakableType::Window:
	case EFBBreakableType::Bottle:
		BreakAll(Hit.Location, Push);
		break;

	case EFBBreakableType::Barricade:
	{
		if (Hit.Amount < 5.f)
		{
			return;
		}
		// The plank closest to the hit.
		UStaticMeshComponent* Best = nullptr;
		float BestDist = FLT_MAX;
		for (UStaticMeshComponent* P : Parts)
		{
			const float D = P ? FVector::DistSquared(P->GetComponentLocation(), Hit.Location) : FLT_MAX;
			if (D < BestDist)
			{
				BestDist = D;
				Best = P;
			}
		}
		BreakPart(Best, Hit.Location, Push);
		break;
	}

	case EFBBreakableType::Crate:
		CrateDamage += Hit.Amount;
		if (CrateDamage >= 25.f)
		{
			BreakAll(Hit.Location, Push);
		}
		break;
	}
}

// ------------------------------------------------------------------ Breaking

void AFBBreakable::BreakAll(const FVector& From, const FVector& Push)
{
	const TArray<TObjectPtr<UStaticMeshComponent>> Copy = Parts;
	for (UStaticMeshComponent* P : Copy)
	{
		BreakPart(P, From, Push);
	}
}

void AFBBreakable::BreakPart(UStaticMeshComponent* Part, const FVector& From, const FVector& Push)
{
	if (!Part || !Parts.Contains(Part))
	{
		return;
	}
	Parts.Remove(Part);

	switch (Type)
	{
	case EFBBreakableType::Window:    Shatter(Part, From, Push); break;
	case EFBBreakableType::Bottle:    Shatter(Part, From, Push); break;
	case EFBBreakableType::Barricade: SplitPlank(Part, Push); break;
	case EFBBreakableType::Crate:     SmashCrate(Part, Push); break;
	}

	// The intact part disappears (destroyed a moment later - not inside its own hit event).
	Part->SetSimulatePhysics(false);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetVisibility(false);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Part, [Part]() { Part->DestroyComponent(); }));

	const float Loudness = Type == EFBBreakableType::Window ? 1.f : (Type == EFBBreakableType::Bottle ? 0.5f : 0.7f);
	UFBNoiseSubsystem::ReportNoise(this, Part->GetComponentLocation(), Loudness, this);
	CheckAllBroken();
}

void AFBBreakable::Shatter(UStaticMeshComponent* Part, const FVector& From, const FVector& Push)
{
	const FTransform T = Part->GetComponentTransform();
	const FVector S = T.GetScale3D() * 100.f; // engine shapes are 100 cm
	const bool bBottle = Type == EFBBreakableType::Bottle;
	const int32 Cols = bBottle ? 2 : 5;
	const int32 Rows = bBottle ? 4 : 5;

	for (int32 R = 0; R < Rows; ++R)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			const float CellY = S.Y / Cols;
			const float CellZ = S.Z / Rows;
			const FVector Local(FMath::FRandRange(-S.X, S.X) * 0.3f,
			                    -S.Y * 0.5f + CellY * (Col + 0.5f) + FMath::FRandRange(-0.2f, 0.2f) * CellY,
			                    -S.Z * 0.5f + CellZ * (R + 0.5f) + FMath::FRandRange(-0.2f, 0.2f) * CellZ);
			const FVector World = T.TransformPositionNoScale(Local);
			const FRotator Rot = (T.GetRotation() * FRotator(FMath::FRandRange(-30.f, 30.f), 0.f, FMath::FRandRange(0.f, 360.f)).Quaternion()).Rotator();
			const FVector PieceSize = bBottle
				? FVector(FMath::FRandRange(2.f, 4.f), FMath::FRandRange(3.f, 6.f), FMath::FRandRange(3.f, 7.f))
				: FVector(0.8f, CellY * FMath::FRandRange(0.5f, 1.f), CellZ * FMath::FRandRange(0.5f, 1.f));

			UStaticMeshComponent* Shard = FFBPhysicsLib::MakePiece(this, CubeMesh, FTransform(Rot, World), PieceSize, Material, PartColor(), bBottle ? 0.05f : 0.25f, true);
			if (!Shard)
			{
				continue;
			}
			const FVector Away = (World - From).GetSafeNormal();
			Shard->SetPhysicsLinearVelocity(Push * FMath::FRandRange(0.6f, 1.1f) + Away * FMath::FRandRange(60.f, 180.f) + FMath::VRand() * 40.f);
			Shard->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * FMath::FRandRange(200.f, 700.f));
			KeepDebris(Shard);
		}
	}
}

void AFBBreakable::SplitPlank(UStaticMeshComponent* Part, const FVector& Push)
{
	const FTransform T = Part->GetComponentTransform();
	const FVector S = T.GetScale3D() * 100.f;
	const float Break = FMath::FRandRange(0.35f, 0.65f); // where it cracks
	const float LenA = S.Y * Break;
	const float LenB = S.Y - LenA;

	const FVector CenterA = T.TransformPositionNoScale(FVector(0.f, -S.Y * 0.5f + LenA * 0.5f, 0.f));
	const FVector CenterB = T.TransformPositionNoScale(FVector(0.f, S.Y * 0.5f - LenB * 0.5f, 0.f));
	const FVector Right = T.GetRotation().GetRightVector();

	UStaticMeshComponent* A = FFBPhysicsLib::MakePiece(this, CubeMesh, FTransform(T.GetRotation(), CenterA), FVector(S.X, LenA * 0.97f, S.Z), Material, WoodColor, 2.f, true);
	UStaticMeshComponent* B = FFBPhysicsLib::MakePiece(this, CubeMesh, FTransform(T.GetRotation(), CenterB), FVector(S.X, LenB * 0.97f, S.Z), Material, WoodColor, 2.f, true);
	for (UStaticMeshComponent* P : { A, B })
	{
		if (P)
		{
			const float Side = (P == A) ? -1.f : 1.f;
			P->SetPhysicsLinearVelocity(Push + Right * Side * FMath::FRandRange(40.f, 120.f) + FVector(0.f, 0.f, 80.f));
			P->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 300.f);
			KeepDebris(P);
		}
	}
}

void AFBBreakable::SmashCrate(UStaticMeshComponent* Part, const FVector& Push)
{
	const FTransform T = Part->GetComponentTransform();
	const FVector S = T.GetScale3D() * 100.f;
	const FVector Center = T.GetLocation();
	const FQuat Q = T.GetRotation();
	const float Board = 3.f;

	// Six boards: the crate's sides fly apart.
	struct FSide { FVector Offset; FVector Size; };
	const FSide Sides[] = {
		{ FVector( S.X * 0.5f, 0.f, 0.f), FVector(Board, S.Y, S.Z) },
		{ FVector(-S.X * 0.5f, 0.f, 0.f), FVector(Board, S.Y, S.Z) },
		{ FVector(0.f,  S.Y * 0.5f, 0.f), FVector(S.X, Board, S.Z) },
		{ FVector(0.f, -S.Y * 0.5f, 0.f), FVector(S.X, Board, S.Z) },
		{ FVector(0.f, 0.f,  S.Z * 0.5f), FVector(S.X, S.Y, Board) },
		{ FVector(0.f, 0.f, -S.Z * 0.5f), FVector(S.X, S.Y, Board) },
	};
	for (const FSide& Side : Sides)
	{
		const FVector World = Center + Q.RotateVector(Side.Offset);
		UStaticMeshComponent* P = FFBPhysicsLib::MakePiece(this, CubeMesh, FTransform(Q, World), Side.Size, Material, WoodColor, 1.5f, true);
		if (P)
		{
			P->SetPhysicsLinearVelocity(Push + Q.RotateVector(Side.Offset).GetSafeNormal() * FMath::FRandRange(120.f, 260.f) + FVector(0.f, 0.f, 100.f));
			P->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 250.f);
			KeepDebris(P);
		}
	}

	// Splinters.
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector World = Center + FMath::VRand() * S.X * 0.3f;
		UStaticMeshComponent* P = FFBPhysicsLib::MakePiece(this, CubeMesh, FTransform(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), World),
		                                                   FVector(2.f, FMath::FRandRange(10.f, 30.f), 3.f), Material, WoodColor, 0.2f, true);
		if (P)
		{
			P->SetPhysicsLinearVelocity(Push + FMath::VRand() * 250.f + FVector(0.f, 0.f, 150.f));
			KeepDebris(P);
		}
	}
}

void AFBBreakable::CheckAllBroken()
{
	if (Parts.Num() > 0 || bBroken)
	{
		return;
	}
	bBroken = true;

	if (SpawnOnBreak)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		GetWorld()->SpawnActor<AActor>(SpawnOnBreak, GetActorLocation() + FVector(0.f, 0.f, 20.f), GetActorRotation(), Params);
	}
}

void AFBBreakable::KeepDebris(UStaticMeshComponent* Piece)
{
	if (!Piece)
	{
		return;
	}
	Debris.Add(Piece);
	if (DebrisLifetime > 0.f)
	{
		FTimerHandle Handle;
		TWeakObjectPtr<UStaticMeshComponent> Weak = Piece;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Weak]()
		{
			if (UStaticMeshComponent* P = Weak.Get())
			{
				Debris.Remove(P);
				P->DestroyComponent();
			}
		}), DebrisLifetime * FMath::FRandRange(0.9f, 1.1f), false);
	}
}
