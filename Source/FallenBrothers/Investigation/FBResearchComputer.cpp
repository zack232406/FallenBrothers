// Fallen Brothers - Research computer.

#include "Investigation/FBResearchComputer.h"

#include "UI/FBHUD.h"
#include "Core/FBRealModels.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBResearchComputer::AFBResearchComputer()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Desk: 120 x 60 cm top at 75 cm height. Actor origin on the floor.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Desk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Desk"));
	Desk->SetupAttachment(Root);
	Desk->SetStaticMesh(Cube.Object);
	Desk->SetRelativeScale3D(FVector(0.6f, 1.2f, 0.75f));

	// Monitor sits on the desk (relative values are divided by the desk scale).
	Monitor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Monitor"));
	Monitor->SetupAttachment(Desk);
	Monitor->SetStaticMesh(Cube.Object);

	ScreenGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("ScreenGlow"));
	ScreenGlow->SetupAttachment(Desk);
	ScreenGlow->IntensityUnits = ELightUnits::Candelas;
	ScreenGlow->SetIntensity(15.f);
	ScreenGlow->SetLightColor(FLinearColor(0.6f, 0.85f, 1.f));
	ScreenGlow->SetAttenuationRadius(300.f);
	ScreenGlow->SetCastShadows(false);

	if (BasicMaterial.Succeeded())
	{
		Desk->SetMaterial(0, BasicMaterial.Object);
		Monitor->SetMaterial(0, BasicMaterial.Object);
	}
}

void AFBResearchComputer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The engine cube's pivot is in its centre, so lift the desk by half its height.
	Desk->SetRelativeLocation(FVector(0.f, 0.f, 37.5f));

	const FVector DeskScale = Desk->GetRelativeScale3D();
	// Monitor: 8 cm deep, 55 cm wide, 40 cm tall, standing on the desk.
	Monitor->SetRelativeScale3D(FVector(0.08f, 0.55f, 0.4f) / DeskScale);
	Monitor->SetRelativeLocation(FVector(10.f, 0.f, 37.5f + 22.f) / DeskScale);
	ScreenGlow->SetRelativeLocation(FVector(-30.f, 0.f, 37.5f + 22.f) / DeskScale);

	if (UMaterialInstanceDynamic* MID = Desk->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.17f, 0.1f));
	}
	if (UMaterialInstanceDynamic* MID = Monitor->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.05f, 0.05f));
	}
	Dress();
}

void AFBResearchComputer::Dress()
{
	// Earlier dressing goes first (construction can run many times in the editor).
	TInlineComponentArray<USceneComponent*> Old(this);
	for (USceneComponent* C : Old)
	{
		if (C && C->ComponentHasTag(TEXT("FBDress")))
		{
			C->DestroyComponent();
		}
	}

	// One imported model (all its parts share the model's space): bottom-centre at At (actor space), Height tall, turned by Yaw.
	auto Place = [this](const TCHAR* Name, float Height, const FVector& At, float Yaw, const FRotator& Fix = FRotator::ZeroRotator, bool bWhole = false) -> bool
	{
		const TArray<UStaticMesh*> Parts = FBRealModels::FindAll<UStaticMesh>(TEXT("Props"), Name);
		if (Parts.Num() == 0)
		{
			return false;
		}
		FBox MB(ForceInit);
		for (const UStaticMesh* P : Parts)
		{
			MB += P->GetBoundingBox();
		}
		// Centre on the biggest part (a long cable or a stray mouse must not pull the model off the desk).
		FBox Main(ForceInit);
		for (const UStaticMesh* P : Parts)
		{
			const FBox PB = P->GetBoundingBox();
			if (!Main.IsValid || PB.GetVolume() > Main.GetVolume())
			{
				Main = PB;
			}
		}
		if (bWhole)
		{
			Main = MB; // a set of separate things (monitor, tower, keyboard): place the whole set
		}
		// Fix: models exported lying down (Y-up) are stood up first.
		Main = Main.TransformBy(FTransform(Fix));
		const float S = Height / FMath::Max(1.f, float(Main.GetSize().Z));
		const FRotator Rot = (FQuat(FRotator(0.f, Yaw, 0.f)) * FQuat(Fix)).Rotator();
		const FVector Offset = -FRotator(0.f, Yaw, 0.f).RotateVector(FVector(Main.GetCenter().X, Main.GetCenter().Y, Main.Min.Z) * S);
		for (UStaticMesh* P : Parts)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transactional);
			C->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			C->ComponentTags.Add(TEXT("FBDress"));
			C->SetStaticMesh(P);
			C->SetupAttachment(Root);
			C->SetRelativeScale3D(FVector(S));
			C->SetRelativeRotation(Rot);
			C->SetRelativeLocation(At + Offset);
			C->SetCollisionProfileName(TEXT("BlockAll"));
			C->RegisterComponent();
		}
		return true;
	};

	// The desk's drawers (its front) face +X: the chair, the screen and the player are on that side.
	const float DeskTop = 78.f;
	const float CrtYaw = 0.f;
	if (!Place(TEXT("prop_antique_desk"), DeskTop, FVector::ZeroVector, ComputerYaw))
	{
		return; // not imported: keep the placeholder desk + monitor
	}
	// The computer: a cream 90s PC (monitor, tower, keyboard, mouse); the older retro model as a fallback.
	if (!Place(TEXT("prop_crt_pc"), 46.f, FVector(10.f, 0.f, DeskTop), ComputerYaw + CrtYaw, FRotator::ZeroRotator, true))
	{
		Place(TEXT("prop_retro_computer"), 44.f, FVector(8.f, 0.f, DeskTop), ComputerYaw - 90.f, FRotator(0.f, 0.f, -90.f)); // exported Y-up
	}
	if (Place(TEXT("prop_desk_lamp"), 42.f, FVector(12.f, -48.f, DeskTop), ComputerYaw + 120.f))
	{
		// The lamp is on: warm, soft, casts shadows over the desk.
		UPointLightComponent* Bulb = NewObject<UPointLightComponent>(this, NAME_None, RF_Transactional);
		Bulb->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Bulb->ComponentTags.Add(TEXT("FBDress"));
		Bulb->SetupAttachment(Root);
		Bulb->SetRelativeLocation(FVector(4.f, -40.f, DeskTop + 30.f));
		Bulb->IntensityUnits = ELightUnits::Candelas;
		Bulb->SetIntensity(9.f);
		Bulb->SetUseTemperature(true);
		Bulb->SetTemperature(2700.f);
		Bulb->SetAttenuationRadius(420.f);
		Bulb->SetSourceRadius(3.f);
		Bulb->SetCastShadows(true);
		Bulb->RegisterComponent();
	}
	Place(TEXT("prop_office_chair"), 92.f, FVector(80.f, 0.f, 0.f), ComputerYaw + 90.f); // in front of the drawers (+X), facing the desk

	// The placeholder shapes stay only as the (invisible) thing you aim at.
	Desk->SetVisibility(false, true);
	ScreenGlow->SetVisibility(true);
	ScreenGlow->SetRelativeLocation(FVector(-25.f, 0.f, DeskTop - 37.5f + 24.f) / Desk->GetRelativeScale3D());
}

FText AFBResearchComputer::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	return NSLOCTEXT("FB", "UseComputer", "Use computer");
}

void AFBResearchComputer::Interact_Implementation(AActor* InteractingActor)
{
	const APawn* Pawn = Cast<APawn>(InteractingActor);
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (AFBHUD* HUD = PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->OpenComputer(this);
	}
}
