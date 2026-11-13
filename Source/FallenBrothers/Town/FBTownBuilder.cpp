// Fallen Brothers - Town builder.

#include "Town/FBTownBuilder.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RandomStream.h"

namespace FBTown
{
	/** 0 = X axis, 1 = Y axis (rows are always axis-aligned in the builder's space). */
	static int32 AxisOf(const FVector& V) { return FMath::Abs(V.X) > 0.5f ? 0 : 1; }

	static float YawFacing(const FVector& Direction) { return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)); }
}

AFBTownBuilder::AFBTownBuilder()
{
	Asphalt = MakeISM(TEXT("Asphalt"), CubeMesh, true);
	Sidewalks = MakeISM(TEXT("Sidewalks"), CubeMesh, true);
	Paint = MakeISM(TEXT("Paint"), CubeMesh, false);
}

void AFBTownBuilder::ApplyColors()
{
	ApplyLook(Asphalt, RoadMaterial, FLinearColor(0.06f, 0.06f, 0.065f));
	ApplyLook(Sidewalks, SidewalkMaterial, FLinearColor(0.32f, 0.31f, 0.3f));
	SetColor(Paint, FLinearColor(0.75f, 0.68f, 0.45f));
}

// ------------------------------------------------------------------ Main

void AFBTownBuilder::Rebuild()
{
	// Remove the per-model components made by the previous build.
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* C : Components)
	{
		if (C && C->CreationMethod == EComponentCreationMethod::UserConstructionScript)
		{
			C->DestroyComponent();
		}
	}
	MeshISMs.Reset();
	Occupied.Reset();
	Reserved.Reset();
	CrossX.Reset();
	LightCount = 0;

	// Hand-placed story buildings stay free.
	if (UWorld* World = GetWorld())
	{
		const FTransform ToLocal = GetActorTransform().Inverse();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (*It != this && !ReservedTag.IsNone() && It->ActorHasTag(ReservedTag))
			{
				const FBox WorldBox = It->GetComponentsBoundingBox(true);
				if (WorldBox.IsValid)
				{
					Reserved.Add(WorldBox.TransformBy(ToLocal).ExpandBy(150.f));
				}
			}
		}
	}

	TArray<FRow> Rows;
	BuildStreets(Rows);

	FRandomStream Random(Seed);
	for (const FRow& Row : Rows)
	{
		FillRow(Row, Random);
	}
	for (const FRow& Row : Rows)
	{
		PlaceProps(Row, Random);
	}
}

// ------------------------------------------------------------------ Streets

void AFBTownBuilder::BuildStreets(TArray<FRow>& OutRows)
{
	const float HalfL = MainStreetLength * 0.5f;
	const float HalfR = RoadWidth * 0.5f;
	const float SW = SidewalkWidth;
	const float CornerMargin = 300.f;

	for (int32 i = 1; i <= SideStreets; ++i)
	{
		CrossX.Add(-HalfL + MainStreetLength * i / (SideStreets + 1));
	}

	auto AddRoadSlab = [this](const FVector& Center, const FVector& Size)
	{
		AddBox(Asphalt, Center + FVector(0.f, 0.f, -3.f), FVector(Size.X, Size.Y, 10.f)); // top at +2 cm
	};
	auto AddSidewalk = [this](const FVector& Center, const FVector& Size)
	{
		AddBox(Sidewalks, FVector(Center.X, Center.Y, CurbHeight * 0.5f), FVector(Size.X, Size.Y, CurbHeight));
	};
	auto AddDashes = [this](const FVector& From, const FVector& Dir, float Length)
	{
		const FRotator Rot(0.f, FBTown::YawFacing(Dir), 0.f);
		for (float S = 150.f; S < Length - 300.f; S += 600.f)
		{
			AddBox(Paint, From + Dir * (S + 150.f) + FVector(0.f, 0.f, 2.5f), FVector(300.f, 12.f, 1.f), Rot);
		}
	};

	// ---- Main Street (along X)
	AddRoadSlab(FVector(0.f, 0.f, 0.f), FVector(MainStreetLength, RoadWidth, 0.f));

	// Sidewalks + lot rows between the side streets.
	TArray<float> Cuts;
	Cuts.Add(-HalfL);
	Cuts.Append(CrossX);
	Cuts.Add(HalfL);

	for (int32 Seg = 0; Seg + 1 < Cuts.Num(); ++Seg)
	{
		const bool bFirst = (Seg == 0);
		const bool bLast = (Seg + 2 == Cuts.Num());
		const float A = Cuts[Seg] + (bFirst ? 0.f : HalfR);
		const float B = Cuts[Seg + 1] - (bLast ? 0.f : HalfR);
		if (B - A < 100.f)
		{
			continue;
		}

		for (const float Side : { 1.f, -1.f })
		{
			AddSidewalk(FVector((A + B) * 0.5f, Side * (HalfR + SW * 0.5f), 0.f), FVector(B - A, SW, 0.f));

			FRow Row;
			Row.Origin = FVector::ZeroVector;
			Row.Dir = FVector(1.f, 0.f, 0.f);
			Row.Normal = FVector(0.f, Side, 0.f);
			Row.StartS = A + (bFirst ? 200.f : SW + CornerMargin);
			Row.EndS = B - (bLast ? 200.f : SW + CornerMargin);
			Row.bMainStreet = true;
			if (Row.EndS - Row.StartS > 500.f)
			{
				OutRows.Add(Row);
			}
		}

		AddDashes(FVector(A, 0.f, 0.f), FVector(1.f, 0.f, 0.f), B - A);
	}

	// ---- Side streets (along Y, both directions from Main Street)
	for (const float X : CrossX)
	{
		for (const float Dir : { 1.f, -1.f })
		{
			const FVector D(0.f, Dir, 0.f);
			const float Start = HalfR;                     // road edge of Main Street
			const float Len = SideStreetLength;
			AddRoadSlab(FVector(X, Dir * (Start + Len * 0.5f), 0.f), FVector(RoadWidth, Len, 0.f));
			AddDashes(FVector(X, Dir * (Start + SW + 200.f), 0.f), D, Len - SW - 200.f);

			// Crosswalk stripes where the side street meets Main Street.
			for (float Off = -HalfR + 60.f; Off < HalfR - 40.f; Off += 120.f)
			{
				AddBox(Paint, FVector(X + Off, Dir * (Start + SW * 0.5f), 2.5f), FVector(60.f, SW * 0.8f, 1.f));
			}

			for (const float Side : { 1.f, -1.f })
			{
				AddSidewalk(FVector(X + Side * (HalfR + SW * 0.5f), Dir * (Start + Len * 0.5f), 0.f), FVector(SW, Len, 0.f));

				FRow Row;
				Row.Origin = FVector(X, 0.f, 0.f);
				Row.Dir = D;
				Row.Normal = FVector(Side, 0.f, 0.f);
				Row.StartS = SideLotStart;
				Row.EndS = Start + Len - 300.f;
				Row.bMainStreet = false;
				if (Row.EndS - Row.StartS > 500.f)
				{
					OutRows.Add(Row);
				}
			}
		}
	}
}

// ------------------------------------------------------------------ Buildings

const FFBTownBuildingType* AFBTownBuilder::PickBuilding(bool bMainStreet, FRandomStream& Random) const
{
	float Total = 0.f;
	for (const FFBTownBuildingType& B : Buildings)
	{
		if (B.Mesh && B.Weight > 0.f && (bMainStreet ? B.bMainStreet : B.bSideStreets))
		{
			Total += B.Weight;
		}
	}
	if (Total <= 0.f)
	{
		return nullptr;
	}

	float Roll = Random.FRandRange(0.f, Total);
	const FFBTownBuildingType* Last = nullptr;
	for (const FFBTownBuildingType& B : Buildings)
	{
		if (B.Mesh && B.Weight > 0.f && (bMainStreet ? B.bMainStreet : B.bSideStreets))
		{
			Last = &B;
			Roll -= B.Weight;
			if (Roll <= 0.f)
			{
				return &B;
			}
		}
	}
	return Last;
}

FBox AFBTownBuilder::PlaceMesh(UStaticMesh* Mesh, float Yaw, float Scale, const FRow& Row, float S, float DistanceFromRoad,
                               float GroundZ, bool bDryRun, FTransform& OutTransform) const
{
	const FTransform RotScale(FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, FVector(Scale));
	const FBox Box = Mesh->GetBoundingBox().TransformBy(RotScale);

	const int32 A = FBTown::AxisOf(Row.Dir);     // along the road
	const int32 N = 1 - A;                       // towards the lots
	const float NSign = Row.Normal[N] >= 0.f ? 1.f : -1.f;

	FVector L = FVector::ZeroVector;
	L[A] = (Row.Origin + Row.Dir * S)[A] - Box.GetCenter()[A];
	L[N] = (NSign > 0.f) ? Row.Origin[N] + DistanceFromRoad - Box.Min[N]
	                     : Row.Origin[N] - DistanceFromRoad - Box.Max[N];
	L.Z = GroundZ - Box.Min.Z - SinkDepth;

	OutTransform = FTransform(FRotator(0.f, Yaw, 0.f), L, FVector(Scale));
	return Box.ShiftBy(L);
}

bool AFBTownBuilder::IsBlocked(const FBox& LocalBox) const
{
	const FBox Shrunk = LocalBox.ExpandBy(-20.f); // touching is fine
	for (const FBox& B : Reserved)
	{
		if (B.Intersect(Shrunk))
		{
			return true;
		}
	}
	for (const FBox& B : Occupied)
	{
		if (B.Intersect(Shrunk))
		{
			return true;
		}
	}
	return false;
}

void AFBTownBuilder::FillRow(const FRow& Row, FRandomStream& Random)
{
	const float FrontLine = RoadWidth * 0.5f + SidewalkWidth;
	const float FacingYaw = FBTown::YawFacing(-Row.Normal); // front door looks at the road

	float S = Row.StartS;
	int32 Safety = 0;
	while (S < Row.EndS && ++Safety < 500)
	{
		// Empty / overgrown lot.
		if (Random.FRand() < EmptyLotChance)
		{
			S += Random.FRandRange(800.f, 1600.f);
			continue;
		}

		const FFBTownBuildingType* Type = PickBuilding(Row.bMainStreet, Random);
		if (!Type)
		{
			return; // No models assigned yet - streets only.
		}

		const float Yaw = FacingYaw + Type->FrontYaw;
		const float Distance = FrontLine + Setback + Random.FRandRange(0.f, SetbackJitter);

		// Measure the frontage first, then centre the building in its lot.
		FTransform T;
		FBox Box = PlaceMesh(Type->Mesh, Yaw, Type->Scale, Row, 0.f, Distance, 0.f, true, T);
		const int32 A = FBTown::AxisOf(Row.Dir);
		const float Frontage = Box.GetSize()[A];
		if (S + Frontage > Row.EndS)
		{
			break;
		}

		const float Center = S + Frontage * 0.5f;
		Box = PlaceMesh(Type->Mesh, Yaw, Type->Scale, Row, Center, Distance, 0.f, false, T);

		if (!IsBlocked(Box))
		{
			GetMeshISM(Type->Mesh)->AddInstance(T);
			Occupied.Add(Box);
		}
		S += Frontage + LotGap + Random.FRandRange(0.f, LotGapJitter);
	}
}

// ------------------------------------------------------------------ Props

void AFBTownBuilder::PlaceProps(const FRow& Row, FRandomStream& Random)
{
	const float HalfR = RoadWidth * 0.5f;

	for (const FFBTownPropType& Type : Props)
	{
		if (!Type.Mesh)
		{
			continue;
		}

		// Distance of the prop's road-facing edge from the road centre line, and ground height.
		float Distance = 0.f;
		float GroundZ = CurbHeight;
		float FaceYaw = FBTown::YawFacing(-Row.Normal);
		switch (Type.Spot)
		{
		case EFBTownSpot::Curb:         Distance = HalfR + 30.f; break;
		case EFBTownSpot::SidewalkBack: Distance = HalfR + SidewalkWidth - 90.f; break;
		case EFBTownSpot::ParkingLane:  Distance = HalfR - 260.f; GroundZ = 2.f;
		                                FaceYaw = FBTown::YawFacing(Row.Normal[FBTown::AxisOf(Row.Normal)] > 0.f ? Row.Dir : -Row.Dir); break;
		case EFBTownSpot::FrontYard:    Distance = HalfR + SidewalkWidth + 60.f; GroundZ = 0.f; break;
		case EFBTownSpot::BehindLots:   GroundZ = 0.f; break;
		}

		float S = Row.StartS + Random.FRandRange(0.f, Type.Spacing);
		int32 Safety = 0;
		while (S < Row.EndS && ++Safety < 1000)
		{
			const float Here = S;
			S += Type.Spacing * Random.FRandRange(0.7f, 1.3f);
			if (Random.FRand() > Type.Chance)
			{
				continue;
			}

			float D = Distance;
			if (Type.Spot == EFBTownSpot::BehindLots)
			{
				D = HalfR + SidewalkWidth + Setback + Random.FRandRange(1500.f, 3500.f);
			}

			const float Yaw = FaceYaw + Type.FrontYaw + Random.FRandRange(-Type.RandomYaw, Type.RandomYaw);
			const float Scale = Type.Scale * (1.f + Random.FRandRange(-Type.ScaleJitter, Type.ScaleJitter));

			FTransform T;
			const FBox Box = PlaceMesh(Type.Mesh, Yaw, Scale, Row, Here, D, GroundZ, false, T);

			// Street furniture may sit next to buildings, but never inside one.
			if (IsBlocked(Box))
			{
				continue;
			}
			GetMeshISM(Type.Mesh)->AddInstance(T);
			Occupied.Add(Box);

			if (Type.bLight)
			{
				AddLight(T.TransformPosition(Type.LightOffset), Type); // offset is in model units
			}
		}
	}
}

// ------------------------------------------------------------------ Helpers

UInstancedStaticMeshComponent* AFBTownBuilder::GetMeshISM(UStaticMesh* Mesh)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = MeshISMs.Find(Mesh))
	{
		return *Found;
	}

	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);
	ISM->CreationMethod = EComponentCreationMethod::UserConstructionScript; // rebuilt with the town
	ISM->SetMobility(EComponentMobility::Static);
	ISM->SetStaticMesh(Mesh);
	ISM->SetupAttachment(GetRootComponent());
	ISM->RegisterComponent();
	MeshISMs.Add(Mesh, ISM);
	return ISM;
}

void AFBTownBuilder::AddLight(const FVector& Location, const FFBTownPropType& Type)
{
	if (LightCount >= MaxLights)
	{
		return;
	}
	++LightCount;

	USpotLightComponent* Light = NewObject<USpotLightComponent>(this, NAME_None, RF_Transactional);
	Light->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Light->SetupAttachment(GetRootComponent());
	Light->SetRelativeLocation(Location);
	Light->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // straight down
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->SetIntensity(Type.LightIntensity);
	Light->SetLightColor(Type.LightColor);
	Light->SetInnerConeAngle(25.f);
	Light->SetOuterConeAngle(60.f);
	Light->SetAttenuationRadius(1600.f);
	Light->SetCastShadows(false); // many lamps: shadows off keeps the FPS up
	Light->RegisterComponent();
}
