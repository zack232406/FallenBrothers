// Fallen Brothers - Base class for the town "blockout" kit.
//
// Blockout = grey-box level building: real sizes, real collision, real navmesh,
// placeholder looks. Every piece rebuilds itself live in the editor when you change
// a value in Details. Later we swap the looks for real Fab / Megascans models.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBBlockoutActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS(Abstract)
class FALLENBROTHERS_API AFBBlockoutActor : public AActor
{
	GENERATED_BODY()

public:
	AFBBlockoutActor();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	/** Derived classes add their boxes / cylinders here. Instances are already cleared. */
	virtual void Rebuild() {}

	/** Derived classes colour their parts here (called in the editor and again when the game starts). */
	virtual void ApplyColors() {}

	/** Create an instanced mesh component (call in the constructor). */
	UInstancedStaticMeshComponent* MakeISM(FName Name, UStaticMesh* Mesh, bool bCollision);

	/** Box from its centre and size in cm (local space). */
	static void AddBox(UInstancedStaticMeshComponent* ISM, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);

	/** Engine cylinder / cone: Diameter x Height, centre point. */
	static void AddRound(UInstancedStaticMeshComponent* ISM, const FVector& Center, float Diameter, float Height);

	/** Fence from Start to End: posts every PostSpacing + two rails. Optional gate gap (centre 0-1 along the line). */
	static void AddFenceLine(UInstancedStaticMeshComponent* Posts, UInstancedStaticMeshComponent* Rails,
	                         const FVector& Start, const FVector& End, float Height, float PostSpacing,
	                         float GateAlpha = -1.f, float GateWidth = 0.f);

	static void SetColor(UInstancedStaticMeshComponent* ISM, const FLinearColor& Color);

	/** Real material if one is chosen in Details, otherwise the plain placeholder colour. */
	void ApplyLook(UInstancedStaticMeshComponent* ISM, UMaterialInterface* Material, const FLinearColor& Color);

	/** Instanced component for one of YOUR models (Megaplants tree, house...). Made during Rebuild, remade every rebuild. */
	UInstancedStaticMeshComponent* GetDynamicISM(UStaticMesh* Mesh);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BasicMaterial;

private:
	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> DynamicISMs;
};
