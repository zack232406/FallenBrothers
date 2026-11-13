// Fallen Brothers - Last-seen spot. See FBLastSeenSpot.h.

#include "Paranormal/FBLastSeenSpot.h"

#include "Paranormal/FBParanormalSourceComponent.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Investigation/FBInvestigationTypes.h"
#include "UI/FBHUD.h"
#include "Components/SphereComponent.h"

AFBLastSeenSpot::AFBLastSeenSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	Touch = CreateDefaultSubobject<USphereComponent>(TEXT("Touch"));
	Touch->InitSphereRadius(70.f);
	Touch->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Touch->SetCollisionResponseToAllChannels(ECR_Ignore);
	Touch->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // the look-at trace finds it, nothing bumps into it
	SetRootComponent(Touch);

	EMF = CreateDefaultSubobject<UFBParanormalSourceComponent>(TEXT("EMF"));
	EMF->SetupAttachment(Touch);
	EMF->EMFStrength = 4.2f;
	EMF->Radius = 1400.f; // the needle starts to move a good way off: follow it in
}

void AFBLastSeenSpot::SetQuiet(bool bQuiet)
{
	EMF->SetSourceActive(!bQuiet);
	Touch->SetCollisionEnabled(bQuiet ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

bool AFBLastSeenSpot::CanInteract_Implementation(AActor* InteractingActor) const
{
	return !bRead && InteractingActor && FVector::Dist(InteractingActor->GetActorLocation(), GetActorLocation()) < 350.f;
}

FText AFBLastSeenSpot::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	return PromptOverride.IsEmpty() ? NSLOCTEXT("FB", "TakeReading", "Take an EMF reading") : PromptOverride;
}

void AFBLastSeenSpot::Interact_Implementation(AActor* InteractingActor)
{
	if (bRead || !InteractingActor)
	{
		return;
	}
	bRead = true;
	EMF->EMFStrength = 1.5f; // read: only a trace left
	if (UFBCaseJournalComponent* Journal = InteractingActor->FindComponentByClass<UFBCaseJournalComponent>())
	{
		FFBJournalEntry Entry;
		Entry.Id = SpotId;
		Entry.Title = bNightSpot ? FText::Format(NSLOCTEXT("FB", "FoundTitle", "Found: {0}"), Person) : FText::Format(NSLOCTEXT("FB", "LastSeenTitle", "Last seen: {0}"), Person);
		Entry.Text = Reading;
		Entry.Type = EFBJournalEntryType::Evidence;
		Journal->AddEntry(Entry, false);
		Journal->AddClues(GrantsClues);
	}
	if (bNightSpot)
	{
		AFBHUD::ShowNotificationFor(InteractingActor, FText::Format(NSLOCTEXT("FB", "FoundNote", "You found {0}. (Journal updated)"), Person), 5.f);
		if (UFBCaseJournalComponent* Journal = InteractingActor->FindComponentByClass<UFBCaseJournalComponent>())
		{
			if (Journal->HasAllClues(TArray<FName>{ FName(TEXT("Clue_FoundDecker")), FName(TEXT("Clue_FoundBarlow")), FName(TEXT("Clue_FoundWhitmore")) }))
			{
				Journal->AddClue(TEXT("Clue_FoundAll"));
			}
		}
		return;
	}
	AFBHUD::ShowNotificationFor(InteractingActor, FText::Format(NSLOCTEXT("FB", "LastSeenNote", "EMF spike - {0} was taken here. (Journal updated)"), Person), 5.f);
}
