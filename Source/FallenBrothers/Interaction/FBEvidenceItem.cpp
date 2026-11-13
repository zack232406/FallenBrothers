// Fallen Brothers - Evidence item.

#include "Interaction/FBEvidenceItem.h"
#include "Core/FBRealModels.h"

#include "UI/FBHUD.h"
#include "Save/FBSaveSubsystem.h"
#include "Investigation/FBCaseJournalComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AFBEvidenceItem::AFBEvidenceItem()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Placeholder: small 20 cm cube.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.2f));
}

void AFBEvidenceItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	TInlineComponentArray<UStaticMeshComponent*> Old(this);
	for (UStaticMeshComponent* C : Old)
	{
		if (C && C->ComponentHasTag(TEXT("FBDress")))
		{
			C->DestroyComponent();
		}
	}
	if (ModelName.IsNone())
	{
		return;
	}
	TArray<UStaticMesh*> Parts = FBRealModels::FindAll<UStaticMesh>(TEXT("Props"), *ModelName.ToString());
	// A model imported as one combined mesh (Poly Haven FBX, "<name>_2k"): use just that, not the loose parts.
	for (UStaticMesh* P : Parts)
	{
		if (P && P->GetName().EndsWith(TEXT("_2k")))
		{
			Parts = { P };
			break;
		}
	}
	if (Parts.Num() == 0)
	{
		return;
	}
	FBox MB(ForceInit);
	for (const UStaticMesh* P : Parts)
	{
		MB += P->GetBoundingBox();
	}
	const float S = ModelSize / FMath::Max(1.f, float(MB.GetSize().GetMax()));
	// Small flat things (a watch, papers) are modelled standing up, face towards -Y: lay them down face up.
	FQuat Lay = FQuat::Identity;
	const FVector Ext = MB.GetSize();
	if (ModelSize <= 50.f && Ext.GetMin() < 0.3f * Ext.GetMax() && Ext.Z > Ext.GetMin() + KINDA_SMALL_NUMBER)
	{
		Lay = (Ext.Y <= Ext.X) ? FRotator(0.f, 0.f, -90.f).Quaternion() : FRotator(-90.f, 0.f, 0.f).Quaternion();
	}
	const FBox LB = MB.TransformBy(FTransform(Lay));
	// Lying on the floor / table under the (now invisible) cube: bottom at the cube's bottom.
	const FTransform T = GetActorTransform();
	FVector Bottom = T.GetLocation() - FVector(0.f, 0.f, 10.f);
	// Rest on the real surface (scanned floors are bumpy): trace down near the nominal spot, keep it only if it is close.
	if (UWorld* W = GetWorld())
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(FBEvidenceRest), true, this);
		FHitResult Hit;
		if (W->LineTraceSingleByChannel(Hit, Bottom + FVector(0.f, 0.f, 25.f), Bottom - FVector(0.f, 0.f, 25.f), ECC_Visibility, Q)
			&& FMath::Abs(Hit.ImpactPoint.Z - Bottom.Z) < 20.f)
		{
			Bottom.Z = Hit.ImpactPoint.Z + 0.5f;
		}
	}
	const FVector Offset = T.GetRotation().RotateVector(FVector(LB.GetCenter().X, LB.GetCenter().Y, LB.Min.Z) * S);
	for (UStaticMesh* P : Parts)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transactional);
		C->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		C->ComponentTags.Add(TEXT("FBDress"));
		C->SetStaticMesh(P);
		C->SetupAttachment(Mesh);
		C->SetUsingAbsoluteScale(true);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		C->SetWorldLocationAndRotation(Bottom - Offset, T.GetRotation() * Lay);
		C->SetWorldScale3D(FVector(S));
	}
	// The cube stays only as the (invisible) thing you aim at.
	Mesh->SetVisibility(false, false);
	Mesh->SetHiddenInGame(true, false);
}

bool AFBEvidenceItem::CanInteract_Implementation(AActor* InteractingActor) const
{
	return !bCollected;
}

FText AFBEvidenceItem::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	return FText::Format(NSLOCTEXT("FB", "EvidencePrompt", "Examine {0}"), DisplayName);
}

void AFBEvidenceItem::Interact_Implementation(AActor* InteractingActor)
{
	if (bCollected)
	{
		return;
	}
	bCollected = true;

	UE_LOG(LogTemp, Log, TEXT("Evidence collected: %s (%s)"), *DisplayName.ToString(), *EvidenceId.ToString());

	const FText Message = Description.IsEmpty()
		? FText::Format(NSLOCTEXT("FB", "EvidenceCollected", "Evidence collected: {0}"), DisplayName)
		: FText::Format(NSLOCTEXT("FB", "EvidenceCollectedDesc", "Evidence collected: {0}\n{1}"), DisplayName, Description);
	AFBHUD::ShowNotificationFor(InteractingActor, Message, 4.f);

	// Into the case journal.
	if (UFBCaseJournalComponent* Journal = InteractingActor ? InteractingActor->FindComponentByClass<UFBCaseJournalComponent>() : nullptr)
	{
		FFBJournalEntry Entry;
		Entry.Id = EvidenceId;
		Entry.Title = DisplayName;
		Entry.Text = Description;
		Entry.Type = EFBJournalEntryType::Evidence;
		Journal->AddEntry(Entry, /*bNotifyPlayer*/ false); // The pickup message is enough.
		Journal->AddClues(GrantsClues);
	}

	OnCollected.Broadcast(this, InteractingActor);

	if (bDestroyOnCollect)
	{
		UFBSaveSubsystem::NotifyActorRemoved(this);
		Destroy();
	}
}
