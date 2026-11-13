// Fallen Brothers - Blockout building.

#include "Town/FBBuilding.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"

AFBBuilding::AFBBuilding()
{
	Walls = MakeISM(TEXT("Walls"), CubeMesh, true);
	Slabs = MakeISM(TEXT("Slabs"), CubeMesh, true);
	RoofParts = MakeISM(TEXT("RoofParts"), CubeMesh, true);

	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Root);
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteriorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	InteriorLight->SetupAttachment(Root);
	InteriorLight->IntensityUnits = ELightUnits::Candelas;
	InteriorLight->SetCastShadows(true);
}

// ------------------------------------------------------------------ Build

void AFBBuilding::AddWindows(TArray<FOpening>& Openings, float Length) const
{
	if (WindowSpacing <= 0.f || WindowWidth <= 0.f)
	{
		return;
	}

	const int32 Count = FMath::FloorToInt(Length / WindowSpacing);
	const TArray<FOpening> Doors = Openings;
	for (int32 i = 0; i < Count; ++i)
	{
		const float C = -Length * 0.5f + Length * (i + 0.5f) / Count;

		// Keep away from corners and doors.
		if (FMath::Abs(C) > Length * 0.5f - WindowWidth * 0.5f - 40.f)
		{
			continue;
		}
		bool bHitsDoor = false;
		for (const FOpening& D : Doors)
		{
			if (FMath::Abs(C - D.Center) < (WindowWidth + D.Width) * 0.5f + 40.f)
			{
				bHitsDoor = true;
				break;
			}
		}
		if (bHitsDoor)
		{
			continue;
		}

		FOpening W;
		W.Center = C;
		W.Width = WindowWidth;
		W.Bottom = WindowSill;
		W.Top = WindowSill + WindowHeight;
		Openings.Add(W);
	}
}

void AFBBuilding::AddWall(const FVector& Middle, const FVector& Dir, float Length, float Z0, float Height, TArray<FOpening> Openings)
{
	const FRotator Rot(0.f, Dir.Rotation().Yaw, 0.f);
	auto AddPiece = [&](float From, float To, float Bottom, float Top)
	{
		if (To - From < 1.f || Top - Bottom < 1.f)
		{
			return;
		}
		const FVector Center = Middle + Dir * ((From + To) * 0.5f) + FVector(0.f, 0.f, Z0 + (Bottom + Top) * 0.5f);
		AddBox(Walls, Center, FVector(To - From, WallThickness, Top - Bottom), Rot);
	};

	Openings.Sort([](const FOpening& A, const FOpening& B) { return A.Center < B.Center; });

	float Cursor = -Length * 0.5f;
	for (const FOpening& O : Openings)
	{
		const float Left = FMath::Clamp(O.Center - O.Width * 0.5f, -Length * 0.5f, Length * 0.5f);
		const float Right = FMath::Clamp(O.Center + O.Width * 0.5f, -Length * 0.5f, Length * 0.5f);
		if (Left < Cursor)
		{
			continue; // Overlapping openings - skip.
		}
		AddPiece(Cursor, Left, 0.f, Height);                             // solid wall before the hole
		AddPiece(Left, Right, 0.f, FMath::Min(O.Bottom, Height));        // under the window
		AddPiece(Left, Right, FMath::Min(O.Top, Height), Height);        // above the door / window
		Cursor = Right;
	}
	AddPiece(Cursor, Length * 0.5f, 0.f, Height);
}

void AFBBuilding::Rebuild()
{
	const float T = WallThickness;
	const float HalfD = Depth * 0.5f;
	const float HalfW = Width * 0.5f;
	const float TotalHeight = Floors * FloorHeight;

	for (int32 Floor = 0; Floor < Floors; ++Floor)
	{
		const float Z0 = Floor * FloorHeight;

		// Floor slab (ground floor slab sits just under the actor's origin).
		AddBox(Slabs, FVector(0.f, 0.f, Z0 - 10.f), FVector(Depth, Width, 20.f));

		// Front wall (+X), doors only on the ground floor.
		TArray<FOpening> Front;
		if (Floor == 0)
		{
			for (int32 i = 0; i < FrontDoors; ++i)
			{
				FOpening D;
				D.Center = -HalfW + Width * (i + 0.5f) / FrontDoors;
				D.Width = DoorWidth;
				D.Bottom = 0.f;
				D.Top = DoorHeight;
				Front.Add(D);
			}
		}
		AddWindows(Front, Width);
		AddWall(FVector(HalfD - T * 0.5f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), Width, Z0, FloorHeight, Front);

		// Back wall (-X).
		TArray<FOpening> Back;
		if (Floor == 0 && bBackDoor)
		{
			FOpening D;
			D.Center = 0.f;
			D.Width = DoorWidth;
			D.Top = DoorHeight;
			Back.Add(D);
		}
		AddWindows(Back, Width);
		AddWall(FVector(-HalfD + T * 0.5f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), Width, Z0, FloorHeight, Back);

		// Side walls fit between front and back.
		const float SideLength = Depth - T * 2.f;
		TArray<FOpening> Side;
		AddWindows(Side, SideLength);
		AddWall(FVector(0.f, HalfW - T * 0.5f, 0.f), FVector(1.f, 0.f, 0.f), SideLength, Z0, FloorHeight, Side);
		AddWall(FVector(0.f, -HalfW + T * 0.5f, 0.f), FVector(1.f, 0.f, 0.f), SideLength, Z0, FloorHeight, Side);
	}

	// Ceiling / flat roof.
	const float Overhang = 40.f;
	AddBox(RoofParts, FVector(0.f, 0.f, TotalHeight + 12.f), FVector(Depth + Overhang * 2.f, Width + Overhang * 2.f, 24.f));

	if (Roof == EFBRoofType::Pitched && RoofRise > 1.f)
	{
		// Two sloped slabs meeting at a ridge that runs left-right.
		const float Run = HalfD + Overhang;
		const float SlopeLength = FMath::Sqrt(Run * Run + RoofRise * RoofRise);
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(RoofRise, Run));
		const float MidZ = TotalHeight + 24.f + RoofRise * 0.5f;
		const FVector Size(SlopeLength, Width + Overhang * 2.f, 20.f);
		AddBox(RoofParts, FVector(Run * 0.5f, 0.f, MidZ), Size, FRotator(-Angle, 0.f, 0.f));
		AddBox(RoofParts, FVector(-Run * 0.5f, 0.f, MidZ), Size, FRotator(Angle, 0.f, 0.f));

		// Gable ends: stacked boxes, getting narrower towards the ridge.
		const int32 Steps = 6;
		for (int32 i = 0; i < Steps; ++i)
		{
			const float Alpha = (i + 0.5f) / Steps;
			const float StepDepth = Depth * (1.f - Alpha);
			const float Z = TotalHeight + 24.f + RoofRise * (i + 0.5f) / Steps;
			const FVector StepSize(StepDepth, T, RoofRise / Steps);
			AddBox(Walls, FVector(0.f, HalfW - T * 0.5f, Z), StepSize);
			AddBox(Walls, FVector(0.f, -HalfW + T * 0.5f, Z), StepSize);
		}
	}

	// Sign above the (first) front door.
	Sign->SetText(SignText);
	Sign->SetVisibility(!SignText.IsEmpty());
	Sign->SetWorldSize(SignSize);
	Sign->SetRelativeLocation(FVector(HalfD + 4.f, 0.f, FMath::Min(DoorHeight + 70.f, FloorHeight - 30.f)));

	// Light in the middle of the ground floor.
	InteriorLight->SetVisibility(bInteriorLight);
	InteriorLight->SetIntensity(InteriorLightIntensity);
	InteriorLight->SetLightColor(InteriorLightColor);
	InteriorLight->SetAttenuationRadius(FMath::Max(Width, Depth));
	InteriorLight->SetRelativeLocation(FVector(0.f, 0.f, FloorHeight - 60.f));

	// Shell under a real model: only the collision is left (rebuilt every construction, so it sticks).
	const bool bDraw = !bShellOnly;
	for (UPrimitiveComponent* C : { static_cast<UPrimitiveComponent*>(Walls), static_cast<UPrimitiveComponent*>(RoofParts) })
	{
		C->SetVisibility(bDraw);
		C->SetHiddenInGame(!bDraw);
		// Under a real model the model's own walls block (its exact shape); the grey walls would be invisible walls inside it.
		C->SetCollisionEnabled(bDraw ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (bShellOnly)
	{
		Sign->SetVisibility(false);
		if (ShellFloorSize.X > 1.f && ShellFloorSize.Y > 1.f && Slabs->GetInstanceCount() > 0)
		{
			const FTransform Floor(FQuat::Identity, FVector(ShellFloorCenter.X, ShellFloorCenter.Y, -10.f), FVector(ShellFloorSize.X / 100.f, ShellFloorSize.Y / 100.f, 20.f / 100.f));
			Slabs->UpdateInstanceTransform(0, Floor, false, true, true);
		}
	}
}

void AFBBuilding::ApplyColors()
{
	ApplyLook(Walls, WallMaterial, WallColor);
	ApplyLook(Slabs, FloorMaterial, FloorColor);
	ApplyLook(RoofParts, RoofMaterial, RoofColor);
	Sign->SetTextRenderColor(SignColor.ToFColor(true));
}

// ------------------------------------------------------------------ Presets

AFBBuilding_Diner::AFBBuilding_Diner()
{
	Width = 1600.f;
	Depth = 1000.f;
	FloorHeight = 380.f;
	WindowSpacing = 300.f;
	WindowWidth = 200.f;
	WindowHeight = 140.f;
	WindowSill = 90.f;
	WallColor = FLinearColor(0.78f, 0.74f, 0.64f);
	RoofColor = FLinearColor(0.45f, 0.07f, 0.06f);
	FloorColor = FLinearColor(0.35f, 0.33f, 0.32f);
	SignText = FText::FromString(TEXT("KELL'S DINER"));
	SignColor = FLinearColor(1.f, 0.25f, 0.2f);
	SignSize = 70.f;
	InteriorLightIntensity = 40.f;
}

AFBBuilding_Motel::AFBBuilding_Motel()
{
	Width = 3200.f;
	Depth = 900.f;
	FrontDoors = 6;
	bBackDoor = false;
	WindowSpacing = 260.f;
	WallColor = FLinearColor(0.55f, 0.45f, 0.35f);
	RoofColor = FLinearColor(0.18f, 0.28f, 0.22f);
	SignText = FText::FromString(TEXT("PINE REST MOTEL"));
	SignColor = FLinearColor(0.3f, 1.f, 0.6f);
	SignSize = 80.f;
	InteriorLightIntensity = 20.f;
}

AFBBuilding_House::AFBBuilding_House()
{
	Width = 1000.f;
	Depth = 900.f;
	Floors = 2;
	FloorHeight = 300.f;
	Roof = EFBRoofType::Pitched;
	RoofRise = 300.f;
	WindowSpacing = 280.f;
	WallColor = FLinearColor(0.68f, 0.68f, 0.64f);
	RoofColor = FLinearColor(0.12f, 0.1f, 0.09f);
	InteriorLightIntensity = 12.f;
}

AFBBuilding_Library::AFBBuilding_Library()
{
	Width = 1600.f;
	Depth = 1200.f;
	Floors = 2;
	FloorHeight = 380.f;
	WindowSpacing = 260.f;
	WindowHeight = 180.f;
	WallColor = FLinearColor(0.42f, 0.2f, 0.15f);
	RoofColor = FLinearColor(0.2f, 0.2f, 0.2f);
	SignText = FText::FromString(TEXT("HARLOW FALLS PUBLIC LIBRARY"));
	SignColor = FLinearColor(0.9f, 0.9f, 0.85f);
	SignSize = 45.f;
	InteriorLightIntensity = 30.f;
}

AFBBuilding_Mill::AFBBuilding_Mill()
{
	Width = 1400.f;
	Depth = 1800.f;
	Floors = 3;
	FloorHeight = 400.f;
	Roof = EFBRoofType::Pitched;
	RoofRise = 400.f;
	WindowSpacing = 450.f;
	WindowWidth = 100.f;
	WallColor = FLinearColor(0.25f, 0.19f, 0.14f);
	FloorColor = FLinearColor(0.2f, 0.16f, 0.12f);
	RoofColor = FLinearColor(0.1f, 0.09f, 0.08f);
	SignText = FText::FromString(TEXT("HARLOW MILL  EST. 1891"));
	SignColor = FLinearColor(0.6f, 0.55f, 0.45f);
	SignSize = 55.f;
	bInteriorLight = false; // Dark. Bring a flashlight.
}

AFBBuilding_Church::AFBBuilding_Church()
{
	Width = 1200.f;
	Depth = 2000.f;
	FloorHeight = 700.f;
	Roof = EFBRoofType::Pitched;
	RoofRise = 500.f;
	DoorWidth = 110.f;
	DoorHeight = 260.f;
	WindowSpacing = 400.f;
	WindowWidth = 100.f;
	WindowHeight = 320.f;
	WindowSill = 160.f;
	WallColor = FLinearColor(0.8f, 0.8f, 0.78f);
	RoofColor = FLinearColor(0.15f, 0.15f, 0.17f);
	SignText = FText::FromString(TEXT("ST. AGNES"));
	SignColor = FLinearColor(0.25f, 0.25f, 0.25f);
	InteriorLightColor = FLinearColor(1.f, 0.85f, 0.65f);
	InteriorLightIntensity = 15.f;
}

AFBBuilding_Sheriff::AFBBuilding_Sheriff()
{
	Width = 1300.f;
	Depth = 1000.f;
	WindowSpacing = 300.f;
	WallColor = FLinearColor(0.5f, 0.52f, 0.55f);
	RoofColor = FLinearColor(0.15f, 0.16f, 0.2f);
	SignText = FText::FromString(TEXT("SHERIFF"));
	SignColor = FLinearColor(0.85f, 0.75f, 0.3f);
	SignSize = 70.f;
	InteriorLightColor = FLinearColor(0.85f, 0.95f, 1.f);
	InteriorLightIntensity = 35.f;
}
