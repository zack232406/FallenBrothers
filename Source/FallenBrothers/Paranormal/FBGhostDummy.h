// Fallen Brothers - Test ghost.
//
// A simple apparition for testing weaknesses before the real creature AI (step 9).
//   - Normal objects pass straight through it.
//   - IRON (pipe, chandelier) moving fast through it -> it disperses for a few seconds.
//   - It is an EMF source, so the detector reacts and your flashlight flickers near it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/FBDamageTypes.h"
#include "FBGhostDummy.generated.h"

class UStaticMeshComponent;
class UFBParanormalSourceComponent;

UCLASS()
class FALLENBROTHERS_API AFBGhostDummy : public AActor, public IFBDamageable
{
	GENERATED_BODY()

public:
	AFBGhostDummy();

	virtual void Tick(float DeltaSeconds) override;

	// IFBDamageable
	virtual void ApplyHit(const FFBHitInfo& Hit) override;

	UFUNCTION(BlueprintPure, Category = "Ghost")
	bool IsDispersed() const { return bDispersed; }

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
	TObjectPtr<UFBParanormalSourceComponent> EMFSource;

	/** What hurts this ghost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	TArray<EFBDamageKind> Weaknesses = { EFBDamageKind::Iron, EFBDamageKind::Salt };

	/** Seconds the ghost stays gone after a weakness hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	float DisperseTime = 6.f;

	/** An object must move at least this fast (cm/s) through the ghost to count as a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	float MinImpactSpeed = 250.f;

	/** Gentle floating up and down (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	float FloatAmplitude = 6.f;

private:
	void Disperse();
	void Reform();
	void Notify(const FText& Message) const;

	FTimerHandle ReformTimer;
	bool bDispersed = false;
	float FloatTime = 0.f;
	float LastNoEffectMessageTime = -100.f;
};
