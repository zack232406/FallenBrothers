// Fallen Brothers - A pedestrian: the new townspeople. See FBPedestrian.h.

#include "Town/FBPedestrian.h"

#include "Town/FBStreetNetwork.h"
#include "Town/FBTownTalk.h"
#include "Town/FBAtmosphere.h"
#include "Dialogue/FBNPC.h"
#include "UI/FBHUD.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* GDefaultLocomotion = TEXT("/Game/FallenBrothers/Anims/MH/BS_MH_Locomotion.BS_MH_Locomotion");
	const TCHAR* GDefaultSit = TEXT("/Game/FallenBrothers/Anims/MH/MH_Sit.MH_Sit");
}

AFBPedestrian::AFBPedestrian()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(32.f, 90.f);
	// The interaction trace (Visibility) finds the person by their capsule.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetVisibility(false); // the look actor carries the visible body

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 200.f, 0.f);
	Move->MaxWalkSpeed = 135.f;
	Move->MaxAcceleration = 220.f;            // a few steps to get going
	Move->BrakingDecelerationWalking = 380.f; // and to stop
	Move->MaxStepHeight = 45.f;               // curbs, porch steps
	Move->bUseRVOAvoidance = true;            // walk around each other
	Move->AvoidanceConsiderationRadius = 260.f;
	Move->AvoidanceWeight = 0.5f;
	Move->SetWalkableFloorAngle(50.f);
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false); // they walk all over town: never stream out with one cell
#endif
}

void AFBPedestrian::BeginPlay()
{
	Super::BeginPlay();
	Random.Initialize(int32(GetUniqueID() * 2654435761u % 2147483647u));
	if (!Locomotion)
	{
		Locomotion = LoadObject<UBlendSpace>(nullptr, GDefaultLocomotion);
	}
	if (!SitAnim)
	{
		SitAnim = LoadObject<UAnimSequenceBase>(nullptr, GDefaultSit);
	}
	const float Pace = WalkSpeed > 0.f ? WalkSpeed : Random.FRandRange(112.f, 148.f);
	GetCharacterMovement()->MaxWalkSpeed = Pace;
	SpawnLook();

	if (bSeated)
	{
		GetCharacterMovement()->DisableMovement();
		bWander = false;
	}
	if (bWander)
	{
		if (UFBStreetNetwork* Net = UFBStreetNetwork::Get(this))
		{
			Net->EnsureBuilt();
			float T = 0.f;
			Strip = Net->NearestStrip(GetActorLocation(), 2500.f, &T);
			if (Strip != INDEX_NONE)
			{
				const FFBWalkStrip& S = Net->Strips()[Strip];
				bTowardsB = FVector::DotProduct(GetActorForwardVector(), (S.B - S.A).GetSafeNormal2D()) >= 0.f;
				Lateral = S.Width * Random.FRandRange(-0.3f, 0.3f);
				SetStripTarget();
			}
		}
		if (Strip == INDEX_NONE)
		{
			bWander = false;
		}
	}
	PauseLeft = Random.FRandRange(0.2f, 2.5f);
	StuckFrom = GetActorLocation();
}

void AFBPedestrian::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LookActor)
	{
		LookActor->Destroy();
		LookActor = nullptr;
	}
	if (TalkProxy)
	{
		TalkProxy->Destroy();
		TalkProxy = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AFBPedestrian::SpawnLook()
{
	UClass* LookClass = Look.LoadSynchronous();
	if (!LookClass || !GetWorld())
	{
		return;
	}
	FActorSpawnParameters P;
	P.Owner = this;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	LookActor = GetWorld()->SpawnActor<AActor>(LookClass, GetActorLocation() - FVector(0.f, 0.f, Half), GetActorRotation() + FRotator(0.f, -90.f, 0.f), P);
	if (!LookActor)
	{
		return;
	}
	LookActor->SetActorEnableCollision(false);
	LookActor->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
	LookActor->SetActorRelativeLocation(FVector(0.f, 0.f, -Half));
	LookActor->SetActorRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// The MetaHuman's body: the skeletal mesh the face, hair and clothes follow (named "Body"; else the biggest).
	TInlineComponentArray<USkeletalMeshComponent*> Skins(LookActor);
	for (USkeletalMeshComponent* S : Skins)
	{
		if (S->GetName().Equals(TEXT("Body"), ESearchCase::IgnoreCase))
		{
			Body = S;
		}
	}
	if (!Body)
	{
		int32 Best = -1;
		for (USkeletalMeshComponent* S : Skins)
		{
			const int32 Bones = S->GetNumBones();
			if (S->GetSkeletalMeshAsset() && Bones > Best)
			{
				Best = Bones;
				Body = S;
			}
		}
	}
	for (USkeletalMeshComponent* S : Skins)
	{
		S->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		S->bAllowCullDistanceVolume = false;
		S->LDMaxDrawDistance = 0.f;
	}
	if (Body)
	{
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (bSeated && SitAnim)
		{
			Body->PlayAnimation(SitAnim, true);
			bBodyOnSit = true;
		}
		else if (Locomotion)
		{
			Body->PlayAnimation(Locomotion, true);
		}
	}
}

void AFBPedestrian::UpdateBody()
{
	if (!Body || bBodyOnSit)
	{
		return;
	}
	if (UAnimSingleNodeInstance* Node = Body->GetSingleNodeInstance())
	{
		// The blend space picks idle / slow walk / walk from the real ground speed: no foot sliding.
		Node->SetBlendSpacePosition(FVector(GetVelocity().Size2D(), 0.f, 0.f));
	}
}

bool AFBPedestrian::IsTalking() const
{
	if (!TalkProxy || !GetWorld())
	{
		return false;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AFBHUD* HUD = PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr;
	return HUD && HUD->GetDialogueNPC() == TalkProxy;
}

void AFBPedestrian::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBody();

	if (IsTalking())
	{
		// Stop and face whoever is asking.
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const FVector To = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (!To.IsNearlyZero())
			{
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), DeltaSeconds, 5.f));
			}
		}
		PauseLeft = FMath::Max(PauseLeft, 1.5f);
		return;
	}
	if (bWander)
	{
		Walk(DeltaSeconds);
	}
}

void AFBPedestrian::SetStripTarget()
{
	UFBStreetNetwork* Net = UFBStreetNetwork::Get(this);
	if (!Net || !Net->Strips().IsValidIndex(Strip))
	{
		return;
	}
	const FFBWalkStrip& S = Net->Strips()[Strip];
	const FVector Dir = (S.B - S.A).GetSafeNormal2D();
	const FVector Right(-Dir.Y, Dir.X, 0.f);
	Target = (bTowardsB ? S.B : S.A) + Right * Lateral;
}

void AFBPedestrian::ArriveAtTarget()
{
	UFBStreetNetwork* Net = UFBStreetNetwork::Get(this);
	if (!Net || !Net->Strips().IsValidIndex(Strip))
	{
		bWander = false;
		return;
	}
	const FFBWalkStrip& S = Net->Strips()[Strip];
	bool bFromA = true;
	const int32 Next = Net->NextStrip(Strip, bTowardsB ? S.B : S.A, Random, bFromA);
	if (Next == INDEX_NONE)
	{
		bTowardsB = !bTowardsB; // dead end: back the way they came
	}
	else
	{
		Strip = Next;
		bTowardsB = bFromA;
		Lateral = Net->Strips()[Strip].Width * Random.FRandRange(-0.3f, 0.3f);
	}
	SetStripTarget();
	if (Random.FRand() < 0.3f)
	{
		PauseLeft = Random.FRandRange(1.5f, 5.f); // wait at the corner, look around
	}
}

void AFBPedestrian::Walk(float DeltaSeconds)
{
	if (PauseLeft > 0.f)
	{
		PauseLeft -= DeltaSeconds;
		StuckFrom = GetActorLocation();
		StuckCheck = 0.f;
		return;
	}
	const FVector At = GetActorLocation();
	FVector To = Target - At;
	To.Z = 0.f;
	if (To.Size() < 70.f)
	{
		ArriveAtTarget();
		return;
	}
	const FVector Dir = To.GetSafeNormal();

	// Somebody (the player) standing right in the way: stop and let them be.
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector P = Player->GetActorLocation() - At;
		if (FMath::Abs(P.Z) < 150.f && P.Size2D() < 150.f && FVector::DotProduct(P.GetSafeNormal2D(), Dir) > 0.75f)
		{
			PauseLeft = Random.FRandRange(0.8f, 1.8f);
			return;
		}
	}
	AddMovementInput(Dir, 1.f);

	// Wedged on something for a while: turn back.
	StuckCheck += DeltaSeconds;
	if (StuckCheck > 2.5f)
	{
		if (FVector::Dist2D(StuckFrom, At) < 40.f)
		{
			bTowardsB = !bTowardsB;
			SetStripTarget();
			PauseLeft = Random.FRandRange(0.5f, 1.5f);
		}
		StuckCheck = 0.f;
		StuckFrom = At;
	}
}

FText AFBPedestrian::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	if (!NPCName.IsEmpty())
	{
		return FText::Format(NSLOCTEXT("FB", "TalkToNamed", "Talk to {0}"), NPCName);
	}
	if (bAskedName && TalkProxy)
	{
		return FText::Format(NSLOCTEXT("FB", "TalkToNamed", "Talk to {0}"), TalkProxy->GetNPCName());
	}
	return NSLOCTEXT("FB", "TalkToTownsperson", "Talk to townsperson");
}

void AFBPedestrian::Interact_Implementation(AActor* InteractingActor)
{
	if (!TalkProxy && GetWorld())
	{
		const FTransform T(FRotator::ZeroRotator, GetActorLocation());
		TalkProxy = GetWorld()->SpawnActorDeferred<AFBNPC>(AFBNPC::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (TalkProxy)
		{
			if (StoryPreset.IsNone())
			{
				if (!Options.IsEmpty())
				{
					TalkProxy->ConfigureProxy(NPCName, NPCRole, Greeting, Farewell, Options);
				}
				else
				{
					float Hour = 12.f;
					for (TActorIterator<AFBAtmosphere> It(GetWorld()); It; ++It)
					{
						Hour = It->TimeOfDay;
						break;
					}
					const FBTownTalk::FPerson Person = FBTownTalk::Build(int32(GetUniqueID() * 2654435761u % 2147483647u), bFemale, Hour);
					TalkProxy->ConfigureProxy(Person.Name, Person.Role, Person.Greeting, Person.Farewell, Person.Options);
				}
				TalkProxy->FinishSpawning(T);
			}
			else
			{
				TalkProxy->FinishSpawning(T);
				TalkProxy->ConfigureStoryProxy(StoryPreset);
			}
		}
	}
	if (!TalkProxy)
	{
		return;
	}
	bAskedName = true;
	TalkProxy->SetActorLocation(GetActorLocation());
	TalkProxy->Interact_Implementation(InteractingActor);
}
