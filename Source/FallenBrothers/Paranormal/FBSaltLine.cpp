// Fallen Brothers - Salt line.

#include "Paranormal/FBSaltLine.h"

#include "Paranormal/FBParanormalSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBSaltLine::AFBSaltLine()
{
	PrimaryActorTick.bCanEverTick = false;

	Grains = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Grains"));
	SetRootComponent(Grains);
	Grains->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Grains->SetCastShadow(false);
	Grains->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Cube.Succeeded())
	{
		Grains->SetStaticMesh(Cube.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Grains->SetMaterial(0, BasicMaterial.Object);
	}
}

void AFBSaltLine::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* MID = Grains->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.95f, 0.92f));
	}

	// Pre-made line placed in the editor: Points are relative to the actor (edit widgets).
	if (Points.Num() > 1)
	{
		TArray<FVector> Local = Points;
		Points.Reset();
		for (const FVector& P : Local)
		{
			AddPoint(GetActorTransform().TransformPosition(P));
		}
	}

	if (UFBParanormalSubsystem* Subsystem = GetWorld()->GetSubsystem<UFBParanormalSubsystem>())
	{
		Subsystem->RegisterSaltLine(this);
	}
}

void AFBSaltLine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UFBParanormalSubsystem* Subsystem = World->GetSubsystem<UFBParanormalSubsystem>())
		{
			Subsystem->UnregisterSaltLine(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AFBSaltLine::AddPoint(const FVector& WorldPoint)
{
	if (Points.Num() > 0)
	{
		const FVector& Last = Points.Last();
		AddSegmentVisual(Last, WorldPoint);
		TotalLength += FVector::Dist2D(Last, WorldPoint);
	}
	Points.Add(WorldPoint);
}

void AFBSaltLine::AddSegmentVisual(const FVector& A, const FVector& B)
{
	// One thin white strip per segment (engine cube = 100 cm).
	const FVector Mid = (A + B) * 0.5f + FVector(0.f, 0.f, 0.5f);
	const FVector Dir = B - A;
	const float Len = Dir.Size();
	if (Len < 1.f)
	{
		return;
	}
	const FRotator Rot = Dir.Rotation();
	const FTransform WorldT(Rot, Mid, FVector(Len / 100.f, 0.06f, 0.01f));
	Grains->AddInstance(WorldT, /*bWorldSpace*/ true);
}

static bool SegmentsIntersect2D(const FVector& P1, const FVector& P2, const FVector& Q1, const FVector& Q2)
{
	auto Cross = [](const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; };
	const FVector2D p(P1), r = FVector2D(P2) - FVector2D(P1);
	const FVector2D q(Q1), s = FVector2D(Q2) - FVector2D(Q1);
	const double Denom = Cross(r, s);
	if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
	{
		return false; // Parallel
	}
	const double t = Cross(q - p, s) / Denom;
	const double u = Cross(q - p, r) / Denom;
	return t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0;
}

bool AFBSaltLine::BlocksPath(const FVector& A, const FVector& B) const
{
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		if (SegmentsIntersect2D(A, B, Points[i - 1], Points[i]))
		{
			return true;
		}
	}
	return false;
}
