// Fallen Brothers - Grave site (salt and burn).

#include "Paranormal/FBGraveSite.h"
#include "Mission/FBCaseDirector.h"

#include "Investigation/FBCaseJournalComponent.h"
#include "Inventory/FBInventoryComponent.h"
#include "Paranormal/FBGhost.h"
#include "UI/FBHUD.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBGraveSite::AFBGraveSite()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Only while burning.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeMesh = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Scale);
		if (BasicMaterial.Succeeded())
		{
			C->SetMaterial(0, BasicMaterial.Object);
		}
		return C;
	};

	// Grave mound 200 x 100 cm (actor origin on the ground).
	Mound = MakeMesh(TEXT("Mound"), Cube.Object, FVector(0.f, 0.f, 12.f), FVector(2.0f, 1.0f, 0.25f));
	Mound->SetCollisionProfileName(TEXT("BlockAll"));

	// Open hole (dark), skull + bones: hidden until dug.
	Hole = MakeMesh(TEXT("Hole"), Cube.Object, FVector(0.f, 0.f, 1.f), FVector(1.9f, 0.9f, 0.02f));
	Skull = MakeMesh(TEXT("Skull"), Sphere.Object, FVector(70.f, 0.f, 10.f), FVector(0.18f));
	Bones = MakeMesh(TEXT("Bones"), Cylinder.Object, FVector(0.f, 0.f, 6.f), FVector(0.08f, 0.08f, 1.1f));
	Bones->SetRelativeRotation(FRotator(90.f, 0.f, 0.f)); // Lying along the grave

	// Fire (placeholder flames + light): hidden until burned.
	Flames = MakeMesh(TEXT("Flames"), Cube.Object, FVector(0.f, 0.f, 30.f), FVector(1.4f, 0.5f, 0.6f));

	for (UStaticMeshComponent* C : { Hole.Get(), Skull.Get(), Bones.Get(), Flames.Get() })
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetVisibility(false);
	}
	// The open hole stays "lookable" (interaction trace) after the mound is gone.
	Hole->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hole->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hole->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Flames->SetCastShadow(false);

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Root);
	FireLight->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	FireLight->IntensityUnits = ELightUnits::Candelas;
	FireLight->SetIntensity(0.f);
	FireLight->SetLightColor(FLinearColor(1.f, 0.5f, 0.15f));
	FireLight->SetAttenuationRadius(1500.f);
	FireLight->SetVisibility(false);
}

void AFBGraveSite::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetCompColor(Mound, FLinearColor(0.2f, 0.13f, 0.08f));
	SetCompColor(Hole, FLinearColor(0.02f, 0.015f, 0.01f));
	SetCompColor(Skull, FLinearColor(0.85f, 0.8f, 0.68f));
	SetCompColor(Bones, FLinearColor(0.8f, 0.75f, 0.62f));
	SetCompColor(Flames, FLinearColor(1.f, 0.45f, 0.05f));
}

void AFBGraveSite::BeginPlay()
{
	Super::BeginPlay();
	OnConstruction(GetActorTransform());
}

void AFBGraveSite::SetCompColor(UStaticMeshComponent* Comp, const FLinearColor& Color) const
{
	if (UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

AFBGhost* AFBGraveSite::GetGhost() const
{
	if (LinkedGhost)
	{
		return LinkedGhost;
	}
	for (TActorIterator<AFBGhost> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

bool AFBGraveSite::PlayerKnowsGrave(const AActor* Player) const
{
	if (RequiredClue.IsNone())
	{
		return true;
	}
	const UFBCaseJournalComponent* Journal = Player ? Player->FindComponentByClass<UFBCaseJournalComponent>() : nullptr;
	return Journal && Journal->HasClue(RequiredClue);
}

// ------------------------------------------------------------------ Interaction

bool AFBGraveSite::CanInteract_Implementation(AActor* InteractingActor) const
{
	// Nobody digs up a grave with the whole town watching: the case's evening part only.
	if (UFBCaseDirector::IsCaseDay(GetWorld()))
	{
		return false;
	}
	return PlayerKnowsGrave(InteractingActor) && State < EFBGraveState::Burning;
}

FText AFBGraveSite::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	if (!PlayerKnowsGrave(InteractingActor))
	{
		return NSLOCTEXT("FB", "GraveUnknown", "An unmarked grave");
	}
	if (UFBCaseDirector::IsCaseDay(GetWorld()))
	{
		return NSLOCTEXT("FB", "GraveDaylight", "Crane's grave - not in broad daylight. Come back after dark.");
	}
	switch (State)
	{
	case EFBGraveState::Undug:   return DigDone > 0 ? NSLOCTEXT("FB", "GraveDigMore", "Keep digging") : NSLOCTEXT("FB", "GraveDig", "Dig up the grave");
	case EFBGraveState::Dug:     return NSLOCTEXT("FB", "GraveSalt", "Salt the bones");
	case EFBGraveState::Salted:  return NSLOCTEXT("FB", "GraveBurn", "Throw in the key ring and burn the bones");
	case EFBGraveState::Burning: return NSLOCTEXT("FB", "GraveBurning", "The bones are burning");
	default:                     return NSLOCTEXT("FB", "GraveDone", "Ashes");
	}
}

void AFBGraveSite::Interact_Implementation(AActor* InteractingActor)
{
	UFBInventoryComponent* Inventory = InteractingActor ? InteractingActor->FindComponentByClass<UFBInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return;
	}

	switch (State)
	{
	case EFBGraveState::Undug:
		if (!DigItemId.IsNone() && !Inventory->HasItem(DigItemId))
		{
			AFBHUD::ShowNotificationFor(InteractingActor, FText::Format(NSLOCTEXT("FB", "GraveNeedTool", "You need a {0} to dig."), FText::FromName(DigItemId)));
			return;
		}
		{
			// Digging is work: one shovelful per press, and he doesn't wait for you to finish.
			const float Now = GetWorld()->GetTimeSeconds();
			if (Now < NextDigTime)
			{
				return;
			}
			NextDigTime = Now + 1.1f;
			++DigDone;
			if (DigDone == 1)
			{
				if (AFBGhost* Ghost = GetGhost())
				{
					Ghost->SetActorLocation(GetActorLocation() + FVector(600.f, 0.f, 0.f)); // his haunt is here now
					Ghost->Enrage();
				}
			}
			if (DigDone < DigSteps)
			{
				Mound->SetRelativeScale3D(Mound->GetRelativeScale3D() * FVector(1.f, 1.f, 0.82f));
				AFBHUD::ShowNotificationFor(InteractingActor, FText::Format(NSLOCTEXT("FB", "GraveDigging", "Digging... ({0}/{1})"), FText::AsNumber(DigDone), FText::AsNumber(DigSteps)), 1.2f);
				return;
			}
		}
		State = EFBGraveState::Dug;
		Mound->SetVisibility(false);
		Mound->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hole->SetVisibility(true);
		Skull->SetVisibility(true);
		Bones->SetVisibility(true);
		AFBHUD::ShowNotificationFor(InteractingActor, NSLOCTEXT("FB", "GraveDug", "The shovel hits rotten wood. The lid splits... bones."), 3.5f);

		if (AFBGhost* Ghost = GetGhost())
		{
			Ghost->Enrage(); // It knows. Hold it off while you finish.
		}
		break;

	case EFBGraveState::Dug:
		if (!Inventory->RemoveItem(SaltItemId, 1))
		{
			AFBHUD::ShowNotificationFor(InteractingActor, NSLOCTEXT("FB", "GraveNeedSalt", "You need salt."));
			return;
		}
		State = EFBGraveState::Salted;
		SetCompColor(Bones, FLinearColor(0.97f, 0.97f, 0.95f));
		SetCompColor(Skull, FLinearColor(0.97f, 0.97f, 0.95f));
		AFBHUD::ShowNotificationFor(InteractingActor, NSLOCTEXT("FB", "GraveSalted", "You pour the salt over the bones."));
		break;

	case EFBGraveState::Salted:
		if (!Inventory->HasItem(LighterItemId))
		{
			AFBHUD::ShowNotificationFor(InteractingActor, NSLOCTEXT("FB", "GraveNeedLighter", "You need something to light it with."));
			return;
		}
		if (!RelicClue.IsNone())
		{
			const UFBCaseJournalComponent* J = InteractingActor->FindComponentByClass<UFBCaseJournalComponent>();
			if (!J || !J->HasClue(RelicClue))
			{
				AFBHUD::ShowNotificationFor(InteractingActor, NSLOCTEXT("FB", "GraveNeedRelic", "The flame dies on the bones. Something of his is still above ground - the key ring. The mill."), 6.f);
				return;
			}
		}
		State = EFBGraveState::Burning;
		BurnStartTime = GetWorld()->GetTimeSeconds();
		Flames->SetVisibility(true);
		FireLight->SetVisibility(true);
		SetActorTickEnabled(true);

		// The spirit burns with its bones.
		if (AFBGhost* Ghost = GetGhost())
		{
			Ghost->SetActorLocation(GetActorLocation());
			Ghost->Banish();
		}

		if (UFBCaseJournalComponent* Journal = InteractingActor->FindComponentByClass<UFBCaseJournalComponent>())
		{
			FFBJournalEntry Entry;
			Entry.Id = TEXT("Case_Solved");
			Entry.Title = FText::Format(NSLOCTEXT("FB", "CaseSolvedTitle", "CASE CLOSED: {0}"), CaseTitle);
			Entry.Text = NSLOCTEXT("FB", "CaseSolvedText", "The remains were salted and burned. The cold is gone. Whatever was holding on in this town has finally let go.");
			Entry.Type = EFBJournalEntryType::Note;
			Journal->AddEntry(Entry, false);
			Journal->AddClue(TEXT("Case_Solved"));
		}

		AFBHUD::ShowNotificationFor(InteractingActor,
			FText::Format(NSLOCTEXT("FB", "CaseSolvedMsg", "The bones catch fire...\nCASE SOLVED: {0}"), CaseTitle), 7.f);
		break;

	default:
		break;
	}
}

// ------------------------------------------------------------------ Fire

void AFBGraveSite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (State != EFBGraveState::Burning)
	{
		return;
	}

	const float Elapsed = GetWorld()->GetTimeSeconds() - BurnStartTime;
	const float Life = FMath::Clamp(1.f - Elapsed / BurnDuration, 0.f, 1.f);

	// Flickering fire light + dancing placeholder flames.
	FlickerTime += DeltaSeconds * 8.f;
	const float N = 0.75f + 0.25f * FMath::PerlinNoise1D(FlickerTime);
	FireLight->SetIntensity(1500.f * N * FMath::Max(Life, 0.15f));
	Flames->SetRelativeScale3D(FVector(1.4f, 0.5f, 0.6f * N * FMath::Max(Life, 0.1f)));

	if (Life <= 0.f)
	{
		State = EFBGraveState::Done;
		Flames->SetVisibility(false);
		Skull->SetVisibility(false);
		Bones->SetVisibility(false);
		FireLight->SetIntensity(80.f); // Glowing embers
		SetActorTickEnabled(false);
	}
}
