// Fallen Brothers - Salt-round shotgun.
//
// Double-barrel loaded with rock-salt shells. Left click fires, R reloads one shell at a time
// from the inventory ("SaltShell"). Salt disperses ghosts; pellets also push physics props.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FBEquipmentBase.h"
#include "Combat/FBDamageTypes.h"
#include "FBShotgun.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UFBInventoryComponent;

UCLASS()
class FALLENBROTHERS_API AFBShotgun : public AFBEquipmentBase
{
	GENERATED_BODY()

public:
	AFBShotgun();

	virtual void Tick(float DeltaSeconds) override;
	virtual void UseEquipment() override { Fire(); }
	virtual bool Reload(UFBInventoryComponent* Inventory) override;
	virtual void OnUnequipped() override;
	virtual void DrawEquipmentHUD(AFBHUD* HUD, float UIScale) override;

	UFUNCTION(BlueprintCallable, Category = "Shotgun")
	void Fire();

	UFUNCTION(BlueprintPure, Category = "Shotgun")
	int32 GetLoadedShells() const { return LoadedShells; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Barrel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MuzzleFlash;

	/** Inventory item used as ammo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	FName AmmoItemId = TEXT("SaltShell");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	int32 MagazineSize = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	int32 PelletCount = 9;

	/** Pellet spread (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	float SpreadDegrees = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	float Range = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	float DamagePerPellet = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	EFBDamageKind DamageKind = EFBDamageKind::Salt;

	/** Push on physics objects per pellet (cm/s velocity change, divided by mass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	float PelletImpulse = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	float FireCooldown = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	float ReloadTimePerShell = 0.75f;

	/** Camera kick upward per shot (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	float RecoilPitch = 3.5f;

private:
	void FinishReloadShell();
	void ApplyRecoil();

	int32 LoadedShells = 2;
	float LastFireTime = -100.f;
	float FlashTimeLeft = 0.f;

	bool bReloading = false;
	FTimerHandle ReloadTimer;
	TWeakObjectPtr<UFBInventoryComponent> ReloadInventory;
};
