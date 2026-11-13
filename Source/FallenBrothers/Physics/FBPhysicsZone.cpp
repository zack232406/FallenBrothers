// Fallen Brothers - Base for box-shaped physics areas.

#include "Physics/FBPhysicsZone.h"

#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

AFBPhysicsZone::AFBPhysicsZone()
{
	PrimaryActorTick.bCanEverTick = true;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	SetRootComponent(Area);
	Area->SetBoxExtent(FVector(400.f, 400.f, 200.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision); // we check by hand - never blocks anything
	Area->SetCanEverAffectNavigation(false);
	Area->ShapeColor = FColor(255, 140, 0);
	Area->SetHiddenInGame(true);
}

FVector AFBPhysicsZone::GetBoxExtent() const
{
	return Area->GetScaledBoxExtent();
}

bool AFBPhysicsZone::IsInside(const FVector& World) const
{
	const FVector Local = Area->GetComponentTransform().InverseTransformPositionNoScale(World);
	const FVector E = GetBoxExtent();
	return FMath::Abs(Local.X) <= E.X && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}

void AFBPhysicsZone::GetCharactersInside(TArray<ACharacter*>& Out) const
{
	Out.Reset();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (IsInside(It->GetActorLocation()))
		{
			Out.Add(*It);
		}
	}
}

void AFBPhysicsZone::GetBodiesInside(TArray<UPrimitiveComponent*>& Out) const
{
	Out.Reset();
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Area->GetComponentLocation(), Area->GetComponentQuat(), Objects,
	                                     FCollisionShape::MakeBox(GetBoxExtent()));
	for (const FOverlapResult& O : Overlaps)
	{
		UPrimitiveComponent* C = O.GetComponent();
		if (C && C->IsSimulatingPhysics())
		{
			Out.AddUnique(C);
		}
	}
}
