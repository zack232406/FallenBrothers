// Fallen Brothers - Physical creature (base for flesh-and-blood monsters).
//
// Unlike ghosts, creatures walk on the NAVMESH, open doors, and hunt by SIGHT and SOUND.
// Demo setup: "The Gaunt" - a tall starving thing that lives in the dark.
//
//   Wander      - prowls around its lair (where you placed it)
//   Investigate - heard something: goes to the sound, looks around
//   Chase       - saw you: runs you down and attacks
//   Flee        - your flashlight held on it too long: it recoils and runs
//   Stagger     - hit by something that isn't its weakness: knocked back, now furious
//   Dead        - only its weakness (silver) can kill it
//
// Stealth: crouch-walking is almost silent, sprinting is loud, a lit flashlight
// makes you easier to spot, throwing objects makes noise you can use as a distraction.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/FBDamageTypes.h"
#include "FBCreature.generated.h"

class AAIController;
class AFBFlashlight;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EFBCreatureState : uint8
{
	Wander,
	Investigate,
	Chase,
	Flee,
	Stagger,
	Dead
};

UCLASS()
class FALLENBROTHERS_API AFBCreature : public ACharacter, public IFBDamageable
{
	GENERATED_BODY()

public:
	AFBCreature();

	virtual void Tick(float DeltaSeconds) override;

	// IFBDamageable
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	UFUNCTION(BlueprintPure, Category = "Creature")
	EFBCreatureState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Creature")
	bool IsDead() const { return State == EFBCreatureState::Dead; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleNoise(const FVector& Location, float Loudness, AActor* NoiseInstigator);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;

	// ---------------------------------------------------------------- Identity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	FText CreatureName = NSLOCTEXT("FB", "GauntName", "The Gaunt");

	/** Only these can wound it. Everything else just staggers it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	TArray<EFBDamageKind> Weaknesses = { EFBDamageKind::Silver, EFBDamageKind::Fire };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	float MaxHealth = 100.f;

	// ---------------------------------------------------------------- Movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Movement")
	float WanderSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Movement")
	float InvestigateSpeed = 300.f;

	/** Player sprint is 520: you can just barely outrun it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Movement")
	float ChaseSpeed = 470.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Movement")
	float FleeSpeed = 560.f;

	/** How far from its lair it prowls (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Movement")
	float WanderRadius = 1500.f;

	/** Opens (unlocked) doors in its way. Locked doors stop it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Movement")
	bool bOpensDoors = true;

	// ---------------------------------------------------------------- Senses
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Senses")
	float SightRange = 2000.f;

	/** Half of its field of view (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Senses")
	float SightHalfAngle = 60.f;

	/** A loudness-1 noise is heard from this far (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Senses")
	float HearingRange = 2500.f;

	/** Seconds without seeing you before it gives up the chase and searches instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Senses")
	float LoseSightTime = 4.f;

	// ---------------------------------------------------------------- Attack
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Attack")
	float AttackRange = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Attack")
	float AttackDamage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Attack")
	float AttackCooldown = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Attack")
	float AttackKnockback = 600.f;

	// ---------------------------------------------------------------- Light
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Light")
	bool bAfraidOfLight = true;

	/** Seconds in your flashlight beam before it recoils. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Light")
	float LightTolerance = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Light")
	float FleeTime = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature|Light")
	float StaggerTime = 0.9f;

private:
	AAIController* GetAI() const;

	/** Who it's chasing: the player or the brother (whoever it saw). */
	APawn* GetChaseTarget(APawn* Player);
	void SetState(EFBCreatureState NewState);
	void OnEnterState(EFBCreatureState NewState);

	void TickWander();
	void TickInvestigate(float DeltaSeconds);
	void TickChase(float DeltaSeconds, APawn* Player);
	void TickFlee();
	void TickStagger();

	bool CanSeePlayer(APawn* Player) const;
	bool IsLitByPlayer(APawn* Player) const;
	static AFBFlashlight* GetChestLight(const APawn* Player);
	void TryAttack(APawn* Player);
	void OpenNearbyDoors();
	void Die(const FFBHitInfo& KillingHit);
	void Notify(const FText& Message) const;

	EFBCreatureState State = EFBCreatureState::Wander;
	float StateStartTime = 0.f;
	float Health = 100.f;
	FVector HomeLocation = FVector::ZeroVector;

	float NextSenseTime = 0.f;
	float LastSeenTime = -1000.f;
	FVector LastKnownPlayerLocation = FVector::ZeroVector;
	FVector InvestigateLocation = FVector::ZeroVector;
	float InvestigateArriveTime = -1.f;
	float WanderPauseUntil = 0.f;
	float LastAttackTime = -1000.f;
	float NextChaseRepath = 0.f;
	float LitTime = 0.f;
	float NextDoorCheck = 0.f;
	float LastNoEffectMessage = -100.f;

	bool bAnnouncedSpotted = false;
	bool bAnnouncedLight = false;

	FDelegateHandle NoiseHandle;
	TWeakObjectPtr<APawn> ChaseTarget;
};
