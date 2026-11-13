// Fallen Brothers - Townsfolk on the Game Animation Sample locomotion. See FBTownsfolk.h.

#include "Town/FBTownsfolk.h"

#include "Town/FBStreetNetwork.h"
#include "Town/FBTownTalk.h"
#include "Town/FBAtmosphere.h"
#include "Dialogue/FBNPC.h"
#include "UI/FBHUD.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* GRetargetABP = TEXT("/Game/Blueprints/RetargetedCharacters/ABP_GenericRetarget.ABP_GenericRetarget_C");

	/** Sets every bool of a blueprint struct variable whose name starts with Prefix (BP struct members carry GUID suffixes). */
	void SetBlueprintStructBool(UObject* Obj, const TCHAR* StructVar, const TCHAR* Prefix, bool bValue)
	{
		FStructProperty* SP = FindFProperty<FStructProperty>(Obj->GetClass(), StructVar);
		if (!SP)
		{
			return;
		}
		void* Data = SP->ContainerPtrToValuePtr<void>(Obj);
		for (TFieldIterator<FProperty> It(SP->Struct); It; ++It)
		{
			if (It->GetName().StartsWith(Prefix))
			{
				if (FBoolProperty* B = CastField<FBoolProperty>(*It))
				{
					B->SetPropertyValue_InContainer(Data, bValue);
				}
			}
		}
	}

	void SetBlueprintVector(UObject* Obj, const TCHAR* Var, const FVector& Value)
	{
		FStructProperty* SP = FindFProperty<FStructProperty>(Obj->GetClass(), Var);
		if (SP && SP->Struct == TBaseStructure<FVector>::Get())
		{
			*SP->ContainerPtrToValuePtr<FVector>(Obj) = Value;
		}
	}
}

TArrayView<const AFBTownsfolk::FLookDef> AFBTownsfolk::Looks()
{
	// Checked in a lineup: these stand, walk and fit right. (Left out: realistic_woman_walking, old_russian_guy and
	// maxine - their skinning / retarget breaks the body; traffic_cop - broken bounds.)
	static const FLookDef GLooks[] =
	{
		{ TEXT("/Game/FallenBrothers/Characters_New/Businessman/Businessman_Rigged/SkeletalMeshes/Businessman_Rigged.Businessman_Rigged"), TEXT("RTG_N_Businessman_Rigged"), 172.f, 186.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/CasualManRigged/CasualManRigged/SkeletalMeshes/CasualManRigged.CasualManRigged"), TEXT("RTG_N_CasualManRigged"), 170.f, 185.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/EverydayJane/everyday_jane_casual_modern_woman_rigged/SkeletalMeshes/everyday_jane_casual_modern_woman_rigged.everyday_jane_casual_modern_woman_rigged"), TEXT("RTG_N_everyday_jane_casual_modern_woman_rigged"), 158.f, 170.f, true, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/FemalePoliceOfficer/female_police_officer_fully_rigged_3d_character/SkeletalMeshes/female_police_office_y_rigged_3d_character.female_police_office_y_rigged_3d_character"), TEXT("RTG_N_female_police_office_y_rigged_3d_charact"), 162.f, 172.f, true, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/JoeRealistic/JoeRealistic_Rigged/SkeletalMeshes/JoeRealistic_Rigged.JoeRealistic_Rigged"), TEXT("RTG_N_JoeRealistic_Rigged"), 172.f, 186.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/JustMan/JustMan/SkeletalMeshes/JustMan.JustMan"), TEXT("RTG_N_JustMan"), 172.f, 188.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/MaleInSuit/MaleInSuit/SkeletalMeshes/MaleInSuit.MaleInSuit"), TEXT("RTG_N_MaleInSuit"), 172.f, 188.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/ManInCoat/man_in_coat_-_human_rigged_model/SkeletalMeshes/man_in_coat_-_human_rigged_model.man_in_coat_-_human_rigged_model"), TEXT("RTG_N_man_in_coat_-_human_rigged_model"), 172.f, 186.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/MixedAdultMale/MixedAdultMale_Rigged/SkeletalMeshes/MixedAdultMale_Rigged.MixedAdultMale_Rigged"), TEXT("RTG_N_MixedAdultMale_Rigged"), 170.f, 186.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/OldManInCoat/old_man_in_coat_-_human_riged_model/SkeletalMeshes/old_man_in_coat_-_human_riged_model.old_man_in_coat_-_human_riged_model"), TEXT("RTG_N_old_man_in_coat_-_human_riged_model"), 164.f, 176.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/Characters_New/PoliceManUSA/police_man_from_the_u_s_a__t-pose_rig_mixamo/SkeletalMeshes/police_man_from_the_u_s_a__t-pose_rig_mixamo.police_man_from_the_u_s_a__t-pose_rig_mixamo"), TEXT("RTG_N_police_man_from_the_u_s_a__t-pose_rig_mi"), 175.f, 188.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/People/npc_sophia/npc_sophia/SkeletalMeshes/npc_sophia.npc_sophia"), TEXT("RTG_GASP_npc_sophia"), 160.f, 172.f, true, 0.f },
		{ TEXT("/Game/FallenBrothers/People/npc_carla/npc_carla/SkeletalMeshes/npc_carla.npc_carla"), TEXT("RTG_GASP_npc_carla"), 160.f, 172.f, true, 0.f },
		{ TEXT("/Game/FallenBrothers/People/npc_claudia/npc_claudia/SkeletalMeshes/npc_claudia.npc_claudia"), TEXT("RTG_GASP_npc_claudia"), 160.f, 172.f, true, 0.f },
		{ TEXT("/Game/FallenBrothers/People/npc_eric/npc_eric/SkeletalMeshes/npc_eric.npc_eric"), TEXT("RTG_GASP_npc_eric"), 172.f, 186.f, false, 0.f },
		{ TEXT("/Game/FallenBrothers/People/npc_old_man/npc_old_man/SkeletalMeshes/npc_old_man.npc_old_man"), TEXT("RTG_GASP_npc_old_man"), 164.f, 176.f, false, 0.f },
	};
	return TArrayView<const FLookDef>(GLooks, UE_ARRAY_COUNT(GLooks));
}

AFBTownsfolk::AFBTownsfolk()
{
	// Kept minimal on purpose: the Game Animation Sample blueprint on top owns the components and their settings.
	PrimaryActorTick.bCanEverTick = true;
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false); // they walk all over town: never stream out with one cell
#endif
}

void AFBTownsfolk::BeginPlay()
{
	Super::BeginPlay(); // the blueprint's own BeginPlay (movement setup, foley, visual override manager)

	Random.Initialize(int32(GetUniqueID() * 2654435761u % 2147483647u));
	if (!Controller)
	{
		AIControllerClass = AAIController::StaticClass();
		SpawnDefaultController(); // a plain AI controller: this class does the thinking
	}
	// The interaction trace (Visibility) finds the person by the capsule.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetUpGaspMovement();
	ApplyLook();

	if (FBWander)
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
			FBWander = false;
		}
	}
	PauseLeft = Random.FRandRange(0.6f, 2.5f); // stand a moment first (the look's height fit runs meanwhile)
	StuckFrom = GetActorLocation();
}

void AFBTownsfolk::SetUpGaspMovement()
{
	// A townsperson walks (the sample's character jogs by default) at an everyday pace.
	const float Pace = FBWalkPace > 0.f ? FBWalkPace : Random.FRandRange(135.f, 175.f);
	SetBlueprintStructBool(this, TEXT("CharacterInputState"), TEXT("WantsToWalk"), true);
	SetBlueprintStructBool(this, TEXT("CharacterInputState"), TEXT("WantsToSprint"), false);
	SetBlueprintVector(this, TEXT("WalkSpeeds"), FVector(Pace, Pace * 0.85f, Pace * 0.75f));

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = Pace;
	Move->bUseRVOAvoidance = true; // walk around each other
	Move->AvoidanceConsiderationRadius = 260.f;
	Move->AvoidanceWeight = 0.5f;
	Move->SetAvoidanceEnabled(true);
}

void AFBTownsfolk::ApplyLook()
{
	USkeletalMeshComponent* Driver = GetMesh(); // the sample's (hidden) mannequin: motion matching + foot IK
	if (!Driver)
	{
		return;
	}
	Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (UClass* ActorLook = FBLookActor.LoadSynchronous())
	{
		// A whole MetaHuman (body, face, hair, clothes) through the sample's visual override slot.
		TInlineComponentArray<UChildActorComponent*> Slots(this);
		for (UChildActorComponent* Slot : Slots)
		{
			if (Slot->GetName() == TEXT("VisualOverride"))
			{
				Slot->SetChildActorClass(ActorLook);
				Driver->SetVisibility(false, false);
				break;
			}
		}
		return;
	}

	USkeletalMesh* LookMesh = FBLookMesh.LoadSynchronous();
	UClass* RetargetABP = StaticLoadClass(UAnimInstance::StaticClass(), nullptr, GRetargetABP);
	if (!LookMesh || !RetargetABP || FBLookRetargeter.IsNone())
	{
		return; // no look: the mannequin stays visible
	}
	LookBody = NewObject<USkeletalMeshComponent>(this, TEXT("FBLookBody"));
	LookBody->SetupAttachment(Driver);
	LookBody->ComponentTags.Add(FBLookRetargeter); // ABP_GenericRetarget picks the IK Retargeter by this tag
	LookBody->SetSkeletalMeshAsset(LookMesh);
	LookBody->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	LookBody->SetAnimInstanceClass(RetargetABP);
	LookBody->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	LookBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LookBody->SetCanEverAffectNavigation(false);
	LookBody->bAllowCullDistanceVolume = false;
	LookBody->SetBoundsScale(2.f);
	LookBody->SetRelativeRotation(FRotator(0.f, FBLookYaw, 0.f));
	// First guess at the size from the model's bounds; FitLook measures the posed skeleton and corrects it.
	const FVector Ext = LookMesh->GetBounds().BoxExtent;
	const float Guess = FBHeight / FMath::Max(1.f, 2.f * FMath::Max3(float(Ext.X), float(Ext.Y), float(Ext.Z)));
	LookBody->SetRelativeScale3D(FVector(Guess));
	LookBody->RegisterComponent();
	Driver->SetVisibility(false, false);
	bFitDone = false;
	FitRuns = 0;
	FitWait = 0.f;
}

void AFBTownsfolk::FitLook(float DeltaSeconds)
{
	if (bFitDone || !LookBody)
	{
		return;
	}
	FitWait += DeltaSeconds;
	if (FitWait < 0.15f)
	{
		return;
	}
	FitWait = 0.f;
	const int32 N = LookBody->GetNumBones();
	FBox Bones(ForceInit);
	for (int32 I = 0; I < N; ++I)
	{
		Bones += LookBody->GetBoneTransform(I).GetLocation();
	}
	const FVector Size = Bones.GetSize();
	if (N < 2 || Size.Z < 1.f || Size.Z < FMath::Max(Size.X, Size.Y) * 0.6f)
	{
		if (++FitRuns > 20)
		{
			// The retarget never stood the model up (missing / wrong retargeter): show the mannequin instead.
			UE_LOG(LogTemp, Warning, TEXT("FBTownsfolk: look %s did not stand up (retargeter %s) - mannequin shown."),
				*GetNameSafe(LookBody->GetSkeletalMeshAsset()), *FBLookRetargeter.ToString());
			LookBody->DestroyComponent();
			LookBody = nullptr;
			GetMesh()->SetVisibility(true, false);
			bFitDone = true;
		}
		return;
	}
	const float Factor = FBHeight / (Size.Z * 1.07f); // the top of the head is a little above the last bone
	if (FMath::Abs(Factor - 1.f) > 0.015f && FitRuns < 12)
	{
		LookBody->SetRelativeScale3D(LookBody->GetRelativeScale3D() * Factor);
		++FitRuns;
		return;
	}
	// The right size: feet on the ground (the lowest bones are the ankles / toes, a few cm up). Some models'
	// retarget poses put the hips too low (the body sinks into the street): lift them until the feet touch.
	const float Floor = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Lift = Floor + 4.f - Bones.Min.Z;
	if (FMath::Abs(Lift) > 2.5f && FMath::Abs(Lift) < 250.f && FitRuns < 18)
	{
		LookBody->AddWorldOffset(FVector(0.f, 0.f, Lift));
		++FitRuns;
		return; // measure again next time
	}
	bFitDone = true;
}

void AFBTownsfolk::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TalkProxy)
	{
		TalkProxy->Destroy();
		TalkProxy = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool AFBTownsfolk::IsTalking() const
{
	if (!TalkProxy || !GetWorld())
	{
		return false;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AFBHUD* HUD = PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr;
	return HUD && HUD->GetDialogueNPC() == TalkProxy;
}

void AFBTownsfolk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FitLook(DeltaSeconds);

	if (TalkProxy)
	{
		// The interview camera looks at the face.
		const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		TalkProxy->SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, FBHeight * 0.93f - Half - 75.f));
	}
	if (IsTalking())
	{
		// Stop and turn to whoever is asking (the sample's animation turns in place on its own).
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const FVector To = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (!To.IsNearlyZero())
			{
				SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), To.Rotation(), DeltaSeconds, 140.f));
			}
		}
		bWasTalking = true;
		PauseLeft = FMath::Max(PauseLeft, 1.5f);
		return;
	}
	if (bWasTalking)
	{
		bWasTalking = false;
		PauseLeft = Random.FRandRange(1.f, 2.5f);
	}
	if (FBWander)
	{
		Walk(DeltaSeconds);
	}
}

void AFBTownsfolk::SetStripTarget()
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

void AFBTownsfolk::ArriveAtTarget()
{
	UFBStreetNetwork* Net = UFBStreetNetwork::Get(this);
	if (!Net || !Net->Strips().IsValidIndex(Strip))
	{
		FBWander = false;
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

void AFBTownsfolk::Walk(float DeltaSeconds)
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
	if (To.Size() < 80.f)
	{
		ArriveAtTarget();
		return;
	}
	const FVector Dir = To.GetSafeNormal();

	// The player standing right in the way: stop and let them be.
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector P = Player->GetActorLocation() - At;
		if (FMath::Abs(P.Z) < 150.f && P.Size2D() < 160.f && FVector::DotProduct(P.GetSafeNormal2D(), Dir) > 0.75f)
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

FText AFBTownsfolk::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	if (!FBName.IsEmpty())
	{
		return FText::Format(NSLOCTEXT("FB", "TalkToNamed", "Talk to {0}"), FBName);
	}
	if (bAskedName && TalkProxy)
	{
		return FText::Format(NSLOCTEXT("FB", "TalkToNamed", "Talk to {0}"), TalkProxy->GetNPCName());
	}
	return NSLOCTEXT("FB", "TalkToTownsperson", "Talk to townsperson");
}

void AFBTownsfolk::Interact_Implementation(AActor* InteractingActor)
{
	if (!TalkProxy && GetWorld())
	{
		const FTransform T(FRotator::ZeroRotator, GetActorLocation());
		TalkProxy = GetWorld()->SpawnActorDeferred<AFBNPC>(AFBNPC::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (TalkProxy)
		{
			if (FBStoryPreset.IsNone())
			{
				if (!FBOptions.IsEmpty())
				{
					TalkProxy->ConfigureProxy(FBName, FBRole, FBGreeting, FBFarewell, FBOptions);
				}
				else
				{
					float Hour = 12.f;
					for (TActorIterator<AFBAtmosphere> It(GetWorld()); It; ++It)
					{
						Hour = It->TimeOfDay;
						break;
					}
					const FBTownTalk::FPerson Person = FBTownTalk::Build(int32(GetUniqueID() * 2654435761u % 2147483647u), FBFemale, Hour);
					TalkProxy->ConfigureProxy(Person.Name, Person.Role, Person.Greeting, Person.Farewell, Person.Options);
				}
				TalkProxy->FinishSpawning(T);
			}
			else
			{
				TalkProxy->FinishSpawning(T);
				TalkProxy->ConfigureStoryProxy(FBStoryPreset);
			}
		}
	}
	if (!TalkProxy)
	{
		return;
	}
	bAskedName = true;
	TalkProxy->Interact_Implementation(InteractingActor);
}
