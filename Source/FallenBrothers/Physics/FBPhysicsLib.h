// Fallen Brothers - Small physics helpers shared by traps, breakables, poltergeist and weather zones.

#pragma once

#include "CoreMinimal.h"
#include "Combat/FBDamageTypes.h"

class AActor;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;

struct FALLENBROTHERS_API FFBPhysicsLib
{
	/** Hurts anything: creatures / Zack (IFBDamageable) or the player (health component). */
	static void DamageActor(AActor* Target, float Amount, EFBDamageKind Kind, AActor* Causer, const FVector& Where);

	/** True if the player is holding this component right now (grab / swing). */
	static bool IsHeldByPlayer(const UObject* WorldContext, const UPrimitiveComponent* Component);

	/** Engine basic shapes: "Cube", "Cylinder", "Sphere", "Cone", "Plane". */
	static UStaticMesh* BasicShape(const TCHAR* Name);

	/**
	 * Spawns a loose physics piece (debris, shard, fallen plank) owned by Owner.
	 * Size in cm (engine shapes are 100 cm). Pieces don't block pawns, so nobody gets stuck on them.
	 */
	static UStaticMeshComponent* MakePiece(AActor* Owner, UStaticMesh* Mesh, const FTransform& WorldTransform, const FVector& SizeCm,
	                                       UMaterialInterface* Material, const FLinearColor& Color, float MassKg, bool bSimulate);

	/** Placeholder colour on BasicShapeMaterial, or the real material if one is set. */
	static void ApplyLook(UStaticMeshComponent* Mesh, UMaterialInterface* Material, const FLinearColor& Color);

	/** Hits every character a fast-moving piece passes through (traps, debris). Returns true if it hit someone. */
	static bool HurtCharactersTouching(UPrimitiveComponent* Piece, float MinSpeed, float DamagePerKgMs, AActor* Causer, TSet<TWeakObjectPtr<AActor>>& AlreadyHit);
};
