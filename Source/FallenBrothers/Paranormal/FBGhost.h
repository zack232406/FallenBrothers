// Fallen Brothers - Ghost (vengeful spirit).
//
// Behaviour:
//   Dormant     - invisible, haunting its area. Every so often it MANIFESTS near the player.
//   Manifested  - visible (flickering), glides toward the player through walls.
//                 Can't cross salt lines. If you stare at it, it fades away.
//                 If it reaches you: you take damage and get thrown back, then it vanishes.
//   Dispersed   - hit by its weakness (iron / salt): gone for a while, then it comes back.
//   Banished    - its remains were salted and burned (AFBGraveSite). Gone for good.
// It also throws nearby physics props at the player (poltergeist) and makes lights flicker.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/FBDamageTypes.h"
#include "FBGhost.generated.h"

class UStaticMeshComponent;
class UFBParanormalSourceComponent;

UENUM(BlueprintType)
enum class EFBGhostState : uint8
{
	Dormant,
	Manifested,
	Dispersed,
	Banished
};

UCLASS()
class FALLENBROTHERS_API AFBGhost : public AActor, public IFBDamageable
{
	GENERATED_BODY()

public:
	AFBGhost();

	virtual void Tick(float DeltaSeconds) override;

	// IFBDamageable
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	/** The remains were destroyed: gone forever. */
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void Banish();

	/** Disturbing its grave makes it furious: manifests more often and moves faster. */
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void Enrage();

	UFUNCTION(BlueprintPure, Category = "Ghost")
	EFBGhostState GetState() const { return State; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBodyOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                   int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBParanormalSourceComponent> EMFSource;

	// ---------------------------------------------------------------- Identity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	TArray<EFBDamageKind> Weaknesses = { EFBDamageKind::Iron, EFBDamageKind::Salt };

	// ---------------------------------------------------------------- Haunting
	/** It only manifests while the player is within this distance of its haunt (where you placed it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Haunting")
	float ActivationRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Haunting")
	float ManifestIntervalMin = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Haunting")
	float ManifestIntervalMax = 22.f;

	/** Distance from the player where it appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Haunting")
	float ManifestDistanceMin = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Haunting")
	float ManifestDistanceMax = 850.f;

	/** Max seconds it stays manifested before fading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Haunting")
	float MaxManifestTime = 14.f;

	// ---------------------------------------------------------------- Attack
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attack")
	float GlideSpeed = 140.f;

	/**
	 * false = drifts straight through walls (classic apparition).
	 * true  = walks the corridors on the navmesh and opens doors - you hear it coming
	 *         but never know from where. Needs a Nav Mesh Bounds Volume in the level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attack")
	bool bWalksAroundWalls = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attack")
	float AttackRange = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attack")
	float AttackDamage = 25.f;

	/** Push applied to the player when hit (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attack")
	float AttackKnockback = 700.f;

	/** Staring at it this long makes it fade away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attack")
	float StareVanishTime = 2.5f;

	// ---------------------------------------------------------------- Weakness
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Weakness")
	float DisperseTime = 8.f;

	/** Objects must move at least this fast through it to count (iron pipe swing, falling chandelier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Weakness")
	float MinImpactSpeed = 250.f;

	// ---------------------------------------------------------------- Poltergeist
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Poltergeist")
	bool bThrowsObjects = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Poltergeist")
	float ThrowIntervalMin = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Poltergeist")
	float ThrowIntervalMax = 16.f;

	/** How far from the ghost (or its haunt) it can grab objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Poltergeist")
	float ThrowGrabRange = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Poltergeist")
	float ThrowSpeed = 950.f;

private:
	void SetState(EFBGhostState NewState);
	void SetVisible(bool bVisible);
	void Manifest();
	void Vanish(float MinDelay);
	void TickManifested(float DeltaSeconds, APawn* Player);
	void TryPoltergeist(APawn* Player);
	void AttackPlayer(APawn* Player);
	void ScheduleNextManifest(float MinDelay = 0.f);
	void FlickerPlayerLights(APawn* Player, float Amount, float Duration) const;
	void Notify(const FText& Message) const;

	EFBGhostState State = EFBGhostState::Dormant;
	FVector HomeLocation = FVector::ZeroVector;
	float StateStartTime = 0.f;
	float NextManifestTime = 0.f;
	float NextThrowTime = 0.f;
	float StareTime = 0.f;
	float BanishTime = 0.f;
	float LastNoEffectMessage = -100.f;
	float LastSaltMessage = -100.f;
	float SpeedMultiplier = 1.f;
	float IntervalMultiplier = 1.f;
	bool bFirstManifest = true;

	// Navmesh walking (bWalksAroundWalls)
	FVector GetMoveTarget(const FVector& MyLoc, const FVector& PlayerFeet);
	void OpenDoorsAhead(const FVector& MoveDir);
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	float NextPathTime = 0.f;
};
