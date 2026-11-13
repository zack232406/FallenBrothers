// Fallen Brothers - Mountain range around a map.

#include "Environment/FBMountains.h"

#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Math/RandomStream.h"

namespace
{
	// UInstancedSkinnedMeshComponent (Nanite skinned instances, how Megaplants trees are drawn), found by name.
	USkinnedMeshComponent* MakeTreeComponent(AActor* Owner, USkeletalMesh* Mesh)
	{
		static UClass* Cls = FindObject<UClass>(nullptr, TEXT("/Script/Engine.InstancedSkinnedMeshComponent"));
		if (!Cls || !Mesh)
		{
			return nullptr;
		}
		USkinnedMeshComponent* Comp = NewObject<USkinnedMeshComponent>(Owner, Cls, NAME_None, RF_Transactional);
		Comp->CreationMethod = EComponentCreationMethod::UserConstructionScript; // remade on every rebuild
		Comp->SetupAttachment(Owner->GetRootComponent());
		Comp->SetSkinnedAssetAndUpdate(Mesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		Comp->RegisterComponent();
		return Comp;
	}

	void AddTreeInstance(USkinnedMeshComponent* Comp, const FTransform& Transform)
	{
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
			*P->ContainerPtrToValuePtr<int32>(Params) = 0;
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
}

AFBMountains::AFBMountains()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->bUseAsyncCooking = true;
	Mesh->bUseComplexAsSimpleCollision = true; // the real slopes are the collision
	Mesh->SetCastShadow(true);
}

void AFBMountains::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

float AFBMountains::Ridged(const FVector2D& P) const
{
	// Ridged multifractal: sharp crests and valleys like real eroded mountains.
	float Sum = 0.f;
	float Norm = 0.f;
	float Amp = 1.f;
	float Freq = 1.f;
	float Weight = 1.f;
	for (int32 Octave = 0; Octave < 6; ++Octave)
	{
		const FVector2D Offset(Seed * 17.13f + Octave * 31.7f, Seed * 9.71f - Octave * 13.3f);
		const float N = FMath::PerlinNoise2D(P * Freq + Offset); // about -1..1
		float R = 1.f - FMath::Abs(N);
		R *= R;
		R *= Weight;
		Weight = FMath::Clamp(R * 1.6f, 0.f, 1.f);

		const float Soft = N * 0.5f + 0.5f;
		Sum += FMath::Lerp(Soft, R, Sharpness) * Amp;
		Norm += Amp;
		Amp *= 0.5f;
		Freq *= 2.03f;
	}
	return FMath::Clamp(Sum / FMath::Max(Norm, 0.01f), 0.f, 1.f);
}

float AFBMountains::HeightAt(const FVector2D& World, float DepthAlpha) const
{
	const FVector2D P = (World - FVector2D(GetActorLocation().X, GetActorLocation().Y)) / NoiseScale;
	const float N = Ridged(P);
	const float Rise = FMath::SmoothStep(0.f, RiseDistance, DepthAlpha);
	const float Back = 1.f - 0.35f * FMath::SmoothStep(0.75f, 1.f, DepthAlpha); // far ridges a bit lower: depth
	const float Shape = 0.12f + 0.88f * FMath::Pow(N, 1.3f);
	return PeakHeight * Rise * Back * Shape - 30.f * (1.f - Rise); // inner rim sinks under the ground: no seam
}

void AFBMountains::Build()
{
	const int32 NA = FMath::Clamp(SegmentsAround, 32, 1024);
	const int32 NR = FMath::Clamp(SegmentsDeep, 8, 256);
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	const float OuterR = FMath::Max(InnerRadius.X, InnerRadius.Y) + OuterDistance;
	auto Index = [NR](int32 I, int32 J) { return I * (NR + 1) + J; };

	TArray<FVector> V;
	TArray<FVector2D> UV;
	V.SetNumUninitialized((NA + 1) * (NR + 1));
	UV.SetNumUninitialized(V.Num());

	for (int32 I = 0; I <= NA; ++I)
	{
		const float A = 2.f * PI * (I % NA) / NA;
		const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
		const FVector2D Inner(InnerRadius.X * Dir.X, InnerRadius.Y * Dir.Y);
		const FVector2D Outer = Dir * OuterR;
		for (int32 J = 0; J <= NR; ++J)
		{
			const float D = FMath::Pow(float(J) / NR, 1.7f); // more detail near the player
			const FVector2D Pos = FMath::Lerp(Inner, Outer, D);
			V[Index(I, J)] = FVector(Pos.X, Pos.Y, HeightAt(Center + Pos, D));
			UV[Index(I, J)] = Pos / 2000.f;
		}
	}

	// Smooth normals from the neighbours.
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	Normals.SetNumUninitialized(V.Num());
	Tangents.SetNumUninitialized(V.Num());
	for (int32 I = 0; I <= NA; ++I)
	{
		const int32 Prev = (I + NA - 1) % NA;
		const int32 Next = (I + 1) % NA;
		for (int32 J = 0; J <= NR; ++J)
		{
			const FVector Around = V[Index(Next, J)] - V[Index(Prev, J)];
			const FVector Depth = V[Index(I, FMath::Min(J + 1, NR))] - V[Index(I, FMath::Max(J - 1, 0))];
			FVector N = FVector::CrossProduct(Depth, Around).GetSafeNormal();
			if (N.Z < 0.f)
			{
				N = -N;
			}
			Normals[Index(I, J)] = N.IsNearlyZero() ? FVector::UpVector : N;
			Tangents[Index(I, J)] = FProcMeshTangent(Around.GetSafeNormal(), false);
		}
	}

	// Triangles, facing up. Flat-enough triangles above the snow line go to the snow section.
	TArray<int32> RockTris;
	TArray<int32> SnowTris;
	RockTris.Reserve(NA * NR * 6);
	const bool bSnow = SnowMaterial != nullptr;

	auto AddTri = [&](int32 A, int32 B, int32 C)
	{
		const FVector Cross = FVector::CrossProduct(V[B] - V[A], V[C] - V[A]);
		if (Cross.Z > 0.f) // UE is left-handed: same winding fix as the terrain
		{
			Swap(B, C);
		}
		bool bIsSnow = false;
		if (bSnow)
		{
			const FVector Mid = (V[A] + V[B] + V[C]) / 3.f;
			const float Ragged = FMath::PerlinNoise2D(FVector2D(Mid.X, Mid.Y) / 9000.f + FVector2D(Seed, -Seed)) * 3000.f;
			const float Flatness = FMath::Abs(Cross.GetSafeNormal().Z);
			bIsSnow = Mid.Z > SnowLine + Ragged && Flatness > 0.5f; // snow doesn't stick to cliffs
		}
		TArray<int32>& Out = bIsSnow ? SnowTris : RockTris;
		Out.Add(A);
		Out.Add(B);
		Out.Add(C);
	};

	for (int32 I = 0; I < NA; ++I)
	{
		for (int32 J = 0; J < NR; ++J)
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
	Mesh->CreateMeshSection_LinearColor(0, V, RockTris, Normals, UV, NoColors, Tangents, true);
	if (SnowTris.Num() > 0)
	{
		Mesh->CreateMeshSection_LinearColor(1, V, SnowTris, Normals, UV, NoColors, Tangents, true);
	}
	if (RockMaterial)
	{
		Mesh->SetMaterial(0, RockMaterial);
	}
	if (SnowMaterial)
	{
		Mesh->SetMaterial(1, SnowMaterial);
	}

	// Forests on the lower slopes: random points inside the mesh's own cells (so they sit on the real surface),
	// only where it's not too steep and below a ragged tree line.
	TArray<USkinnedMeshComponent*> TreeComps;
	for (USkeletalMesh* T : TreeMeshes)
	{
		if (USkinnedMeshComponent* Comp = MakeTreeComponent(this, T))
		{
			TreeComps.Add(Comp);
		}
	}
	if (TreeComps.Num() > 0 && TreeCount > 0)
	{
		FRandomStream R(Seed + 4242);
		const int32 MaxJ = FMath::Clamp(FMath::RoundToInt(NR * FMath::Pow(TreeDepth, 1.f / 1.7f)), 1, NR - 1);
		int32 Planted = 0;
		for (int32 Try = 0; Try < TreeCount * 4 && Planted < TreeCount; ++Try)
		{
			const int32 I = R.RandRange(0, NA - 1);
			const int32 J = R.RandRange(1, MaxJ);
			const float U = R.FRand();
			const float W = R.FRand();
			const FVector P = FMath::Lerp(FMath::Lerp(V[Index(I, J)], V[Index(I + 1, J)], U), FMath::Lerp(V[Index(I, J + 1)], V[Index(I + 1, J + 1)], U), W);
			const FVector N = Normals[Index(I, J)];
			const float Ragged = FMath::PerlinNoise2D(FVector2D(P.X, P.Y) / 7000.f + FVector2D(-Seed, Seed)) * 2500.f;
			if (P.Z < 40.f || P.Z > TreeLine + Ragged || N.Z < 0.7f)
			{
				continue;
			}
			// Clumps: skip some spots by noise so the forest has clearings, like real mountain woods.
			if (FMath::PerlinNoise2D(FVector2D(P.X, P.Y) / 4000.f) < -0.25f)
			{
				continue;
			}
			const float S = R.FRandRange(0.8f, 1.5f);
			const FTransform T(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), P - FVector(0.f, 0.f, 60.f), FVector(S));
			AddTreeInstance(TreeComps[R.RandRange(0, TreeComps.Num() - 1)], T);
			++Planted;
		}
		UE_LOG(LogTemp, Log, TEXT("FBMountains: %d trees on the slopes."), Planted);
	}
}
