// Fallen Brothers - Chandelier trap.

#include "Physics/FBChandelier.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBChandelier::AFBChandelier()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"));
	SetRootComponent(Anchor);

	// Body: wide, flat iron ring (placeholder cylinder 100 cm wide, 30 cm tall).
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Anchor);
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.3f));
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Body->SetSimulatePhysics(false); // Hangs until released.
	Body->SetGenerateOverlapEvents(true);

	// Chain: thin cylinder between ceiling and body. Attached to the body so it falls with it.
	Chain = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chain"));
	Chain->SetupAttachment(Body);
	Chain->SetStaticMesh(Cylinder.Object);
	Chain->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Chain->SetNotifyRigidBodyCollision(true);

	// Rope tie on a wall.
	RopeTie = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RopeTie"));
	RopeTie->SetupAttachment(Anchor);
	RopeTie->SetStaticMesh(Cube.Object);
	RopeTie->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.25f));
	RopeTie->SetRelativeLocation(FVector(250.f, 0.f, -260.f));
	RopeTie->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	if (BasicMaterial.Succeeded())
	{
		Body->SetMaterial(0, BasicMaterial.Object);
		Chain->SetMaterial(0, BasicMaterial.Object);
		RopeTie->SetMaterial(0, BasicMaterial.Object);
	}
}

void AFBChandelier::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bReleased)
	{
		return;
	}

	// Body hangs ChainLength below the ceiling (cylinder pivot is its centre, 30 cm tall).
	Body->SetRelativeLocation(FVector(0.f, 0.f, -ChainLength - 15.f));
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);

	// Chain goes from the top of the body up to the ceiling.
	// It is a child of the scaled body, so undo the body's scale for the chain.
	const FVector BodyScale = Body->GetRelativeScale3D();
	Chain->SetRelativeScale3D(FVector(0.03f, 0.03f, ChainLength / 100.f) / BodyScale);
	Chain->SetRelativeLocation(FVector(0.f, 0.f, (15.f + ChainLength * 0.5f) / BodyScale.Z));

	// Colours: dark iron body + chain, light rope tie.
	if (UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.13f, 0.1f));
	}
	if (UMaterialInstanceDynamic* MID = Chain->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.1f, 0.1f));
	}
	if (UMaterialInstanceDynamic* MID = RopeTie->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.5f, 0.3f));
	}
}

void AFBChandelier::BeginPlay()
{
	Super::BeginPlay();
	Chain->OnComponentHit.AddDynamic(this, &AFBChandelier::OnChainHit);
}

FText AFBChandelier::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	return NSLOCTEXT("FB", "ChandelierPrompt", "Release chandelier rope");
}

void AFBChandelier::OnChainHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bReleased || !OtherComp || !OtherComp->IsSimulatingPhysics())
	{
		return;
	}

	if (OtherComp->GetPhysicsLinearVelocity().Size() >= ChainBreakSpeed)
	{
		Release();
	}
}

void AFBChandelier::Release()
{
	if (bReleased)
	{
		return;
	}
	bReleased = true;

	// The chain snaps: it no longer blocks anything and falls with the body.
	Chain->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Body->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Body->SetSimulatePhysics(true);
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	Body->WakeAllRigidBodies();
}
