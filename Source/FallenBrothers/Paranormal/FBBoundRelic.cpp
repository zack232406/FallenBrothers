// Fallen Brothers - Bound relic. See FBBoundRelic.h.

#include "Paranormal/FBBoundRelic.h"

#include "Paranormal/FBGhost.h"
#include "Paranormal/FBParanormalSourceComponent.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Investigation/FBInvestigationTypes.h"
#include "Companion/FBCompanion.h"
#include "UI/FBHUD.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"

AFBBoundRelic::AFBBoundRelic()
{
	PrimaryActorTick.bCanEverTick = true;

	Touch = CreateDefaultSubobject<USphereComponent>(TEXT("Touch"));
	Touch->InitSphereRadius(45.f);
	Touch->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Touch->SetCollisionResponseToAllChannels(ECR_Ignore);
	Touch->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(Touch);

	Glint = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glint"));
	Glint->SetupAttachment(Touch);
	Glint->IntensityUnits = ELightUnits::Candelas;
	Glint->SetIntensity(0.6f);
	Glint->SetLightColor(FLinearColor(0.55f, 0.75f, 1.f));
	Glint->SetAttenuationRadius(260.f);
	Glint->SetCastShadows(false);

	EMF = CreateDefaultSubobject<UFBParanormalSourceComponent>(TEXT("EMF"));
	EMF->SetupAttachment(Touch);
	EMF->EMFStrength = 5.f;
	EMF->Radius = 1100.f;
}

void AFBBoundRelic::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// A slow, uneven pulse: something breathing.
	Time += DeltaSeconds;
	Glint->SetIntensity(0.35f + 0.45f * FMath::Abs(FMath::Sin(Time * 1.3f)) * (0.6f + 0.4f * FMath::PerlinNoise1D(Time * 2.1f)));
}

bool AFBBoundRelic::CanInteract_Implementation(AActor* InteractingActor) const
{
	return InteractingActor && FVector::Dist(InteractingActor->GetActorLocation(), GetActorLocation()) < 320.f;
}

FText AFBBoundRelic::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	return FText::Format(NSLOCTEXT("FB", "RelicTake", "Take {0}"), RelicName);
}

void AFBBoundRelic::Interact_Implementation(AActor* InteractingActor)
{
	if (!InteractingActor)
	{
		return;
	}
	if (UFBCaseJournalComponent* Journal = InteractingActor->FindComponentByClass<UFBCaseJournalComponent>())
	{
		FFBJournalEntry Entry;
		Entry.Id = TEXT("Relic_KeyRing");
		Entry.Title = RelicName;
		Entry.Text = NSLOCTEXT("FB", "RelicText", "An iron ring, nine keys, a twist of rusted wire still through the loop. Ice cold - it burns to hold. This is what he chained the doors with, and what they drowned him with. It goes in the fire with the bones.");
		Entry.Type = EFBJournalEntryType::Evidence;
		Journal->AddEntry(Entry, false);
		Journal->AddClue(GrantsClue);
	}
	AFBHUD::ShowNotificationFor(InteractingActor, NSLOCTEXT("FB", "RelicTaken", "You have the key ring. Every door in the building slams at once.\nGet to the grave."), 6.f);
	for (TActorIterator<AFBGhost> It(GetWorld()); It; ++It)
	{
		It->Enrage(); // he wants it back
	}
	for (TActorIterator<AFBCompanion> It(GetWorld()); It; ++It)
	{
		It->Bark({ NSLOCTEXT("FB", "ZackRelic", "Got it? Then RUN. Churchyard, north wall - I'll keep him off you!") }, TEXT("RelicTaken"), 0.f);
		break;
	}
	Destroy();
}
