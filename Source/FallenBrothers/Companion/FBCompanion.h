// Fallen Brothers - AI brother (Zack).
//
// Follows the player on the navmesh, opens doors, fights with his own salt shotgun,
// comments on what happens (subtitles), and can be knocked down - help him up with F.
// Look at him + F (when he's up) to make him wait here / follow you again.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/FBDamageTypes.h"
#include "Interaction/FBInteractable.h"
#include "FBCompanion.generated.h"

class AAIController;
class UFBHealthComponent;
class UPointLightComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EFBCompanionState : uint8
{
	Follow,
	Hold,
	Downed
};

UCLASS()
class FALLENBROTHERS_API AFBCompanion : public ACharacter, public IFBInteractable, public IFBDamageable
{
	GENERATED_BODY()

public:
	AFBCompanion();

	virtual void Tick(float DeltaSeconds) override;

	// IFBInteractable
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override { return true; }
	virtual FText GetInteractionText_Implementation(AActor* InteractingActor) const override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;

	// IFBDamageable (only the player's shots end up here: friendly fire)
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	UFUNCTION(BlueprintPure, Category = "Companion")
	bool IsDowned() const { return State == EFBCompanionState::Downed; }

	UFUNCTION(BlueprintPure, Category = "Companion")
	EFBCompanionState GetState() const { return State; }

	/** Say one random line from the list (respects cooldowns). Returns true if he spoke. */
	bool Bark(const TArray<FText>& Lines, FName Key, float Cooldown, float Chance = 1.f);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleDowned();

	UFUNCTION()
	void HandlePlayerHealthChanged(float NewHealth, float Delta);

	UFUNCTION()
	void HandlePlayerJournalUpdated();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MuzzleFlash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFBHealthComponent> Health;

	// ---------------------------------------------------------------- Identity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion")
	FText CompanionName = NSLOCTEXT("FB", "ZackName", "Zack");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion")
	FLinearColor JacketColor = FLinearColor(0.16f, 0.2f, 0.12f);

	// ---------------------------------------------------------------- Follow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Follow")
	float FollowDistance = 250.f;

	/** Further than this he runs to catch up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Follow")
	float CatchUpDistance = 650.f;

	/** Further than this (stuck somewhere) he teleports behind you. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Follow")
	float TeleportDistance = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Follow")
	float WalkSpeed = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Follow")
	float RunSpeed = 540.f;

	// ---------------------------------------------------------------- Combat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float CombatRange = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float FireCooldown = 1.6f;

	/** Chance that a shot actually hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float HitChance = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float ShotDamage = 50.f;

	/** What his shells are loaded with. Salt = scatters ghosts, staggers creatures. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	EFBDamageKind AmmoKind = EFBDamageKind::Salt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float ReviveHealthPercent = 0.5f;

	// ---------------------------------------------------------------- Lines (edit freely)
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> IdleLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> GhostLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> CreatureLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> CombatLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> PlayerHurtLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> ClueLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> KillLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> FriendlyFireLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> HoldLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> FollowLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> DownedLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> RevivedLines;
	UPROPERTY(EditAnywhere, Category = "Companion|Lines") TArray<FText> CaseSolvedLines;

private:
	AAIController* GetAI() const;
	void BuildDefaultLines();

	void TickFollow(APawn* Player);
	AActor* FindCombatTarget(const APawn* Player) const;
	bool HasLineOfSight(const AActor* Target, const FVector& AimPoint) const;
	void ShootAt(AActor* Target);
	void OpenDoorsAhead();
	void CheckWorldForBarks(APawn* Player);
	void Revive();

	EFBCompanionState State = EFBCompanionState::Follow;

	TMap<FName, float> NextBarkTime;
	float LastLineTime = -100.f;
	float NextIdleChatter = 0.f;
	float NextFireTime = 0.f;
	float NextFollowRepath = 0.f;
	float NextDoorCheck = 0.f;
	float NextWorldCheck = 0.f;
	float FlashTimeLeft = 0.f;
	bool bAnnouncedCaseSolved = false;
	bool bBoundToPlayer = false;

	TSet<TWeakObjectPtr<AActor>> AnnouncedKills;

	FVector BodyDefaultLocation = FVector::ZeroVector;
	FRotator BodyDefaultRotation = FRotator::ZeroRotator;
	FVector HeadDefaultLocation = FVector::ZeroVector;
};
