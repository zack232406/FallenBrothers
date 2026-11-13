// Fallen Brothers - Health component.
//
// Player health. Regenerates slowly after a few seconds without damage.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FBHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

UCLASS(ClassGroup = (FallenBrothers), meta = (BlueprintSpawnableComponent))
class FALLENBROTHERS_API UFBHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFBHealthComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Amount, AActor* Causer);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	/** Bring back from 0 health (e.g. helping a downed brother up). Percent 0-1. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Revive(float Percent);

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return Health <= 0.f; }

	/** Seconds since the last hit (for the HUD damage flash). */
	float GetTimeSinceDamage() const;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeath OnDeath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.f;

	/** Health per second once regeneration starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RegenRate = 4.f;

	/** Seconds without damage before regeneration starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RegenDelay = 6.f;

protected:
	virtual void BeginPlay() override;

private:
	float Health = 100.f;
	float LastDamageTime = -1000.f;
};
