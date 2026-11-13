// Fallen Brothers - Town blockout pieces.

#include "Town/FBTownProps.h"

#include "Environment/FBTerrain.h"

#include "Components/SkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"

// ------------------------------------------------------------------ Road

AFBRoad::AFBRoad()
{
	Asphalt = MakeISM(TEXT("Asphalt"), CubeMesh, true);
	Sidewalks = MakeISM(TEXT("Sidewalks"), CubeMesh, true);
	Paint = MakeISM(TEXT("Paint"), CubeMesh, false);
	WhitePaint = MakeISM(TEXT("WhitePaint"), CubeMesh, false);
}

void AFBRoad::Rebuild()
{
	// Asphalt top 2 cm above the actor (no flicker with a ground plane at 0).
	AddBox(Asphalt, FVector(Length * 0.5f, 0.f, -3.f), FVector(Length, RoadWidth, 10.f));

	if (SidewalkWidth > 0.f)
	{
		// 15 cm kerb - the player steps up it automatically.
		const float Y = RoadWidth * 0.5f + SidewalkWidth * 0.5f;
		AddBox(Sidewalks, FVector(Length * 0.5f, Y, 7.5f), FVector(Length, SidewalkWidth, 15.f));
		AddBox(Sidewalks, FVector(Length * 0.5f, -Y, 7.5f), FVector(Length, SidewalkWidth, 15.f));
	}

	if (!bRealMarkings)
	{
		if (bCenterLine)
		{
			for (float X = 150.f; X < Length - 150.f; X += 600.f)
			{
				AddBox(Paint, FVector(X + 150.f, 0.f, 2.5f), FVector(300.f, 12.f, 1.f));
			}
		}
		return;
	}

	// ---- Real markings (US style). Paint is 1 cm thick on top of the asphalt.
	const float Z = 2.5f;
	const float Half = RoadWidth * 0.5f;
	const float Lanes = FMath::Max(1, LanesPerDirection);
	const float TravelHalf = FMath::Min(Half - 20.f, Lanes * LaneWidth);
	const float CrossDepth = 320.f;            // zebra crosswalk length along the road
	const float StopGap = 120.f;               // stop line sits this far before the crosswalk
	const float X0 = bCrosswalkAtStart ? CrossDepth + StopGap + 60.f : 0.f;
	const float X1 = Length - (bCrosswalkAtEnd ? CrossDepth + StopGap + 60.f : 0.f);
	if (X1 <= X0 + 50.f)
	{
		return;
	}
	auto Solid = [&](UInstancedStaticMeshComponent* ISM, float Y, float W)
	{
		AddBox(ISM, FVector((X0 + X1) * 0.5f, Y, Z), FVector(X1 - X0, W, 1.f));
	};
	auto Dashed = [&](float Y, float Dash, float Gap)
	{
		for (float X = X0 + Gap * 0.5f; X + Dash < X1; X += Dash + Gap)
		{
			AddBox(WhitePaint, FVector(X + Dash * 0.5f, Y, Z), FVector(Dash, 12.f, 1.f));
		}
	};

	// Double yellow centre line (no passing in town).
	if (bCenterLine)
	{
		Solid(Paint, 14.f, 11.f);
		Solid(Paint, -14.f, 11.f);
	}
	// Lane lines between the travel lanes: 3 m dashes, 9 m gaps.
	for (int32 K = 1; K < LanesPerDirection; ++K)
	{
		Dashed(K * LaneWidth, 300.f, 900.f);
		Dashed(-K * LaneWidth, 300.f, 900.f);
	}
	// Edge line: between the travel lanes and the parking lane (or near the kerb).
	const float EdgeY = ParkingWidth > 0.f ? TravelHalf : Half - 35.f;
	Solid(WhitePaint, EdgeY, 12.f);
	Solid(WhitePaint, -EdgeY, 12.f);
	// Parking stalls: short ticks every 6.5 m in the parking lanes.
	if (ParkingWidth > 0.f)
	{
		for (float X = X0 + 650.f; X < X1 - 100.f; X += 650.f)
		{
			AddBox(WhitePaint, FVector(X, EdgeY + ParkingWidth * 0.25f, Z), FVector(10.f, ParkingWidth * 0.5f, 1.f));
			AddBox(WhitePaint, FVector(X, -EdgeY - ParkingWidth * 0.25f, Z), FVector(10.f, ParkingWidth * 0.5f, 1.f));
		}
	}

	// Crosswalks (zebra: 60 cm bars every 120 cm across the whole road) and stop lines.
	// Right-hand traffic (UE: +Y is to the right of +X): cars driving towards the start (-X) use the -Y half,
	// towards the end (+X) the +Y half.
	auto Crosswalk = [&](float XStart, float StopX, float StopSign)
	{
		for (float Y = -Half + 60.f; Y < Half - 30.f; Y += 120.f)
		{
			AddBox(WhitePaint, FVector(XStart + CrossDepth * 0.5f, Y + 30.f, Z), FVector(CrossDepth, 60.f, 1.f));
		}
		AddBox(WhitePaint, FVector(StopX, StopSign * TravelHalf * 0.5f, Z), FVector(45.f, TravelHalf, 1.f));
	};
	if (bCrosswalkAtStart)
	{
		Crosswalk(30.f, 30.f + CrossDepth + StopGap, -1.f);
	}
	if (bCrosswalkAtEnd)
	{
		Crosswalk(Length - 30.f - CrossDepth, Length - 30.f - CrossDepth - StopGap, 1.f);
	}
}

void AFBRoad::ApplyColors()
{
	ApplyLook(Asphalt, AsphaltMaterial, AsphaltColor);
	ApplyLook(Sidewalks, SidewalkMaterial, SidewalkColor);
	SetColor(Paint, FLinearColor(0.75f, 0.58f, 0.12f));
	SetColor(WhitePaint, FLinearColor(0.78f, 0.78f, 0.74f));
}

// ------------------------------------------------------------------ Intersection

AFBIntersection::AFBIntersection()
{
	Asphalt = MakeISM(TEXT("Asphalt"), CubeMesh, true);
	Corners = MakeISM(TEXT("Corners"), CubeMesh, true);
}

void AFBIntersection::Rebuild()
{
	AddBox(Asphalt, FVector(0.f, 0.f, -3.f), FVector(AsphaltSize.X, AsphaltSize.Y, 10.f));
	if (Sidewalk.X > 0.f && Sidewalk.Y > 0.f)
	{
		for (const float SX : { -1.f, 1.f })
		{
			for (const float SY : { -1.f, 1.f })
			{
				const FVector C(SX * (AsphaltSize.X + Sidewalk.X) * 0.5f, SY * (AsphaltSize.Y + Sidewalk.Y) * 0.5f, 7.5f);
				AddBox(Corners, C, FVector(Sidewalk.X, Sidewalk.Y, 15.f));
			}
		}
	}
}

void AFBIntersection::ApplyColors()
{
	ApplyLook(Asphalt, AsphaltMaterial, FLinearColor(0.06f, 0.06f, 0.065f));
	ApplyLook(Corners, SidewalkMaterial, FLinearColor(0.32f, 0.31f, 0.3f));
}

// ------------------------------------------------------------------ Fence

AFBFence::AFBFence()
{
	Posts = MakeISM(TEXT("Posts"), CubeMesh, true);
	Rails = MakeISM(TEXT("Rails"), CubeMesh, true);
}

void AFBFence::Rebuild()
{
	AddFenceLine(Posts, Rails, FVector::ZeroVector, FVector(Length, 0.f, 0.f), Height, PostSpacing, GatePosition, GateWidth);
}

void AFBFence::ApplyColors()
{
	ApplyLook(Posts, Material, Color);
	ApplyLook(Rails, Material, Color);
}

// ------------------------------------------------------------------ Cemetery

AFBCemetery::AFBCemetery()
{
	Ground = MakeISM(TEXT("Ground"), CubeMesh, true);
	Stones = MakeISM(TEXT("Stones"), CubeMesh, true);
	Posts = MakeISM(TEXT("Posts"), CubeMesh, true);
	Rails = MakeISM(TEXT("Rails"), CubeMesh, true);
}

void AFBCemetery::Rebuild()
{
	const float Margin = 300.f;
	const float SizeX = (Rows - 1) * RowSpacing + Margin * 2.f;
	const float SizeY = (GravesPerRow - 1) * GraveSpacing + Margin * 2.f;
	const float HalfX = SizeX * 0.5f;
	const float HalfY = SizeY * 0.5f;

	// Dark grass patch.
	AddBox(Ground, FVector(0.f, 0.f, -4.f), FVector(SizeX, SizeY, 10.f));

	// Headstones. A path down the middle stays empty (gate -> back).
	FRandomStream Random(Seed);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float X = HalfX - Margin - Row * RowSpacing;
		for (int32 i = 0; i < GravesPerRow; ++i)
		{
			const float Y = -HalfY + Margin + i * GraveSpacing;
			if (FMath::Abs(Y) < GraveSpacing * 0.6f)
			{
				continue; // centre path
			}
			if (Random.FRand() < 0.12f)
			{
				continue; // missing stone
			}

			const float H = Random.FRandRange(60.f, 120.f);
			const float W = Random.FRandRange(45.f, 70.f);
			const FRotator Tilt(Random.FRandRange(-8.f, 8.f), Random.FRandRange(-6.f, 6.f), Random.FRandRange(-6.f, 6.f));
			AddBox(Stones, FVector(X, Y, H * 0.5f - 5.f), FVector(14.f, W, H), Tilt);
		}
	}

	if (bFence)
	{
		const float FH = 140.f;
		const FVector FL(HalfX, -HalfY, 0.f), FR(HalfX, HalfY, 0.f), BL(-HalfX, -HalfY, 0.f), BR(-HalfX, HalfY, 0.f);
		AddFenceLine(Posts, Rails, FL, FR, FH, 200.f, 0.5f, 220.f); // front, with gate
		AddFenceLine(Posts, Rails, FR, BR, FH, 200.f);
		AddFenceLine(Posts, Rails, BR, BL, FH, 200.f);
		AddFenceLine(Posts, Rails, BL, FL, FH, 200.f);
	}
}

void AFBCemetery::ApplyColors()
{
	ApplyLook(Ground, GroundMaterial, GroundColor);
	ApplyLook(Stones, StoneMaterial, StoneColor);
	ApplyLook(Posts, FenceMaterial, FenceColor);
	ApplyLook(Rails, FenceMaterial, FenceColor);
}

// ------------------------------------------------------------------ Forest

AFBForest::AFBForest()
{
	Trunks = MakeISM(TEXT("Trunks"), CylinderMesh, true);
	Needles = MakeISM(TEXT("Needles"), ConeMesh, false);

	// Invisible trunks for real trees: they only block the player.
	TrunkBlockers = MakeISM(TEXT("TrunkBlockers"), CylinderMesh, true);
	TrunkBlockers->SetVisibility(false);
	TrunkBlockers->SetHiddenInGame(true);
	TrunkBlockers->SetCastShadow(false);
}

USkinnedMeshComponent* AFBForest::GetSkinnedTrees(USkeletalMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}
	if (TObjectPtr<USkinnedMeshComponent>* Found = SkinnedTrees.Find(Mesh))
	{
		return *Found;
	}

	// UInstancedSkinnedMeshComponent, found by name.
	static UClass* InstancedSkinnedClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.InstancedSkinnedMeshComponent"));
	if (!InstancedSkinnedClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("FBForest: InstancedSkinnedMeshComponent not found - Megaplants trees skipped."));
		return nullptr;
	}

	USkinnedMeshComponent* Comp = NewObject<USkinnedMeshComponent>(this, InstancedSkinnedClass, NAME_None, RF_Transactional);
	Comp->CreationMethod = EComponentCreationMethod::UserConstructionScript; // remade on every rebuild
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetSkinnedAssetAndUpdate(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // TrunkBlockers do the blocking
	Comp->SetCanEverAffectNavigation(false);
	Comp->RegisterComponent();
	SkinnedTrees.Add(Mesh, Comp);
	return Comp;
}

void AFBForest::AddSkinnedInstance(USkinnedMeshComponent* Comp, const FTransform& Transform)
{
	// Calls UInstancedSkinnedMeshComponent::AddInstance(Transform, AnimationIndex = 0, bWorldSpace = false).
	static UFunction* AddFunc = nullptr;
	if (!AddFunc)
	{
		AddFunc = Comp->GetClass()->FindFunctionByName(TEXT("AddInstance"));
		if (!AddFunc)
		{
			return;
		}
	}

	uint8* Params = static_cast<uint8*>(FMemory_Alloca_Aligned(AddFunc->ParmsSize, AddFunc->GetMinAlignment()));
	FMemory::Memzero(Params, AddFunc->ParmsSize);
	for (TFieldIterator<FProperty> It(AddFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params);
	}

	if (FStructProperty* P = FindFProperty<FStructProperty>(AddFunc, TEXT("InstanceTransform")))
	{
		*P->ContainerPtrToValuePtr<FTransform>(Params) = Transform;
	}
	if (FIntProperty* P = FindFProperty<FIntProperty>(AddFunc, TEXT("AnimationIndex")))
	{
		*P->ContainerPtrToValuePtr<int32>(Params) = 0; // rest pose (wind comes later)
	}
	if (FBoolProperty* P = FindFProperty<FBoolProperty>(AddFunc, TEXT("bWorldSpace")))
	{
		P->SetPropertyValue_InContainer(Params, false);
	}

	Comp->ProcessEvent(AddFunc, Params);

	for (TFieldIterator<FProperty> It(AddFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Params);
	}
}

void AFBForest::Rebuild()
{
	SkinnedTrees.Reset(); // the components themselves were destroyed by OnConstruction

	FRandomStream Random(Seed);

	// Real tree models (static and Megaplants skeletal), picked at random per tree.
	TArray<UStaticMesh*> ValidStatic;
	for (UStaticMesh* M : TreeMeshes)
	{
		if (M)
		{
			ValidStatic.Add(M);
		}
	}
	TArray<USkeletalMesh*> ValidSkeletal;
	for (USkeletalMesh* M : SkeletalTreeMeshes)
	{
		if (M)
		{
			ValidSkeletal.Add(M);
		}
	}
	const int32 RealCount = ValidStatic.Num() + ValidSkeletal.Num();
	TArray<FVector2D> Placed;
	Placed.Reserve(TreeCount);

	// Trees stand on the rolling ground (if the map has one) and never in the lake.
	const AFBTerrain* Terrain = AFBTerrain::Find(GetWorld());
	const FTransform ToWorld = GetActorTransform();

	const int32 MaxTries = TreeCount * 15;
	for (int32 Try = 0; Try < MaxTries && Placed.Num() < TreeCount; ++Try)
	{
		const FVector2D P(Random.FRandRange(-Size.X * 0.5f, Size.X * 0.5f), Random.FRandRange(-Size.Y * 0.5f, Size.Y * 0.5f));
		float GroundZ = 0.f;
		if (Terrain)
		{
			const FVector W = ToWorld.TransformPosition(FVector(P.X, P.Y, 0.f));
			if (Terrain->IsUnderWater(FVector2D(W.X, W.Y), 260.f)) // a clearing around the lake: no trees in or over the water
			{
				continue;
			}
			GroundZ = Terrain->GetActorLocation().Z + Terrain->HeightAt(FVector2D(W.X, W.Y)) - W.Z;
		}

		bool bTooClose = false;
		for (const FVector2D& Other : Placed)
		{
			if (FVector2D::DistSquared(P, Other) < MinDistance * MinDistance)
			{
				bTooClose = true;
				break;
			}
		}
		if (bTooClose)
		{
			continue;
		}
		Placed.Add(P);

		if (RealCount > 0)
		{
			const int32 Pick = Random.RandRange(0, RealCount - 1);
			const float S = Random.FRandRange(TreeScaleMin, FMath::Max(TreeScaleMin, TreeScaleMax));
			const FRotator Rot(Random.FRandRange(-2.f, 2.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-2.f, 2.f));
			const FTransform T(Rot, FVector(P.X, P.Y, GroundZ - 10.f), FVector(S));

			if (Pick < ValidStatic.Num())
			{
				GetDynamicISM(ValidStatic[Pick])->AddInstance(T);
			}
			else if (USkinnedMeshComponent* Comp = GetSkinnedTrees(ValidSkeletal[Pick - ValidStatic.Num()]))
			{
				AddSkinnedInstance(Comp, T);
			}

			const float D = TrunkCollisionDiameter * S;
			AddRound(TrunkBlockers, FVector(P.X, P.Y, GroundZ + 250.f), D, 500.f);
			continue;
		}

		const float H = Random.FRandRange(MinHeight, MaxHeight);
		const float TrunkD = FMath::Lerp(25.f, 55.f, (H - MinHeight) / FMath::Max(1.f, MaxHeight - MinHeight));
		AddRound(Trunks, FVector(P.X, P.Y, GroundZ + H * 0.5f), TrunkD, H);

		// Pine shape: two cones, starting about a third of the way up.
		const float CanopyD = H * Random.FRandRange(0.28f, 0.38f);
		AddRound(Needles, FVector(P.X, P.Y, GroundZ + H * 0.55f), CanopyD, H * 0.5f);
		AddRound(Needles, FVector(P.X, P.Y, GroundZ + H * 0.82f), CanopyD * 0.7f, H * 0.4f);
	}
}

void AFBForest::ApplyColors()
{
	ApplyLook(Trunks, TrunkMaterial, TrunkColor);
	ApplyLook(Needles, NeedleMaterial, NeedleColor);
}
