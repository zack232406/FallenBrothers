// Fallen Brothers - Silver knife.

#include "Equipment/FBSilverKnife.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBSilverKnife::AFBSilverKnife()
{
	// Real model (if imported): /Game/FallenBrothers/Items/item_knife
	ModelName = TEXT("item_knife");
	ModelLength = 23.f;

	DisplayName = NSLOCTEXT("FB", "SilverKnife", "Silver Knife");
	HoldOffset = FVector(32.f, 16.f, -18.f);
	AimLagSpeed = 18.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Placeholder: 20 cm blade + 11 cm handle, pointing forward.
	Blade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blade"));
	Blade->SetupAttachment(Root);
	Blade->SetStaticMesh(Cube.Object);
	Blade->SetRelativeScale3D(FVector(0.2f, 0.008f, 0.03f));
	Blade->SetRelativeLocation(FVector(12.f, 0.f, 0.f));

	Handle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Handle"));
	Handle->SetupAttachment(Root);
	Handle->SetStaticMesh(Cube.Object);
	Handle->SetRelativeScale3D(FVector(0.11f, 0.025f, 0.035f));
	Handle->SetRelativeLocation(FVector(-4.f, 0.f, 0.f));

	for (UStaticMeshComponent* C : { Blade.Get(), Handle.Get() })
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		if (BasicMaterial.Succeeded())
		{
			C->SetMaterial(0, BasicMaterial.Object);
		}
	}
}

void AFBSilverKnife::BeginPlay()
{
	Super::BeginPlay();
	BaseHoldOffset = HoldOffset;

	if (UMaterialInstanceDynamic* MID = Blade->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.8f, 0.82f, 0.86f));
	}
	if (UMaterialInstanceDynamic* MID = Handle->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.05f, 0.03f));
	}
}

void AFBSilverKnife::UseEquipment()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - StabStartTime < Cooldown)
	{
		return;
	}
	StabStartTime = Now;
	bHitDone = false;
}

void AFBSilverKnife::Tick(float DeltaSeconds)
{
	// Stab animation: thrust forward and back.
	const float T = (GetWorld()->GetTimeSeconds() - StabStartTime) / StabDuration;
	if (T >= 0.f && T <= 1.f)
	{
		const float Arc = FMath::Sin(T * UE_PI);
		HoldOffset = BaseHoldOffset + FVector(45.f * Arc, -12.f * Arc, 6.f * Arc);
		if (T >= 0.4f && !bHitDone)
		{
			bHitDone = true;
			DoHitCheck();
		}
	}
	else
	{
		HoldOffset = BaseHoldOffset;
	}

	Super::Tick(DeltaSeconds); // Follow the camera with the animated offset.
}

void AFBSilverKnife::DoHitCheck()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector Dir = ViewRot.Vector();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FBKnife), false, GetOwner());
	Params.AddIgnoredActor(this);

	// Don't stab through walls.
	float MaxDist = Reach;
	FHitResult WallHit;
	if (GetWorld()->LineTraceSingleByChannel(WallHit, ViewLoc, ViewLoc + Dir * Reach, ECC_Visibility, Params))
	{
		MaxDist = WallHit.Distance + HitRadius;
		if (UPrimitiveComponent* Comp = WallHit.GetComponent())
		{
			if (Comp->IsSimulatingPhysics())
			{
				Comp->AddImpulseAtLocation(Dir * 250.f * Comp->GetMass(), WallHit.ImpactPoint);
			}
		}
	}

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, ViewLoc, ViewLoc + Dir * MaxDist, FQuat::Identity, Objects,
	                                   FCollisionShape::MakeSphere(HitRadius), Params);

	for (const FHitResult& H : Hits)
	{
		AActor* Target = H.GetActor();
		if (IFBDamageable* Damageable = Cast<IFBDamageable>(Target))
		{
			FFBHitInfo Info;
			Info.Amount = Damage;
			Info.Kind = DamageKind;
			Info.Causer = GetOwner();
			Info.Location = H.ImpactPoint;
			Damageable->ApplyHit(Info);
			return; // One target per stab.
		}
	}
}
