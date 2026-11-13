// Fallen Brothers - Street lamp.

#include "Town/FBStreetLamp.h"

#include "Paranormal/FBParanormalSubsystem.h"
#include "Town/FBAtmosphere.h"
#include "Core/FBRealModels.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBStreetLamp::AFBStreetLamp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 5 m pole, 1.6 m arm towards the road.
	Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
	Pole->SetupAttachment(Root);
	Pole->SetStaticMesh(Cylinder.Object);
	Pole->SetRelativeLocation(FVector(0.f, 0.f, 250.f));
	Pole->SetRelativeScale3D(FVector(0.15f, 0.15f, 5.f));

	Arm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->SetStaticMesh(Cube.Object);
	Arm->SetRelativeLocation(FVector(80.f, 0.f, 490.f));
	Arm->SetRelativeScale3D(FVector(1.6f, 0.08f, 0.08f));
	Arm->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetStaticMesh(Cube.Object);
	Head->SetRelativeLocation(FVector(150.f, 0.f, 478.f));
	Head->SetRelativeScale3D(FVector(0.5f, 0.3f, 0.12f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetCastShadow(false);

	if (Material.Succeeded())
	{
		Pole->SetMaterial(0, Material.Object);
		Arm->SetMaterial(0, Material.Object);
		Head->SetMaterial(0, Material.Object);
	}

	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(150.f, 0.f, 465.f));
	Light->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // Straight down
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->SetInnerConeAngle(25.f);
	Light->SetOuterConeAngle(60.f);
	Light->SetAttenuationRadius(2400.f);
	Light->SetSourceRadius(12.f);
	Light->SetUseTemperature(false);
	Light->SetCastShadows(true);
	Light->SetVolumetricScatteringIntensity(0.2f); // a soft cone in the fog, not a glowing blob
}

void AFBStreetLamp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AFBStreetLamp::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
	NextFaultTime = FMath::FRandRange(3.f, 12.f);
}

void AFBStreetLamp::ApplyLook()
{
	auto DressWithModel = [this](FVector& OutLightAt) -> bool
	{
		// The real street light (if imported): pole at the origin, the head reaching out over the road (+X).
		const TArray<UStaticMesh*> Parts = FBRealModels::FindAll<UStaticMesh>(TEXT("Props"), TEXT("lamp_street"));
		if (Parts.Num() == 0)
		{
			return false;
		}
		FBox Raw(ForceInit);
		for (const UStaticMesh* P : Parts)
		{
			Raw += P->GetBoundingBox();
		}
		// The arm points to whichever side reaches further from the pole (the model's origin).
		float Yaw = 0.f;
		if (Raw.GetSize().Y > Raw.GetSize().X)
		{
			Yaw = FMath::Abs(Raw.Min.Y) > FMath::Abs(Raw.Max.Y) ? 90.f : -90.f;
		}
		else
		{
			Yaw = FMath::Abs(Raw.Min.X) > FMath::Abs(Raw.Max.X) ? 180.f : 0.f;
		}
		const FRotator Turn(0.f, Yaw, 0.f);
		const FBox Turned = Raw.TransformBy(FTransform(Turn));
		const float Height = LightHeight + 60.f;
		const float S = Height / FMath::Max(1.f, Turned.GetSize().Z);
		const float PoleX = Turned.Min.X + Turned.GetSize().X * 0.05f;
		const float HeadX = Turned.Max.X - Turned.GetSize().X * 0.08f;

		bool bHave = false;
		TInlineComponentArray<UStaticMeshComponent*> Comps(this);
		for (UStaticMeshComponent* C : Comps)
		{
			bHave |= C && C->ComponentHasTag(TEXT("FBReal"));
		}
		if (!bHave)
		{
			// AttachModel centres the model's box on Offset: shift it so the pole stands on the origin.
			const FVector Offset((Turned.GetCenter().X - PoleX) * S, 0.f, 0.f);
			if (!FBRealModels::AttachModel(this, Root, TEXT("Props"), TEXT("lamp_street"), Height, Turn, Offset, true, true))
			{
				return false;
			}
		}
		OutLightAt = FVector((HeadX - PoleX) * S, 0.f, Height * 0.93f);
		return true;
	};
	const bool bLit = bOn && bLitByNight;
	FVector ModelLight = FVector::ZeroVector;
	const bool bModel = bShowBody && DressWithModel(ModelLight);
	Light->SetRelativeLocation(bModel ? ModelLight : FVector(ArmReach, 0.f, LightHeight));
	Light->SetLightColor(LightColor);
	Light->SetIntensity(bLit ? Intensity : 0.f);
	Light->SetVisibility(bLit);

	Pole->SetVisibility(bShowBody && !bModel); // with the real model the grey pole only keeps its collision
	Arm->SetVisibility(bShowBody && !bModel);
	Head->SetVisibility(bShowBody && !bModel);
	Pole->SetCollisionEnabled(bShowBody ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Pole->SetRelativeLocation(FVector(0.f, 0.f, LightHeight * 0.54f));
	Pole->SetRelativeScale3D(FVector(0.15f, 0.15f, LightHeight * 0.0108f));
	Arm->SetRelativeLocation(FVector(ArmReach * 0.53f, 0.f, LightHeight + 25.f));
	Arm->SetRelativeScale3D(FVector(ArmReach * 0.0107f, 0.08f, 0.08f));
	Head->SetRelativeLocation(FVector(ArmReach, 0.f, LightHeight + 13.f));

	auto Paint = [](UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	};
	Paint(Pole, FLinearColor(0.1f, 0.1f, 0.1f));
	Paint(Arm, FLinearColor(0.1f, 0.1f, 0.1f));
	Paint(Head, bLit ? LightColor : FLinearColor(0.15f, 0.15f, 0.15f));
}

void AFBStreetLamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Dusk / dawn: photocell switch (checked twice a second, each lamp a little different, like real ones).
	NightCheckTimer -= DeltaSeconds;
	if (bAutoNight && NightCheckTimer <= 0.f)
	{
		NightCheckTimer = 0.5f;
		const float Darkness = AFBAtmosphere::DarknessIn(GetWorld());
		const float Threshold = 0.3f + 0.15f * FMath::Frac(GetActorLocation().X * 0.0137f + GetActorLocation().Y * 0.0071f);
		const bool bShouldLight = Darkness > Threshold;
		if (bShouldLight != bLitByNight)
		{
			bLitByNight = bShouldLight;
			ApplyLook();
		}
	}

	if (!bOn || !bLitByNight)
	{
		return;
	}

	// Something paranormal nearby?
	float EMF = 0.f;
	if (const UFBParanormalSubsystem* Paranormal = GetWorld()->GetSubsystem<UFBParanormalSubsystem>())
	{
		EMF = Paranormal->GetEMFLevelAt(Light->GetComponentLocation() - FVector(0.f, 0.f, 400.f));
	}

	// Faulty bulb: short random flicker bursts.
	const float Now = GetWorld()->GetTimeSeconds();
	if (bFaulty && Now > NextFaultTime)
	{
		FaultEndTime = Now + FMath::FRandRange(0.3f, 1.2f);
		NextFaultTime = Now + FMath::FRandRange(4.f, 15.f);
	}
	const bool bFaultFlicker = bFaulty && Now < FaultEndTime;

	float Strength = 0.f;
	if (EMF > EMFFlickerThreshold)
	{
		Strength = FMath::Clamp((EMF - EMFFlickerThreshold) / 2.f, 0.3f, 1.f);
	}
	else if (bFaultFlicker)
	{
		Strength = 0.6f;
	}

	float Multiplier = 1.f;
	if (Strength > 0.f && FMath::FRand() < 0.35f + Strength * 0.4f)
	{
		Multiplier = FMath::FRandRange(0.f, 1.f - Strength * 0.8f);
	}
	Light->SetIntensity(Intensity * Multiplier);
}
