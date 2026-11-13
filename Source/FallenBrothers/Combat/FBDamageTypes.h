// Fallen Brothers - Damage / weakness types.
//
// Every hit carries a "kind". Creatures decide what hurts them:
// ghosts fear iron and salt, other creatures may need silver, fire, holy...

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FBDamageTypes.generated.h"

UENUM(BlueprintType)
enum class EFBDamageKind : uint8
{
	Blunt,   // Crate, chair, fist...
	Iron,    // Iron pipe, iron chandelier, iron rounds
	Salt,    // Salt rounds, thrown salt
	Silver,
	Fire,
	Holy,
	Bullet   // Normal ammunition
};

USTRUCT(BlueprintType)
struct FFBHitInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	float Amount = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	EFBDamageKind Kind = EFBDamageKind::Blunt;

	/** The object that physically hit (pipe, chandelier, bullet...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	TObjectPtr<AActor> Causer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FVector Location = FVector::ZeroVector;
};

/** Anything that can be hurt / affected by hits (creatures, breakables). */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFBDamageable : public UInterface
{
	GENERATED_BODY()
};

class FALLENBROTHERS_API IFBDamageable
{
	GENERATED_BODY()

public:
	virtual void ApplyHit(const FFBHitInfo& Hit) = 0;
};

/** Anything that can hit things physically and knows what it's made of (props, chandelier...). */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFBImpactSource : public UInterface
{
	GENERATED_BODY()
};

class FALLENBROTHERS_API IFBImpactSource
{
	GENERATED_BODY()

public:
	virtual EFBDamageKind GetImpactKind() const = 0;
	virtual float GetImpactMassKg() const = 0;
};
