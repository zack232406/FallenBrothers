// Fallen Brothers - Base class for the town blockout kit.

#include "Town/FBBlockoutActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBBlockoutActor::AFBBlockoutActor()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	ConeMesh = Cone.Object;
	BasicMaterial = Material.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Static); // Buildings don't move (lights on them still can flicker).
}

UInstancedStaticMeshComponent* AFBBlockoutActor::MakeISM(FName Name, UStaticMesh* Mesh, bool bCollision)
{
	UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
	ISM->SetupAttachment(Root);
	ISM->SetStaticMesh(Mesh);
	if (BasicMaterial)
	{
		ISM->SetMaterial(0, BasicMaterial);
	}
	ISM->SetMobility(EComponentMobility::Static);
	if (bCollision)
	{
		ISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
	}
	return ISM;
}

void AFBBlockoutActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Components made by the previous rebuild (one per model) go away first.
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* C : Components)
	{
		if (C && C->CreationMethod == EComponentCreationMethod::UserConstructionScript)
		{
			C->DestroyComponent();
		}
	}
	DynamicISMs.Reset();

	TArray<UInstancedStaticMeshComponent*> ISMs;
	GetComponents<UInstancedStaticMeshComponent>(ISMs);
	for (UInstancedStaticMeshComponent* ISM : ISMs)
	{
		ISM->ClearInstances();
	}
	Rebuild();
	ApplyColors();
}

void AFBBlockoutActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyColors();
}

void AFBBlockoutActor::AddBox(UInstancedStaticMeshComponent* ISM, const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	if (ISM && Size.X > 0.5f && Size.Y > 0.5f && Size.Z > 0.5f)
	{
		ISM->AddInstance(FTransform(Rotation, Center, Size / 100.f)); // Engine cube = 100 cm
	}
}

void AFBBlockoutActor::AddRound(UInstancedStaticMeshComponent* ISM, const FVector& Center, float Diameter, float Height)
{
	if (ISM)
	{
		ISM->AddInstance(FTransform(FRotator::ZeroRotator, Center, FVector(Diameter, Diameter, Height) / 100.f));
	}
}

void AFBBlockoutActor::AddFenceLine(UInstancedStaticMeshComponent* Posts, UInstancedStaticMeshComponent* Rails,
                                    const FVector& Start, const FVector& End, float Height, float PostSpacing,
                                    float GateAlpha, float GateWidth)
{
	const FVector Delta = End - Start;
	const float Length = Delta.Size2D();
	if (Length < 1.f)
	{
		return;
	}
	const FVector Dir = Delta.GetSafeNormal2D();
	const FRotator Rot(0.f, Dir.Rotation().Yaw, 0.f);

	// Gate = a gap in the rails (and no posts inside it).
	const float GateCenter = GateAlpha >= 0.f ? GateAlpha * Length : -1.f;
	const float GateMin = GateCenter - GateWidth * 0.5f;
	const float GateMax = GateCenter + GateWidth * 0.5f;
	const bool bGate = GateAlpha >= 0.f && GateWidth > 0.f;

	// Posts
	const int32 Count = FMath::Max(1, FMath::RoundToInt(Length / FMath::Max(50.f, PostSpacing)));
	for (int32 i = 0; i <= Count; ++i)
	{
		const float S = Length * i / Count;
		if (bGate && S > GateMin && S < GateMax)
		{
			continue;
		}
		AddBox(Posts, Start + Dir * S + FVector(0.f, 0.f, Height * 0.5f), FVector(12.f, 12.f, Height), Rot);
	}
	if (bGate)
	{
		// Gate posts, a bit taller.
		AddBox(Posts, Start + Dir * GateMin + FVector(0.f, 0.f, Height * 0.6f), FVector(16.f, 16.f, Height * 1.2f), Rot);
		AddBox(Posts, Start + Dir * GateMax + FVector(0.f, 0.f, Height * 0.6f), FVector(16.f, 16.f, Height * 1.2f), Rot);
	}

	// Two rails, split around the gate.
	auto AddRails = [&](float From, float To)
	{
		if (To - From < 1.f)
		{
			return;
		}
		const FVector Mid = Start + Dir * ((From + To) * 0.5f);
		AddBox(Rails, Mid + FVector(0.f, 0.f, Height * 0.4f), FVector(To - From, 5.f, 8.f), Rot);
		AddBox(Rails, Mid + FVector(0.f, 0.f, Height * 0.85f), FVector(To - From, 5.f, 8.f), Rot);
	};
	if (bGate)
	{
		AddRails(0.f, FMath::Max(0.f, GateMin));
		AddRails(FMath::Min(Length, GateMax), Length);
	}
	else
	{
		AddRails(0.f, Length);
	}
}

void AFBBlockoutActor::SetColor(UInstancedStaticMeshComponent* ISM, const FLinearColor& Color)
{
	if (ISM)
	{
		if (UMaterialInstanceDynamic* MID = ISM->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

void AFBBlockoutActor::ApplyLook(UInstancedStaticMeshComponent* ISM, UMaterialInterface* Material, const FLinearColor& Color)
{
	if (!ISM)
	{
		return;
	}
	if (Material)
	{
		ISM->SetMaterial(0, Material);
		return;
	}
	// Back to the placeholder look (also undoes a material that was removed in Details).
	ISM->SetMaterial(0, BasicMaterial);
	SetColor(ISM, Color);
}

UInstancedStaticMeshComponent* AFBBlockoutActor::GetDynamicISM(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = DynamicISMs.Find(Mesh))
	{
		return *Found;
	}

	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);
	ISM->CreationMethod = EComponentCreationMethod::UserConstructionScript; // remade on every rebuild
	ISM->SetMobility(EComponentMobility::Static);
	ISM->SetStaticMesh(Mesh);
	ISM->SetupAttachment(GetRootComponent());
	ISM->RegisterComponent();
	DynamicISMs.Add(Mesh, ISM);
	return ISM;
}
