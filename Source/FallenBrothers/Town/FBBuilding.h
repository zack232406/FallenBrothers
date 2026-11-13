// Fallen Brothers - Blockout building.
//
// A walk-in building with real door gaps, window holes, floors, roof, a sign and an inside light.
// Front of the building = the actor's forward arrow (+X). Doors are gaps 110 x 220 cm:
// drop an FBDoor into a gap if you want a real (lockable) door there.
//
// Ready-made versions for Harlow Falls: FBBuilding_Diner, _Motel, _House, _Library, _Mill, _Church, _Sheriff.

#pragma once

#include "CoreMinimal.h"
#include "Town/FBBlockoutActor.h"
#include "FBBuilding.generated.h"

class UInstancedStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EFBRoofType : uint8
{
	Flat,
	Pitched
};

UCLASS()
class FALLENBROTHERS_API AFBBuilding : public AFBBlockoutActor
{
	GENERATED_BODY()

public:
	AFBBuilding();

	// ---------------------------------------------------------------- Size (cm)
	/** Left-right size (along the front). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Size", meta = (ClampMin = "300"))
	float Width = 1200.f;

	/** Front-back size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Size", meta = (ClampMin = "300"))
	float Depth = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Size", meta = (ClampMin = "1", ClampMax = "6"))
	int32 Floors = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Size", meta = (ClampMin = "250"))
	float FloorHeight = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Size", meta = (ClampMin = "5"))
	float WallThickness = 20.f;

	// ---------------------------------------------------------------- Doors / windows
	/** Door gaps along the front (motel = many). 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Openings", meta = (ClampMin = "0", ClampMax = "12"))
	int32 FrontDoors = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Openings")
	bool bBackDoor = true;

	/** 110 fits an FBDoor (100 cm panel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Openings")
	float DoorWidth = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Openings")
	float DoorHeight = 220.f;

	/** Distance between windows. 0 = no windows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Openings")
	float WindowSpacing = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Openings")
	float WindowWidth = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Openings")
	float WindowHeight = 120.f;

	/** Height of the window's bottom edge above the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Openings")
	float WindowSill = 100.f;

	// ---------------------------------------------------------------- Roof
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Roof")
	EFBRoofType Roof = EFBRoofType::Flat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Roof", meta = (EditCondition = "Roof == EFBRoofType::Pitched"))
	float RoofRise = 250.f;

	// ---------------------------------------------------------------- Look
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	FLinearColor WallColor = FLinearColor(0.6f, 0.58f, 0.55f);

	/** Floors, ceiling slabs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	FLinearColor FloorColor = FLinearColor(0.3f, 0.25f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	FLinearColor RoofColor = FLinearColor(0.15f, 0.14f, 0.13f);

	/** Optional real materials (use M_FB_Surface instances - they tile, so nothing stretches). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	TObjectPtr<UMaterialInterface> WallMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	TObjectPtr<UMaterialInterface> RoofMaterial;

	/** Text above the front door. Empty = no sign. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	FText SignText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	FLinearColor SignColor = FLinearColor(1.f, 0.85f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Look")
	float SignSize = 60.f;

	// ---------------------------------------------------------------- Inside light
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Light")
	bool bInteriorLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Light")
	FLinearColor InteriorLightColor = FLinearColor(1.f, 0.78f, 0.5f);

	/** Candelas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Light")
	float InteriorLightIntensity = 25.f;

	// ---------------------------------------------------------------- Shell under a real model
	/** A real model stands over this blockout: walls, roof and sign stay for collision but are not drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Shell")
	bool bShellOnly = false;

	/** Local box the ground-floor slab is stretched to (the model's footprint). Zero size = keep the normal slab. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Shell")
	FVector2D ShellFloorCenter = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Shell")
	FVector2D ShellFloorSize = FVector2D::ZeroVector;

protected:
	virtual void Rebuild() override;
	virtual void ApplyColors() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Walls;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Slabs;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> RoofParts;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> Sign;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> InteriorLight;

private:
	struct FOpening
	{
		float Center = 0.f;  // along the wall, from the wall's middle
		float Width = 0.f;
		float Bottom = 0.f;  // from the floor
		float Top = 0.f;
	};

	/** One storey of one wall, with holes cut for doors / windows. */
	void AddWall(const FVector& Middle, const FVector& Dir, float Length, float Z0, float Height, TArray<FOpening> Openings);

	/** Windows evenly spread along a wall, skipping the doors. */
	void AddWindows(TArray<FOpening>& Openings, float Length) const;
};

// ---------------------------------------------------------------- Harlow Falls presets

UCLASS()
class FALLENBROTHERS_API AFBBuilding_Diner : public AFBBuilding
{
	GENERATED_BODY()
public:
	AFBBuilding_Diner();
};

UCLASS()
class FALLENBROTHERS_API AFBBuilding_Motel : public AFBBuilding
{
	GENERATED_BODY()
public:
	AFBBuilding_Motel();
};

UCLASS()
class FALLENBROTHERS_API AFBBuilding_House : public AFBBuilding
{
	GENERATED_BODY()
public:
	AFBBuilding_House();
};

UCLASS()
class FALLENBROTHERS_API AFBBuilding_Library : public AFBBuilding
{
	GENERATED_BODY()
public:
	AFBBuilding_Library();
};

UCLASS()
class FALLENBROTHERS_API AFBBuilding_Mill : public AFBBuilding
{
	GENERATED_BODY()
public:
	AFBBuilding_Mill();
};

UCLASS()
class FALLENBROTHERS_API AFBBuilding_Church : public AFBBuilding
{
	GENERATED_BODY()
public:
	AFBBuilding_Church();
};

UCLASS()
class FALLENBROTHERS_API AFBBuilding_Sheriff : public AFBBuilding
{
	GENERATED_BODY()
public:
	AFBBuilding_Sheriff();
};
