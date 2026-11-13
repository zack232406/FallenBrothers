// Fallen Brothers - Salt-round shotgun.

#include "Equipment/FBShotgun.h"

#include "Core/FBNoiseSubsystem.h"
#include "Inventory/FBInventoryComponent.h"
#include "Player/FBBodycamComponent.h"
#include "UI/FBHUD.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AFBShotgun::AFBShotgun()
{
	// Real model (if imported): /Game/FallenBrothers/Items/item_shotgun
	ModelName = TEXT("item_shotgun");
	ModelLength = 100.f;

	DisplayName = NSLOCTEXT("FB", "Shotgun", "Salt Shotgun");
	HoldOffset = FVector(35.f, 14.f, -16.f);
	AimLagSpeed = 16.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Placeholder: a dark 70 cm barrel pointing forward.
	Barrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
	Barrel->SetupAttachment(Root);
	Barrel->SetStaticMesh(Cylinder.Object);
	Barrel->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // Cylinder Z axis -> forward
	Barrel->SetRelativeScale3D(FVector(0.05f, 0.08f, 0.7f));
	Barrel->SetRelativeLocation(FVector(20.f, 0.f, 0.f));
	Barrel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Barrel->SetCastShadow(false);
	if (BasicMaterial.Succeeded())
	{
		Barrel->SetMaterial(0, BasicMaterial.Object);
	}

	MuzzleFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlash"));
	MuzzleFlash->SetupAttachment(Root);
	MuzzleFlash->SetRelativeLocation(FVector(60.f, 0.f, 0.f));
	MuzzleFlash->IntensityUnits = ELightUnits::Candelas;
	MuzzleFlash->SetIntensity(800.f);
	MuzzleFlash->SetLightColor(FLinearColor(1.f, 0.75f, 0.45f));
	MuzzleFlash->SetAttenuationRadius(1200.f);
	MuzzleFlash->SetVisibility(false);
}

void AFBShotgun::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInstanceDynamic* MID = Barrel->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.07f, 0.06f));
	}
}

void AFBShotgun::OnUnequipped()
{
	// Switching weapons cancels a reload.
	GetWorldTimerManager().ClearTimer(ReloadTimer);
	bReloading = false;
	MuzzleFlash->SetVisibility(false);
	Super::OnUnequipped();
}

void AFBShotgun::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlashTimeLeft > 0.f)
	{
		FlashTimeLeft -= DeltaSeconds;
		if (FlashTimeLeft <= 0.f)
		{
			MuzzleFlash->SetVisibility(false);
		}
	}

}

void AFBShotgun::Fire()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (bReloading || Now - LastFireTime < FireCooldown)
	{
		return;
	}

	if (LoadedShells <= 0)
	{
		AFBHUD::ShowNotificationFor(GetOwner(), NSLOCTEXT("FB", "ShotgunEmpty", "*click*  Empty - press R to reload."), 1.5f);
		LastFireTime = Now;
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	LoadedShells--;
	LastFireTime = Now;

	// Every creature for 50 m hears that.
	UFBNoiseSubsystem::ReportNoise(this, GetActorLocation(), 2.f, GetOwner());

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector Aim = ViewRot.Vector();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FBShotgun), true, GetOwner());
	Params.AddIgnoredActor(this);

	// Objects that can be "hit" even if they don't block (ghosts overlap everything).
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);

	TSet<AActor*> DamagedThisShot;

	for (int32 i = 0; i < PelletCount; ++i)
	{
		const FVector Dir = FMath::VRandCone(Aim, FMath::DegreesToRadians(SpreadDegrees));
		const FVector End = ViewLoc + Dir * Range;

		// 1) Where does this pellet stop? (walls, props)
		FHitResult WallHit;
		const bool bWall = GetWorld()->LineTraceSingleByChannel(WallHit, ViewLoc, End, ECC_Visibility, Params);
		const float StopDist = bWall ? WallHit.Distance : Range;

		if (bWall)
		{
			if (UPrimitiveComponent* Comp = WallHit.GetComponent())
			{
				if (Comp->IsSimulatingPhysics())
				{
					Comp->AddImpulseAtLocation(Dir * PelletImpulse * Comp->GetMass() * 0.1f, WallHit.ImpactPoint);
				}
			}
		}

		// 2) Anything damageable along the way (including incorporeal ghosts).
		TArray<FHitResult> Hits;
		GetWorld()->LineTraceMultiByObjectType(Hits, ViewLoc, ViewLoc + Dir * (StopDist + 20.f), ObjectTypes, Params);
		for (const FHitResult& H : Hits)
		{
			AActor* Target = H.GetActor();
			if (!Target || Target == GetOwner() || DamagedThisShot.Contains(Target))
			{
				continue;
			}
			if (IFBDamageable* Damageable = Cast<IFBDamageable>(Target))
			{
				FFBHitInfo Info;
				Info.Amount = DamagePerPellet * PelletCount; // One hit per shot per target
				Info.Kind = DamageKind;
				Info.Causer = this;
				Info.Location = H.ImpactPoint;
				Damageable->ApplyHit(Info);
				DamagedThisShot.Add(Target);
			}
		}
	}

	// Muzzle flash + recoil.
	MuzzleFlash->SetVisibility(true);
	FlashTimeLeft = 0.06f;
	ApplyRecoil();
}

void AFBShotgun::ApplyRecoil()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	// Kick the aim up a little (negative pitch input = look up in our setup).
	Pawn->AddControllerPitchInput(-RecoilPitch);
	Pawn->AddControllerYawInput(FMath::FRandRange(-0.6f, 0.6f));

	if (UFBBodycamComponent* Bodycam = Pawn->FindComponentByClass<UFBBodycamComponent>())
	{
		Bodycam->AddKick(FRotator(RecoilPitch * 1.5f, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-2.f, 2.f)));
	}
}

bool AFBShotgun::Reload(UFBInventoryComponent* Inventory)
{
	if (!Inventory || bReloading)
	{
		return true; // Handled (don't fall through to battery replacement).
	}
	if (LoadedShells >= MagazineSize)
	{
		AFBHUD::ShowNotificationFor(GetOwner(), NSLOCTEXT("FB", "ShotgunFull", "Both barrels loaded."), 1.5f);
		return true;
	}
	if (!Inventory->HasItem(AmmoItemId))
	{
		AFBHUD::ShowNotificationFor(GetOwner(), NSLOCTEXT("FB", "NoShells", "No salt shells left."), 1.5f);
		return true;
	}

	bReloading = true;
	ReloadInventory = Inventory;
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &AFBShotgun::FinishReloadShell, ReloadTimePerShell, false);
	return true;
}

void AFBShotgun::FinishReloadShell()
{
	UFBInventoryComponent* Inventory = ReloadInventory.Get();
	if (Inventory && LoadedShells < MagazineSize && Inventory->RemoveItem(AmmoItemId, 1))
	{
		LoadedShells++;
	}

	// Keep loading shell by shell until full or out of ammo.
	if (Inventory && LoadedShells < MagazineSize && Inventory->HasItem(AmmoItemId))
	{
		GetWorldTimerManager().SetTimer(ReloadTimer, this, &AFBShotgun::FinishReloadShell, ReloadTimePerShell, false);
		return;
	}
	bReloading = false;
}

void AFBShotgun::DrawEquipmentHUD(AFBHUD* HUD, float UIScale)
{
	if (!HUD || !HUD->HasCanvas())
	{
		return;
	}
	Super::DrawEquipmentHUD(HUD, UIScale); // Name

	// Shells: filled = loaded.
	const float W = 10.f * UIScale, H = 22.f * UIScale, Gap = 6.f * UIScale;
	const float RightX = HUD->GetScreenSize().X - 40.f * UIScale;
	const float Y = HUD->GetScreenSize().Y - 44.f * UIScale;

	// Reserve ammo count
	int32 Reserve = 0;
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (const UFBInventoryComponent* Inv = Pawn->FindComponentByClass<UFBInventoryComponent>())
		{
			Reserve = Inv->GetItemCount(AmmoItemId);
		}
	}
	const FString ReserveText = bReloading ? FString(TEXT("RELOADING")) : FString::Printf(TEXT("+%d"), Reserve);
	const float TextW = 80.f * UIScale;
	DrawRightAlignedText(HUD, ReserveText, RightX, Y, FLinearColor(1.f, 1.f, 1.f, 0.6f), 0.9f * UIScale);

	for (int32 i = 0; i < MagazineSize; ++i)
	{
		const float X = RightX - TextW - (MagazineSize - i) * (W + Gap);
		const bool bLoaded = i < LoadedShells;
		HUD->DrawRect(bLoaded ? FLinearColor(0.85f, 0.2f, 0.15f, 0.9f) : FLinearColor(1.f, 1.f, 1.f, 0.15f), X, Y, W, H);
	}
}
