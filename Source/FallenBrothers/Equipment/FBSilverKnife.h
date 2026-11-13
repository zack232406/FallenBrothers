// Fallen Brothers - Silver knife.
//
// Slot 5. Left click = quick stab. Silent (makes no noise), short reach.
// Silver wounds creatures that are weak to it. Pushes physics props a little.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FBEquipmentBase.h"
#include "Combat/FBDamageTypes.h"
#include "FBSilverKnife.generated.h"

class UStaticMeshComponent;

UCLASS()
class FALLENBROTHERS_API AFBSilverKnife : public AFBEquipmentBase
{
	GENERATED_BODY()

public:
	AFBSilverKnife();

	virtual void Tick(float DeltaSeconds) override;
	virtual void UseEquipment() override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Blade;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Handle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knife")
	float Reach = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knife")
	float HitRadius = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knife")
	float Damage = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knife")
	EFBDamageKind DamageKind = EFBDamageKind::Silver;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knife")
	float Cooldown = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knife")
	float StabDuration = 0.25f;

private:
	void DoHitCheck();

	FVector BaseHoldOffset = FVector::ZeroVector;
	float StabStartTime = -100.f;
	bool bHitDone = true;
};
