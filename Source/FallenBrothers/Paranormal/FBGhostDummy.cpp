// Fallen Brothers - Test ghost.

#include "Paranormal/FBGhostDummy.h"

#include "Paranormal/FBParanormalSourceComponent.h"
#include "UI/FBHUD.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AFBGhostDummy::AFBGhostDummy()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Placeholder body: a pale human-sized cylinder (50 cm wide, 180 cm tall).
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Body->SetMaterial(0, BasicMaterial.Object);
	}
	Body->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.8f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, 90.f)); // Actor origin on the floor.
	Body->SetCastShadow(false);

	// Incorporeal: nothing collides with it, but physics objects and the world are "felt" (overlap).
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Overlap);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetGenerateOverlapEvents(true);

	EMFSource = CreateDefaultSubobject<UFBParanormalSourceComponent>(TEXT("EMFSource"));
	EMFSource->SetupAttachment(Body);
	EMFSource->EMFStrength = 5.f;
	EMFSource->Radius = 500.f;
}

void AFBGhostDummy::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.75f, 0.85f));
	}

	Body->OnComponentBeginOverlap.AddDynamic(this, &AFBGhostDummy::OnBodyOverlap);
}

void AFBGhostDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FloatTime += DeltaSeconds;
	Body->SetRelativeLocation(FVector(0.f, 0.f, 90.f + FMath::Sin(FloatTime * 1.3f) * FloatAmplitude));
}

void AFBGhostDummy::OnBodyOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                  int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bDispersed || !OtherActor || !OtherComp || !OtherComp->IsSimulatingPhysics())
	{
		return;
	}

	const IFBImpactSource* Source = Cast<IFBImpactSource>(OtherActor);
	if (!Source)
	{
		return;
	}

	const float Speed = OtherComp->GetPhysicsLinearVelocity().Size();
	if (Speed < MinImpactSpeed)
	{
		return;
	}

	FFBHitInfo Hit;
	Hit.Kind = Source->GetImpactKind();
	Hit.Amount = Source->GetImpactMassKg() * Speed * 0.01f;
	Hit.Causer = OtherActor;
	Hit.Location = OtherComp->GetComponentLocation();
	ApplyHit(Hit);
}

void AFBGhostDummy::ApplyHit(const FFBHitInfo& Hit)
{
	if (bDispersed)
	{
		return;
	}

	if (Weaknesses.Contains(Hit.Kind))
	{
		UE_LOG(LogTemp, Log, TEXT("Ghost hit by weakness (%s) - dispersing."), *UEnum::GetValueAsString(Hit.Kind));
		Disperse();
		return;
	}

	// Not a weakness: it passes right through. Don't spam the message.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastNoEffectMessageTime > 3.f)
	{
		LastNoEffectMessageTime = Now;
		Notify(NSLOCTEXT("FB", "GhostNoEffect", "It passed right through it..."));
	}
}

void AFBGhostDummy::Disperse()
{
	bDispersed = true;
	SetActorHiddenInGame(true);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EMFSource->SetSourceActive(false);

	Notify(NSLOCTEXT("FB", "GhostDispersed", "The apparition dispersed!"));

	GetWorldTimerManager().SetTimer(ReformTimer, this, &AFBGhostDummy::Reform, DisperseTime, false);
}

void AFBGhostDummy::Reform()
{
	bDispersed = false;
	SetActorHiddenInGame(false);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EMFSource->SetSourceActive(true);
}

void AFBGhostDummy::Notify(const FText& Message) const
{
	AFBHUD::ShowNotificationFor(UGameplayStatics::GetPlayerPawn(this, 0), Message, 2.5f);
}
