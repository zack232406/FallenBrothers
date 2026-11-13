// Fallen Brothers - Physics prop.
//
// A real physics object the player can grab, throw or (if melee) swing.
// Shape + material are chosen in the Details panel; placeholder engine meshes are used.
// AFBIronPipe below is a ready-made iron pipe preset.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/FBDamageTypes.h"
#include "FBPhysicsProp.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EFBPropShape : uint8
{
	Box,
	Pipe,
	Ball,
	Plank
};

UENUM(BlueprintType)
enum class EFBPropMaterial : uint8
{
	Wood,
	Metal,
	Iron,
	Glass,
	Stone
};

UCLASS()
class FALLENBROTHERS_API AFBPhysicsProp : public AActor, public IFBImpactSource
{
	GENERATED_BODY()

public:
	AFBPhysicsProp();

	virtual void OnConstruction(const FTransform& Transform) override;

	// IFBImpactSource
	virtual EFBDamageKind GetImpactKind() const override;
	virtual float GetImpactMassKg() const override { return MassKg; }

	UFUNCTION(BlueprintPure, Category = "Prop")
	bool IsMeleeWeapon() const { return bIsMeleeWeapon; }

	UFUNCTION(BlueprintPure, Category = "Prop")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Prop")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

protected:
	virtual void BeginPlay() override;

	/** Crashing into things makes noise creatures can hear. */
	UFUNCTION()
	void OnPropHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	FText DisplayName = NSLOCTEXT("FB", "PropCrate", "Wooden crate");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	EFBPropShape Shape = EFBPropShape::Box;

	/** Iron hurts ghosts! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	EFBPropMaterial Material = EFBPropMaterial::Wood;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop", meta = (ClampMin = "0.1"))
	float MassKg = 8.f;

	/** Left click swings it instead of throwing it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	bool bIsMeleeWeapon = false;

	// Placeholder meshes (engine basic shapes).
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

private:
	void ApplyShapeAndMaterial();

	float LastNoiseTime = -100.f;
};

/** Ready-made iron pipe: drag into the level, grab with right click, swing with left click. */
UCLASS()
class FALLENBROTHERS_API AFBIronPipe : public AFBPhysicsProp
{
	GENERATED_BODY()

public:
	AFBIronPipe();
};
