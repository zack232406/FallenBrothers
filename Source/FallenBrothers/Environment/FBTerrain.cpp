// Fallen Brothers - Rolling ground around the town.

#include "Environment/FBTerrain.h"

#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

AFBTerrain::AFBTerrain()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->bUseAsyncCooking = true;
	Mesh->bUseComplexAsSimpleCollision = true; // the real slopes are the collision
	Mesh->SetCastShadow(true);
	Mesh->SetMobility(EComponentMobility::Static);
}

void AFBTerrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

AFBTerrain* AFBTerrain::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AFBTerrain> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

float AFBTerrain::GroundZ(const UWorld* World, const FVector2D& XY, float Default)
{
	if (const AFBTerrain* T = Find(World))
	{
		return T->GetActorLocation().Z + T->HeightAt(XY);
	}
	return Default;
}

float AFBTerrain::FlatDistance(const FVector2D& P) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FBox2D& B : FlatAreas)
	{
		const float DX = FMath::Max3(float(B.Min.X - P.X), 0.f, float(P.X - B.Max.X));
		const float DY = FMath::Max3(float(B.Min.Y - P.Y), 0.f, float(P.Y - B.Max.Y));
		Best = FMath::Min(Best, FMath::Sqrt(DX * DX + DY * DY));
	}
	for (const FFBFlatStrip& S : FlatStrips)
	{
		const FVector2D AB = S.B - S.A;
		const float Len2 = FMath::Max(float(AB.SizeSquared()), 1.f);
		const float T = FMath::Clamp(float(FVector2D::DotProduct(P - S.A, AB)) / Len2, 0.f, 1.f);
		const float D = float(FVector2D::Distance(P, S.A + AB * T)) - S.HalfWidth;
		Best = FMath::Min(Best, FMath::Max(D, 0.f));
	}
	return Best;
}

float AFBTerrain::LakeAlpha(const FVector2D& P) const
{
	const FVector2D D = (P - LakeCenter) / FVector2D(FMath::Max(LakeRadius.X, 100.0), FMath::Max(LakeRadius.Y, 100.0));
	// A wobbly shoreline: real lakes are never perfect ellipses.
	const float Angle = FMath::Atan2(float(D.Y), float(D.X));
	const float Wobble = 1.f + 0.08f * FMath::Sin(Angle * 2.f + Seed) + 0.06f * FMath::Sin(Angle * 3.f + Seed * 2.f) + 0.04f * FMath::Cos(Angle * 5.f - Seed);
	return float(D.Size()) / Wobble;
}

bool AFBTerrain::IsUnderWater(const FVector2D& World, float Margin) const
{
	// Only at the lake: the flat town and farmland also sit low, but they are not water.
	if (!bLake)
	{
		return false;
	}
	const float Grow = Margin / FMath::Max(1.f, FMath::Min(float(LakeRadius.X), float(LakeRadius.Y)));
	return LakeAlpha(World) < 1.05f + Grow && HeightAt(World) < WaterLevel + Margin;
}

float AFBTerrain::HeightAt(const FVector2D& World) const
{
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	const FVector2D Local = World - Center;

	// Distance out towards the mountains (0 = centre, 1 = edge ellipse).
	const FVector2D E = Local / FVector2D(FMath::Max(EdgeRadius.X, 1000.0), FMath::Max(EdgeRadius.Y, 1000.0));
	const float Out = float(E.Size());

	// Rolling hills: three octaves of smooth noise, shaped so there are real valleys between rounded tops.
	const FVector2D P = World / FMath::Max(HillScale, 1000.f) + FVector2D(Seed * 3.17f, Seed * -1.93f);
	float N = FMath::PerlinNoise2D(P) * 0.62f + FMath::PerlinNoise2D(P * 2.07f + 11.3f) * 0.27f + FMath::PerlinNoise2D(P * 4.3f - 5.1f) * 0.11f;
	N = FMath::Clamp(N * 0.5f + 0.5f, 0.f, 1.f);       // 0..1
	N = N * N * (3.f - 2.f * N);                          // softer tops and bottoms

	const float Grow = FMath::Lerp(0.45f, 1.f, FMath::SmoothStep(0.25f, 0.85f, Out));   // bigger towards the mountains
	const float EdgeFade = 1.f - FMath::SmoothStep(0.9f, 1.0f, Out);                    // down to 0 where the mountains begin
	const float Flat = FMath::SmoothStep(0.f, FMath::Max(FlatBlend, 100.f), FlatDistance(World));

	// Small bumps everywhere off the roads (real ground is never a table top).
	const float Bumps = FMath::PerlinNoise2D(World / 900.f + FVector2D(float(Seed), float(Seed))) * 35.f;

	float H = (HillHeight * N * Grow + Bumps) * Flat * EdgeFade;

	if (bLake)
	{
		// The basin: deep in the middle, a gentle beach at the shore, then the land around (kept low near the water).
		const float L = LakeAlpha(World);
		if (L < 1.8f)
		{
			const float Bed = -LakeDepth * FMath::Square(1.f - FMath::SmoothStep(0.f, 1.f, L)) + WaterLevel - 15.f;
			const float Shore = FMath::Lerp(WaterLevel + 25.f, H, FMath::SmoothStep(1.05f, 1.8f, L));
			H = (L < 1.f) ? FMath::Lerp(Bed, WaterLevel + 20.f, FMath::SmoothStep(0.75f, 1.f, L)) : Shore;
		}
	}
	return H;
}

void AFBTerrain::Build()
{
	const int32 NX = FMath::Clamp(FMath::CeilToInt(Size.X / FMath::Max(CellSize, 100.f)), 2, 600);
	const int32 NY = FMath::Clamp(FMath::CeilToInt(Size.Y / FMath::Max(CellSize, 100.f)), 2, 600);
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	const FVector2D Step(Size.X / NX, Size.Y / NY);
	auto Index = [NY](int32 I, int32 J) { return I * (NY + 1) + J; };

	TArray<FVector> V;
	TArray<FVector2D> UV;
	V.SetNumUninitialized((NX + 1) * (NY + 1));
	UV.SetNumUninitialized(V.Num());
	for (int32 I = 0; I <= NX; ++I)
	{
		for (int32 J = 0; J <= NY; ++J)
		{
			const FVector2D L(-Size.X * 0.5f + I * Step.X, -Size.Y * 0.5f + J * Step.Y);
			V[Index(I, J)] = FVector(L.X, L.Y, HeightAt(Center + L));
			UV[Index(I, J)] = (Center + L) / 700.f;
		}
	}

	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	Normals.SetNumUninitialized(V.Num());
	Tangents.SetNumUninitialized(V.Num());
	for (int32 I = 0; I <= NX; ++I)
	{
		for (int32 J = 0; J <= NY; ++J)
		{
			const FVector DX = V[Index(FMath::Min(I + 1, NX), J)] - V[Index(FMath::Max(I - 1, 0), J)];
			const FVector DY = V[Index(I, FMath::Min(J + 1, NY))] - V[Index(I, FMath::Max(J - 1, 0))];
			FVector N = FVector::CrossProduct(DX, DY).GetSafeNormal();
			if (N.Z < 0.f)
			{
				N = -N;
			}
			Normals[Index(I, J)] = N.IsNearlyZero() ? FVector::UpVector : N;
			Tangents[Index(I, J)] = FProcMeshTangent(DX.GetSafeNormal(), false);
		}
	}

	// Triangles by look: grass, steep slopes, lake shore / bed.
	TArray<int32> Tris[3];
	auto AddTri = [&](int32 A, int32 B, int32 C)
	{
		const FVector Cross = FVector::CrossProduct(V[B] - V[A], V[C] - V[A]);
		if (Cross.Z > 0.f)
		{
			Swap(B, C); // Unreal's front faces: this winding faces up (the other way the ground was culled from above)
		}
		const FVector Mid = (V[A] + V[B] + V[C]) / 3.f;
		const float Up = FMath::Abs(Cross.GetSafeNormal().Z);
		int32 Section = 0;
		if (bLake && ShoreMaterial && Mid.Z < WaterLevel + 18.f && LakeAlpha(Center + FVector2D(Mid.X, Mid.Y)) < 1.12f) // the bed and a narrow beach; grass right up to it
		{
			Section = 2;
		}
		else if (SlopeMaterial && Up < 0.86f)
		{
			Section = 1; // steeper than ~30 degrees: bare earth
		}
		Tris[Section].Add(A);
		Tris[Section].Add(B);
		Tris[Section].Add(C);
	};
	for (int32 I = 0; I < NX; ++I)
	{
		for (int32 J = 0; J < NY; ++J)
		{
			const int32 I0 = Index(I, J);
			const int32 I1 = Index(I + 1, J);
			const int32 I2 = Index(I + 1, J + 1);
			const int32 I3 = Index(I, J + 1);
			AddTri(I0, I1, I3);
			AddTri(I1, I2, I3);
		}
	}

	const TArray<FLinearColor> NoColors;
	Mesh->ClearAllMeshSections();
	UMaterialInterface* Mats[3] = { GrassMaterial.Get(), SlopeMaterial.Get(), ShoreMaterial.Get() };
	for (int32 S = 0; S < 3; ++S)
	{
		if (Tris[S].Num() == 0)
		{
			continue;
		}
		Mesh->CreateMeshSection_LinearColor(S, V, Tris[S], Normals, UV, NoColors, Tangents, true);
		if (Mats[S])
		{
			Mesh->SetMaterial(S, Mats[S]);
		}
	}
}
