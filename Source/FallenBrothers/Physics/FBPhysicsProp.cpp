// Fallen Brothers - Physics prop.

#include "Physics/FBPhysicsProp.h"

#include "Core/FBNoiseSubsystem.h"
#include "Engine/World.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBPhysicsProp::AFBPhysicsProp()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;

	Mesh->SetStaticMesh(CubeMesh);
	if (BasicMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMaterial.Object);
	}

	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetGenerateOverlapEvents(true); // So ghosts can feel it pass through them.
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetUseCCD(true); // Fast thrown objects don't tunnel through walls.
	Mesh->SetCanEverAffectNavigation(false); // Moving props shouldn't punch holes in the navmesh.
}

void AFBPhysicsProp::BeginPlay()
{
	Super::BeginPlay();
	Mesh->OnComponentHit.AddDynamic(this, &AFBPhysicsProp::OnPropHit);
}

void AFBPhysicsProp::OnPropHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastNoiseTime < 0.3f)
	{
		return;
	}

	// Speed change caused by the impact (cm/s) -> how loud the crash is.
	const float DeltaV = NormalImpulse.Size() / FMath::Max(Mesh->GetMass(), 0.1f);
	if (DeltaV < 120.f)
	{
		return;
	}
	LastNoiseTime = Now;

	float Loudness = FMath::Clamp(DeltaV / 700.f, 0.15f, 0.8f);
	if (Material == EFBPropMaterial::Metal || Material == EFBPropMaterial::Iron || Material == EFBPropMaterial::Glass)
	{
		Loudness *= 1.4f; // Metal clangs, glass shatters.
	}
	UFBNoiseSubsystem::ReportNoise(this, Hit.ImpactPoint, Loudness, this);
}

void AFBPhysicsProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyShapeAndMaterial();
}

void AFBPhysicsProp::ApplyShapeAndMaterial()
{
	// Shape (sizes in cm, engine shapes are 100 cm).
	switch (Shape)
	{
	case EFBPropShape::Box:   Mesh->SetStaticMesh(CubeMesh);     Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));    break; // 50 cm crate
	case EFBPropShape::Pipe:  Mesh->SetStaticMesh(CylinderMesh); Mesh->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.9f));  break; // 4 cm x 90 cm
	case EFBPropShape::Ball:  Mesh->SetStaticMesh(SphereMesh);   Mesh->SetRelativeScale3D(FVector(0.3f));                break; // 30 cm
	case EFBPropShape::Plank: Mesh->SetStaticMesh(CubeMesh);     Mesh->SetRelativeScale3D(FVector(1.2f, 0.2f, 0.03f));   break; // 120 x 20 x 3
	}

	// Colour by material.
	FLinearColor Color(0.45f, 0.3f, 0.18f); // Wood
	switch (Material)
	{
	case EFBPropMaterial::Metal: Color = FLinearColor(0.55f, 0.57f, 0.6f); break;
	case EFBPropMaterial::Iron:  Color = FLinearColor(0.18f, 0.17f, 0.17f); break;
	case EFBPropMaterial::Glass: Color = FLinearColor(0.7f, 0.85f, 0.9f); break;
	case EFBPropMaterial::Stone: Color = FLinearColor(0.4f, 0.4f, 0.38f); break;
	default: break;
	}
	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}

	Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
}

EFBDamageKind AFBPhysicsProp::GetImpactKind() const
{
	return Material == EFBPropMaterial::Iron ? EFBDamageKind::Iron : EFBDamageKind::Blunt;
}

// ------------------------------------------------------------------ Iron pipe preset

AFBIronPipe::AFBIronPipe()
{
	DisplayName = NSLOCTEXT("FB", "PropIronPipe", "Iron pipe");
	Shape = EFBPropShape::Pipe;
	Material = EFBPropMaterial::Iron;
	MassKg = 2.5f;
	bIsMeleeWeapon = true;
}
