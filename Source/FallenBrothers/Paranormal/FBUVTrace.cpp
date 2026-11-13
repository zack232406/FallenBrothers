// Fallen Brothers - UV trace.

#include "Paranormal/FBUVTrace.h"

#include "Paranormal/FBParanormalSubsystem.h"
#include "Investigation/FBCaseJournalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBUVTrace::AFBUVTrace()
{
	PrimaryActorTick.bCanEverTick = true;
	// Update after the UV light has done its checks this frame.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Placeholder: engine plane (100x100, facing up), scaled to ~40 cm.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PlaneMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMaterial.Object);
	}

	Mesh->SetRelativeScale3D(FVector(0.4f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetHiddenInGame(true); // Visible in the editor, invisible in game until UV reveals it.
}

void AFBUVTrace::BeginPlay()
{
	Super::BeginPlay();

	DynamicMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	ApplyReveal();

	if (UFBParanormalSubsystem* Subsystem = GetWorld()->GetSubsystem<UFBParanormalSubsystem>())
	{
		Subsystem->RegisterUVTrace(this);
	}
}

void AFBUVTrace::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UFBParanormalSubsystem* Subsystem = World->GetSubsystem<UFBParanormalSubsystem>())
		{
			Subsystem->UnregisterUVTrace(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AFBUVTrace::NotifyLitByUV(float Strength)
{
	LitThisFrame = FMath::Max(LitThisFrame, FMath::Clamp(Strength, 0.f, 1.f));
}

void AFBUVTrace::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = LitThisFrame;
	const float Speed = (Target > Reveal) ? FadeInSpeed : FadeOutSpeed;
	Reveal = FMath::FInterpTo(Reveal, Target, DeltaSeconds, Speed);
	LitThisFrame = 0.f; // Must be lit again next frame to stay visible.

	if (Reveal > 0.5f && !bDiscovered)
	{
		bDiscovered = true;
		UE_LOG(LogTemp, Log, TEXT("UV trace discovered: %s"), *Description.ToString());

		// Into the player's journal.
		if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			if (UFBCaseJournalComponent* Journal = Player->FindComponentByClass<UFBCaseJournalComponent>())
			{
				FFBJournalEntry Entry;
				Entry.Id = GetFName();
				Entry.Title = Description;
				Entry.Text = NSLOCTEXT("FB", "UVTraceJournal", "Only visible under UV light. Something was here.");
				Entry.Type = EFBJournalEntryType::Trace;
				Journal->AddEntry(Entry);
				Journal->AddClues(GrantsClues);
			}
		}
	}

	ApplyReveal();
}

void AFBUVTrace::ApplyReveal()
{
	const bool bVisible = Reveal > 0.02f;
	Mesh->SetHiddenInGame(!bVisible);

	if (DynamicMaterial && bVisible)
	{
		// BasicShapeMaterial has a "Color" parameter. Darker = more faded.
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), GlowColor * Reveal);
	}
}
