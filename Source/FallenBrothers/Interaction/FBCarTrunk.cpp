// Fallen Brothers - The brothers' car trunk.

#include "Interaction/FBCarTrunk.h"

#include "Equipment/FBEquipmentComponent.h"
#include "Equipment/FBSaltCanister.h"
#include "Equipment/FBShotgun.h"
#include "Equipment/FBSilverKnife.h"
#include "Inventory/FBInventoryComponent.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "UI/FBHUD.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AFBCarTrunk::AFBCarTrunk()
{
	PrimaryActorTick.bCanEverTick = false;

	// What the look-trace hits: a box over the trunk lid and rear bumper. Only blocks the trace, never the player.
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->InitBoxExtent(FVector(45.f, 80.f, 35.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	TrunkLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TrunkLight"));
	TrunkLight->SetupAttachment(Box);
	TrunkLight->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	TrunkLight->SetMobility(EComponentMobility::Movable);
	TrunkLight->SetIntensityUnits(ELightUnits::Candelas);
	TrunkLight->SetIntensity(6.f);
	TrunkLight->SetLightColor(FLinearColor(1.f, 0.78f, 0.5f));
	TrunkLight->SetAttenuationRadius(220.f);
	TrunkLight->SetCastShadows(false);
	TrunkLight->SetVisibility(false);

	Weapons = { AFBShotgun::StaticClass(), AFBSaltCanister::StaticClass(), AFBSilverKnife::StaticClass() };

	FFBInventoryItem Shells;
	Shells.ItemId = TEXT("SaltShell");
	Shells.DisplayName = NSLOCTEXT("FB", "ItemSaltShell", "Rock salt shells");
	Shells.Description = NSLOCTEXT("FB", "SaltShellDesc", "Shotgun shells packed with rock salt. Won't kill a person. Scatters a spirit.");
	Shells.Category = EFBItemCategory::Ammo;
	Shells.Quantity = 12;
	Shells.MaxStack = 24;
	Supplies.Add(Shells);

	FFBInventoryItem Salt;
	Salt.ItemId = TEXT("Salt");
	Salt.DisplayName = NSLOCTEXT("FB", "ItemSalt", "Salt");
	Salt.Description = NSLOCTEXT("FB", "SaltDesc", "Pour a line (slot 4) - spirits can't cross it. Also for salting remains.");
	Salt.Category = EFBItemCategory::Ritual;
	Salt.Quantity = 2;
	Supplies.Add(Salt);

	FFBInventoryItem Battery;
	Battery.Description = NSLOCTEXT("FB", "BatteryDesc", "Spare battery for the flashlight or UV light. Press R to replace.");
	Battery.Quantity = 2;
	Supplies.Add(Battery);

	OpenSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/FallenBrothers/Audio/Steps/metal/S_metal_0.S_metal_0"));
	CloseSound = OpenSound;
}

void AFBCarTrunk::BeginPlay()
{
	Super::BeginPlay();

	// Gloss black paint over whatever colour the car model came in (chrome and glass get darker too, like tinted chrome).
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(CarTag))
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
		for (UStaticMeshComponent* M : Meshes)
		{
			for (int32 I = 0; I < M->GetNumMaterials(); ++I)
			{
				if (UMaterialInstanceDynamic* MID = M->CreateAndSetMaterialInstanceDynamic(I))
				{
					for (const TCHAR* P : { TEXT("BaseColorFactor"), TEXT("Base Color Factor"), TEXT("BaseColor"), TEXT("Color"), TEXT("Tint") })
					{
						MID->SetVectorParameterValue(P, PaintColor);
					}
					for (const TCHAR* P : { TEXT("RoughnessFactor"), TEXT("Roughness Factor"), TEXT("Roughness") })
					{
						MID->SetScalarParameterValue(P, 0.22f);
					}
					for (const TCHAR* P : { TEXT("MetallicFactor"), TEXT("Metallic Factor"), TEXT("Metallic") })
					{
						MID->SetScalarParameterValue(P, 0.35f);
					}
				}
			}
		}
	}
}

FText AFBCarTrunk::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	if (!bOpen)
	{
		return NSLOCTEXT("FB", "TrunkOpen", "Open the trunk");
	}
	return NSLOCTEXT("FB", "TrunkClose", "Close the trunk");
}

void AFBCarTrunk::Interact_Implementation(AActor* InteractingActor)
{
	bOpen = !bOpen;
	TrunkLight->SetVisibility(bOpen);
	if (USoundBase* S = bOpen ? OpenSound.Get() : CloseSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), bOpen ? 0.8f : 1.f, bOpen ? 0.7f : 0.55f);
	}
	if (!bOpen || bLooted || !InteractingActor)
	{
		return;
	}
	bLooted = true;

	// The false bottom: shotgun, salt, silver, iron... Everything goes with Chris.
	TArray<FString> Taken;
	if (UFBEquipmentComponent* Equipment = InteractingActor->FindComponentByClass<UFBEquipmentComponent>())
	{
		for (const TSubclassOf<AFBEquipmentBase>& W : Weapons)
		{
			if (W && Equipment->AddEquipment(W))
			{
				Taken.Add(W == AFBShotgun::StaticClass() ? TEXT("sawed-off shotgun") : W == AFBSaltCanister::StaticClass() ? TEXT("salt canister") : W == AFBSilverKnife::StaticClass() ? TEXT("silver knife") : W->GetName());
			}
		}
	}
	if (UFBInventoryComponent* Inventory = InteractingActor->FindComponentByClass<UFBInventoryComponent>())
	{
		for (const FFBInventoryItem& I : Supplies)
		{
			if (Inventory->AddItem(I) > 0)
			{
				Taken.Add(FString::Printf(TEXT("%s x%d"), *I.DisplayName.ToString().ToLower(), I.Quantity));
			}
		}
	}
	if (UFBCaseJournalComponent* Journal = InteractingActor->FindComponentByClass<UFBCaseJournalComponent>())
	{
		Journal->AddClue(TEXT("Clue_Gear"));
	}
	AFBHUD::ShowNotificationFor(InteractingActor,
		Taken.Num() > 0 ? FText::Format(NSLOCTEXT("FB", "TrunkTaken", "From the trunk: {0}."), FText::FromString(FString::Join(Taken, TEXT(", "))))
		                : NSLOCTEXT("FB", "TrunkEmpty", "Nothing left in here you need."), 6.f);
}
