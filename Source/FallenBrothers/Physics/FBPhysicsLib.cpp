// Fallen Brothers - Small physics helpers.

#include "Physics/FBPhysicsLib.h"

#include "Combat/FBHealthComponent.h"
#include "Physics/FBGrabComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void FFBPhysicsLib::DamageActor(AActor* Target, float Amount, EFBDamageKind Kind, AActor* Causer, const FVector& Where)
{
	if (!Target || Amount <= 0.f)
	{
		return;
	}
	if (IFBDamageable* Damageable = Cast<IFBDamageable>(Target))
	{
		FFBHitInfo Hit;
		Hit.Amount = Amount;
		Hit.Kind = Kind;
		Hit.Causer = Causer;
		Hit.Location = Where;
		Damageable->ApplyHit(Hit);
		return;
	}
	if (UFBHealthComponent* Health = Target->FindComponentByClass<UFBHealthComponent>())
	{
		Health->ApplyDamage(Amount, Causer);
	}
}

bool FFBPhysicsLib::IsHeldByPlayer(const UObject* WorldContext, const UPrimitiveComponent* Component)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(WorldContext, 0);
	const UFBGrabComponent* Grab = Player ? Player->FindComponentByClass<UFBGrabComponent>() : nullptr;
	return Grab && Component && Grab->GetGrabbedComponent() == Component;
}

UStaticMesh* FFBPhysicsLib::BasicShape(const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name);
	return LoadObject<UStaticMesh>(nullptr, *Path);
}

void FFBPhysicsLib::ApplyLook(UStaticMeshComponent* Mesh, UMaterialInterface* Material, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return;
	}
	if (Material)
	{
		Mesh->SetMaterial(0, Material);
		return;
	}
	static UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Basic)
	{
		Mesh->SetMaterial(0, Basic);
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

UStaticMeshComponent* FFBPhysicsLib::MakePiece(AActor* Owner, UStaticMesh* Mesh, const FTransform& WorldTransform, const FVector& SizeCm,
                                               UMaterialInterface* Material, const FLinearColor& Color, float MassKg, bool bSimulate)
{
	if (!Owner || !Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Owner);
	Piece->SetStaticMesh(Mesh);
	Piece->SetMobility(EComponentMobility::Movable);
	Piece->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Piece->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // never trap the player on debris
	Piece->SetCanEverAffectNavigation(false);
	Piece->SetupAttachment(Owner->GetRootComponent());
	Piece->RegisterComponent();
	Owner->AddInstanceComponent(Piece);

	FTransform T = WorldTransform;
	T.SetScale3D(SizeCm / 100.f);
	Piece->SetWorldTransform(T);
	ApplyLook(Piece, Material, Color);

	if (bSimulate)
	{
		Piece->SetSimulatePhysics(true);
		Piece->SetMassOverrideInKg(NAME_None, FMath::Max(0.05f, MassKg), true);
		Piece->SetLinearDamping(0.05f);
		Piece->SetAngularDamping(0.2f);
	}
	return Piece;
}

bool FFBPhysicsLib::HurtCharactersTouching(UPrimitiveComponent* Piece, float MinSpeed, float DamagePerKgMs, AActor* Causer,
                                           TSet<TWeakObjectPtr<AActor>>& AlreadyHit)
{
	if (!Piece || !Piece->IsSimulatingPhysics())
	{
		return false;
	}
	const float Speed = Piece->GetPhysicsLinearVelocity().Size();
	if (Speed < MinSpeed)
	{
		return false;
	}

	const FVector Center = Piece->Bounds.Origin;
	const float Radius = Piece->Bounds.SphereRadius * 0.8f;
	bool bHit = false;

	for (TActorIterator<ACharacter> It(Piece->GetWorld()); It; ++It)
	{
		ACharacter* C = *It;
		if (AlreadyHit.Contains(C))
		{
			continue;
		}
		const UCapsuleComponent* Capsule = C->GetCapsuleComponent();
		const float R = Capsule->GetScaledCapsuleRadius();
		const float H = Capsule->GetScaledCapsuleHalfHeight() - R;
		const FVector A = C->GetActorLocation() - FVector(0.f, 0.f, H);
		const FVector B = C->GetActorLocation() + FVector(0.f, 0.f, H);
		const FVector Closest = FMath::ClosestPointOnSegment(Center, A, B);
		if (FVector::Dist(Closest, Center) > Radius + R)
		{
			continue;
		}

		const float Damage = FMath::Clamp(Piece->GetMass() * (Speed / 100.f) * DamagePerKgMs, 0.f, 100.f);
		FFBPhysicsLib::DamageActor(C, Damage, EFBDamageKind::Blunt, Causer, Center);
		C->LaunchCharacter(Piece->GetPhysicsLinearVelocity().GetSafeNormal() * FMath::Min(Speed * 0.4f, 600.f) + FVector(0.f, 0.f, 150.f), false, false);
		AlreadyHit.Add(C);
		bHit = true;
	}
	return bHit;
}
