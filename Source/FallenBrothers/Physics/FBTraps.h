// Fallen Brothers - Environmental physics traps.
//
//   FBSwingingLamp    - lamp hanging on a cable. Real pendulum: bump it, shoot it, the poltergeist or
//                       the wind pushes it - the light and the shadows swing with it.
//   FBCollapsingFloor - rotten floorboards. They creak when stepped on, then give way. Heavy creatures
//                       break them much faster (set Player Weight low to make it a creature trap).
//   FBDebrisTrap      - logs on a slope / rocks on a ledge / loose ceiling beams. Kick the stake (F) or
//                       shoot it to release them - they crush whatever is below (also creatures).
//   FBRopeBridge      - walkable bridge. Cut the ropes (F at either end) or shoot them: it falls apart.
//                       Can also snap by itself when a creature walks on it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/FBDamageTypes.h"
#include "Interaction/FBInteractable.h"
#include "FBTraps.generated.h"

class UMaterialInterface;
class UPhysicsConstraintComponent;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

// ====================================================================== Swinging lamp

UCLASS()
class FALLENBROTHERS_API AFBSwingingLamp : public AActor
{
	GENERATED_BODY()

public:
	AFBSwingingLamp();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Push the lamp (velocity change, cm/s). */
	UFUNCTION(BlueprintCallable, Category = "Lamp")
	void Push(const FVector& Velocity);

	/** Place the actor at the CEILING. Cable length down to the lamp (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp", meta = (ClampMin = "20"))
	float CableLength = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float Intensity = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	FLinearColor LightColor = FLinearColor(1.f, 0.78f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bOn = true;

	/** Old wiring: flickers now and then, more while swinging hard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bFlickers = true;

	/** Swings a little by itself (draught). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float IdleSway = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	TObjectPtr<UStaticMesh> LampMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	TObjectPtr<UMaterialInterface> LampMaterial;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Anchor;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Shade;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Cable;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPhysicsConstraintComponent> Joint;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USpotLightComponent> Light;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> Glow;

private:
	void UpdateCable();
	float NextIdlePush = 0.f;
	float FlickerUntil = 0.f;
	float NextFlicker = 0.f;
};

// ====================================================================== Collapsing floor

UCLASS()
class FALLENBROTHERS_API AFBCollapsingFloor : public AActor, public IFBDamageable
{
	GENERATED_BODY()

public:
	AFBCollapsingFloor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	// IFBDamageable - a shotgun blast into rotten boards brings them down.
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	UFUNCTION(BlueprintCallable, Category = "Floor")
	void Collapse(const FVector& Center);

	/** Floor size (cm). The actor is the TOP centre of the boards. Put it over a hole / above a basement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	FVector2D Size = FVector2D(300.f, 300.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	float BoardWidth = 22.f;

	/** Seconds of standing (at weight 1) before it gives way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	float CollapseTime = 1.6f;

	/** Chris and Zack. Set to 0.3 so they only make it creak - and only a creature breaks it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	float PlayerWeight = 1.f;

	/** Creatures (and anything bigger than a man). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	float CreatureWeight = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	TObjectPtr<UMaterialInterface> Material;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Boards;
	/** Makes the parts. Runs in the editor AND again when the game starts (play copies don't keep them). */
	void Build();

	struct FFall { TWeakObjectPtr<UStaticMeshComponent> Part; float Time; };
	TArray<FFall> FallQueue;

	float Strain = 0.f;
	bool bCreaked = false;
	bool bCollapsed = false;
	float NextCreak = 0.f;
};

// ====================================================================== Debris trap

UENUM(BlueprintType)
enum class EFBDebrisType : uint8
{
	Logs,   // stacked logs on a slope, held by two stakes - they roll forward (actor arrow)
	Rocks,  // boulders on a ledge, held by a plank
	Beams   // loose ceiling beams that drop straight down
};

UCLASS()
class FALLENBROTHERS_API AFBDebrisTrap : public AActor, public IFBInteractable, public IFBDamageable, public IFBImpactSource
{
	GENERATED_BODY()

public:
	AFBDebrisTrap();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Trap")
	void Release();

	// IFBInteractable - kick the stake
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return !bReleased && bPlayerCanRelease; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override { Release(); }

	// IFBDamageable - shoot the stake / support
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	// IFBImpactSource
	virtual EFBDamageKind GetImpactKind() const override { return EFBDamageKind::Blunt; }
	virtual float GetImpactMassKg() const override { return PieceMassKg; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	EFBDebrisType Type = EFBDebrisType::Logs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap", meta = (ClampMin = "1", ClampMax = "15"))
	int32 PieceCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	float PieceMassKg = 80.f;

	/** Damage per (kg x m/s). An 80 kg log at 6 m/s with 0.08 = 38 damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	float DamageScale = 0.08f;

	/** The player can release it with F (look at the stake). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	bool bPlayerCanRelease = true;

	/** Released by itself when a creature walks under it (0 = off). Measured from the "TriggerPoint". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	float CreatureTriggerRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	TObjectPtr<UStaticMesh> PieceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	TObjectPtr<UMaterialInterface> Material;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;

	/** Move this in the viewport: where creatures trigger it (usually the path below). */
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> TriggerPoint;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
	/** Makes the parts. Runs in the editor AND again when the game starts (play copies don't keep them). */
	void Build();
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Supports;

	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	float ReleaseTime = -1.f;
	bool bReleased = false;
};

// ====================================================================== Rope bridge

UCLASS()
class FALLENBROTHERS_API AFBRopeBridge : public AActor, public IFBInteractable, public IFBDamageable
{
	GENERATED_BODY()

public:
	AFBRopeBridge();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Bridge")
	void Snap(const FVector& Where);

	// IFBInteractable - cut the ropes at an end post
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return !bSnapped && bPlayerCanCut; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	// IFBDamageable - shoot the ropes
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	/** Bridge goes along the actor's forward arrow, starting at the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge", meta = (ClampMin = "200"))
	float Length = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	float Width = 140.f;

	/** How much the middle hangs down (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	float Sag = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	bool bPlayerCanCut = true;

	/** Snaps by itself when a creature is on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	bool bSnapsUnderCreature = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	TObjectPtr<UMaterialInterface> PlankMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	TObjectPtr<UMaterialInterface> RopeMaterial;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PlankParts;
	/** Makes the parts. Runs in the editor AND again when the game starts (play copies don't keep them). */
	void Build();
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> RopeParts;

	struct FPending { TWeakObjectPtr<UStaticMeshComponent> Part; float Time; };
	TArray<FPending> Falling;

	bool bSnapped = false;
	float SwayTime = 0.f;
};
