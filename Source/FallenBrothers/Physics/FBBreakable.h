// Fallen Brothers - Breakable objects.
//
//   Window     - glass pane in a frame. Shatters when anything hits it (thrown crate, pipe swing, shotgun).
//   Barricade  - planks nailed across a doorway. Every good hit knocks ONE plank off (it cracks in two).
//                Creatures charging into it burst straight through.
//   Crate      - physics crate: grab it, throw it - it smashes into boards. Can drop an item (Spawn On Break).
//   Bottle     - physics bottle: shatters when it falls / is thrown.
//
// Front of the window / barricade = the actor's forward arrow. Set "Size" to fit your doorway or window.
// You can give it a real material (glass, wood...); otherwise placeholder colours are used.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/FBDamageTypes.h"
#include "FBBreakable.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EFBBreakableType : uint8
{
	Window,
	Barricade,
	Crate,
	Bottle
};

UCLASS()
class FALLENBROTHERS_API AFBBreakable : public AActor, public IFBDamageable
{
	GENERATED_BODY()

public:
	AFBBreakable();

	virtual void OnConstruction(const FTransform& Transform) override;

	// IFBDamageable (shotgun, knife, creatures...)
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	/** Breaks everything left, right now. */
	UFUNCTION(BlueprintCallable, Category = "Breakable")
	void BreakAll(const FVector& From, const FVector& Push);

	UFUNCTION(BlueprintPure, Category = "Breakable")
	bool IsBroken() const { return bBroken; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	EFBBreakableType Type = EFBBreakableType::Window;

	/** Width (Y) x thickness (X) x height (Z) in cm. 0 = default size for the type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	FVector Size = FVector::ZeroVector;

	/** Barricade: number of planks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable", meta = (ClampMin = "1", ClampMax = "8"))
	int32 Planks = 4;

	/** Speed (cm/s) something must hit it with to break it / knock a plank off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	float BreakSpeed = 300.f;

	/** Creatures running faster than this crash through it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	float CreatureBreakSpeed = 380.f;

	/** Seconds the debris stays (0 = forever). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	float DebrisLifetime = 60.f;

	/** Optional: something that falls out when it breaks (an item pickup, evidence...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	TSubclassOf<AActor> SpawnOnBreak;

	/** Glass / wood material. Empty = placeholder colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Look")
	TObjectPtr<UMaterialInterface> Material;

	/** Window frame material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Look")
	TObjectPtr<UMaterialInterface> FrameMaterial;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnPartHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

private:
	FVector GetSize() const;
	/** Makes the parts. Runs in the editor AND again when the game starts (play copies don't keep them). */
	void Build();
	bool IsLoose() const { return Type == EFBBreakableType::Crate || Type == EFBBreakableType::Bottle; }
	FLinearColor PartColor() const;

	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm, const FRotator& Rot, bool bBreakable, UMaterialInterface* Mat, const FLinearColor& Color);

	/** One part breaks (a plank, the pane, the crate...). */
	void BreakPart(UStaticMeshComponent* Part, const FVector& From, const FVector& Push);
	void Shatter(UStaticMeshComponent* Part, const FVector& From, const FVector& Push);
	void SplitPlank(UStaticMeshComponent* Part, const FVector& Push);
	void SmashCrate(UStaticMeshComponent* Part, const FVector& Push);
	void CheckAllBroken();

	void KeepDebris(UStaticMeshComponent* Piece);

	/** The parts that can break. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Debris;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	float CrateDamage = 0.f;
	bool bBroken = false;
};
