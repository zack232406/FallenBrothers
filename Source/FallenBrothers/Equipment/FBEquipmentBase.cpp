// Fallen Brothers - Equipment base class.

#include "Equipment/FBEquipmentBase.h"

#include "Core/FBRealModels.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "UI/FBHUD.h"

AFBEquipmentBase::AFBEquipmentBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// After the bodycam has placed the camera for this frame.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SetActorEnableCollision(false);
}

void AFBEquipmentBase::BeginPlay()
{
	Super::BeginPlay();

	// Swap the grey shapes for the real (downloaded) model, if it has been imported.
	if (!ModelName.IsEmpty())
	{
		const FRotator Rot = (FRotator(FQuat(ModelRotation) * FQuat(FBRealModels::LongestToForward(TEXT("Items"), *ModelName))));
		if (FBRealModels::AttachModel(this, Root, TEXT("Items"), *ModelName, ModelLength, Rot, ModelOffset))
		{
			FBRealModels::HideGreyShapes(this);
		}
	}
}

void AFBEquipmentBase::SetHolder(USceneComponent* InHolder)
{
	Holder = InHolder;
	bHasSmoothedRotation = false;
}

void AFBEquipmentBase::OnEquipped()
{
	bEquipped = true;
	bHasSmoothedRotation = false;
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
}

void AFBEquipmentBase::OnUnequipped()
{
	bEquipped = false;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
}

void AFBEquipmentBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Holder)
	{
		return;
	}

	const FTransform HolderTransform = Holder->GetComponentTransform();
	const FRotator TargetRotation = HolderTransform.Rotator();

	if (!bHasSmoothedRotation)
	{
		SmoothedRotation = TargetRotation;
		bHasSmoothedRotation = true;
	}
	SmoothedRotation = FMath::RInterpTo(SmoothedRotation, TargetRotation, DeltaSeconds, AimLagSpeed);

	// Position is rigid (no drifting), only the aim lags.
	SetActorLocationAndRotation(HolderTransform.TransformPosition(HoldOffset), SmoothedRotation);
}

float AFBEquipmentBase::DrawRightAlignedText(AFBHUD* HUD, const FString& Text, float RightX, float Y, const FLinearColor& Color, float Scale) const
{
	UFont* Font = GEngine->GetMediumFont();
	float W = 0.f, H = 0.f;
	HUD->GetTextSize(Text, W, H, Font, Scale);
	HUD->DrawText(Text, Color, RightX - W, Y, Font, Scale);
	return H;
}

void AFBEquipmentBase::DrawEquipmentHUD(AFBHUD* HUD, float UIScale)
{
	if (!HUD || !HUD->HasCanvas())
	{
		return;
	}

	const float RightX = HUD->GetScreenSize().X - 40.f * UIScale;
	const float Y = HUD->GetScreenSize().Y - 70.f * UIScale;
	DrawRightAlignedText(HUD, DisplayName.ToString().ToUpper(), RightX, Y, FLinearColor(1.f, 1.f, 1.f, 0.8f), 1.1f * UIScale);
}
