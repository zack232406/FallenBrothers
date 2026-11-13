// Fallen Brothers - A townsperson.
//
// Walkers follow the town's sidewalks: to the end of the block, then round the corner, straight on across the
// side street or over the road. Now and then they stop to look around. They keep to the right, slow down behind
// someone slower, step round people coming the other way, and wait when you stand in their way.
// Benches get people sitting on them. Anyone can be questioned (F): see FBTownTalk.

#include "Town/FBTownsperson.h"

#include "Core/FBRealModels.h"
#include "Dialogue/FBNPC.h"
#include "Town/FBFootsteps.h"
#include "Town/FBStreetNetwork.h"
#include "Town/FBTownTalk.h"
#include "Town/FBAtmosphere.h"
#include "EngineUtils.h"
#include "Town/FBWeather.h"
#include "UI/FBHUD.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AFBTownsperson::AFBTownsperson()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	if (USkeletalMeshComponent* Skin = GetSkeletalMeshComponent())
	{
		Skin->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Never pop out of existence: no draw-distance or cull-volume culling, and a roomier culling box
		// (the imported bounds of these rigs are their lying-down bind pose, which made standing people
		// vanish when the box went off-screen or behind a curb).
		Skin->LDMaxDrawDistance = 0.f;
		Skin->bAllowCullDistanceVolume = false;
		Skin->SetBoundsScale(1.6f);
	}
#if WITH_EDITOR
	// Townspeople walk all over town: keep them loaded with the level instead of streaming with one cell.
	SetIsSpatiallyLoaded(false);
#endif
	// A person-sized capsule: the player bumps into it and the interaction trace finds it.
	TalkCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TalkCapsule"));
	TalkCapsule->SetupAttachment(GetRootComponent());
	TalkCapsule->SetUsingAbsoluteLocation(true);
	TalkCapsule->SetUsingAbsoluteRotation(true);
	TalkCapsule->SetUsingAbsoluteScale(true);
	TalkCapsule->InitCapsuleSize(32.f, 88.f);
	TalkCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TalkCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	TalkCapsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TalkCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	TalkCapsule->SetGenerateOverlapEvents(false);
	TalkCapsule->SetCanEverAffectNavigation(false);
}

namespace
{
	/** An animation made for this person's rig: <mesh folder>/<mesh>_<kind> (walk, idle, look, sit). */
	UAnimationAsset* FindPersonAnim(const USkeletalMesh* Mesh, const TCHAR* Kind)
	{
		if (!Mesh)
		{
			return nullptr;
		}
		const FString Folder = FPaths::GetPath(Mesh->GetPathName());
		const FString Name = FString::Printf(TEXT("%s_%s"), *Mesh->GetName(), Kind);
		UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		return (Anim && Anim->GetSkeleton() == Mesh->GetSkeleton()) ? Anim : nullptr;
	}

	bool LooksFemale(const USkeletalMesh* Mesh)
	{
		const FString N = Mesh ? Mesh->GetName().ToLower() : FString();
		for (const TCHAR* K : { TEXT("woman"), TEXT("jane"), TEXT("maxine"), TEXT("female"), TEXT("carla"), TEXT("claudia"), TEXT("sophia"), TEXT("martha"), TEXT("lady"), TEXT("traffic_cop"), TEXT("mother") })
		{
			if (N.Contains(K))
			{
				return true;
			}
		}
		return false;
	}

	constexpr float WalkAnimSpeed = 130.f; // cm/s the walk cycles were made for
}

void AFBTownsperson::BeginPlay()
{
	Super::BeginPlay();
	USkeletalMeshComponent* Skin = GetSkeletalMeshComponent();
	if (!Skin)
	{
		return;
	}
	if (!bRuntimeSpawned)
	{
		// The generator spawns everyone turned with yaw + 90, which leaves these glTF people (they face +Y)
		// looking away from the street. Turn them round so they face the road / where they walk.
		SetActorRotation(FRotator(0.f, GetActorRotation().Yaw + 180.f, 0.f));
	}

	// A MetaHuman look (face, hair, clothes) dressed onto this body: every part follows our animated body.
	if (UClass* LookClass = Look.LoadSynchronous())
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AActor* L = GetWorld()->SpawnActor<AActor>(LookClass, Skin->GetComponentTransform(), Params))
		{
			TInlineComponentArray<USkinnedMeshComponent*> Parts(L);
			USkeletalMeshComponent* Body = nullptr;
			for (USkinnedMeshComponent* Part : Parts)
			{
				USkeletalMeshComponent* SK = Cast<USkeletalMeshComponent>(Part);
				if (SK && SK->GetSkeletalMeshAsset() == Skin->GetSkeletalMeshAsset())
				{
					Body = SK;
					break;
				}
			}
			if (Body)
			{
				const FTransform BodyInLook = Body->GetComponentTransform().GetRelativeTransform(L->GetActorTransform());
				L->SetActorTransform(BodyInLook.Inverse() * Skin->GetComponentTransform());
				for (int32 i = 0; i < Body->GetNumMaterials(); ++i)
				{
					Skin->SetMaterial(i, Body->GetMaterial(i));
				}
				Body->SetVisibility(false, false);
			}
			L->AttachToComponent(Skin, FAttachmentTransformRules::KeepWorldTransform);
			for (USkinnedMeshComponent* Part : Parts)
			{
				if (Part != Body)
				{
					Part->SetLeaderPoseComponent(Skin, true);
				}
			}
		}
	}

	const USkeletalMesh* Mesh = Skin->GetSkeletalMeshAsset();
	WalkAnim = FindPersonAnim(Mesh, TEXT("walk"));
	IdleAnim = FindPersonAnim(Mesh, TEXT("idle"));
	if (!IdleAnim)
	{
		IdleAnim = Skin->AnimationData.AnimToPlay; // the generator's people come with their idle set
	}
	LookAnim = FindPersonAnim(Mesh, TEXT("look"));
	SitAnim = FindPersonAnim(Mesh, TEXT("sit"));

	Random.Initialize(int32(GetUniqueID() * 7919 + int32(GetActorLocation().X) + int32(GetActorLocation().Y) * 13));
	Yaw = GetActorRotation().Yaw + 90.f; // the mesh faces the actor's +Y
	CurrentGround = GroundZ;
	Speed = Random.FRandRange(112.f, 148.f);
	if (Mesh && Mesh->GetName().ToLower().Contains(TEXT("old")))
	{
		Speed = Random.FRandRange(86.f, 106.f); // the old folks take their time
	}
	ShownGround = GroundZ;

	// Standing upright first, so the height fit measures a standing person (sitters sit down after it).
	PlayAnim(IdleAnim);
	{
		const float Ground = GroundZ;
		FBRealModels::FitSkeletonToHeight(Skin, GetRootComponent(), TargetHeight, [Ground]() { return Ground; });
	}

	if (bSeated)
	{
		Mode = EMode::Sit;
	}
	else if (bStandStill || !WalkAnim || (!bRuntimeSpawned && Random.FRand() > 0.9f))
	{
		Mode = EMode::Stand;
	}
	else
	{
		UFBStreetNetwork* Net = UFBStreetNetwork::Get(this);
		if (Net)
		{
			Net->EnsureBuilt();
		}
		float T = 0.f;
		const int32 Near = (Net && bWalkTheTown) ? Net->NearestStrip(GetActorLocation(), 1200.f, &T) : INDEX_NONE;
		if (Near != INDEX_NONE)
		{
			Strip = Near;
			bOnStrip = true;
			bTowardsB = Random.FRand() < 0.5f;
			Lateral = Net->Strips()[Strip].Width * Random.FRandRange(0.08f, 0.3f);
			SetStripTarget(true);
		}
		else
		{
			// Old style: up and down this stretch of sidewalk.
			bStroll = true;
			Home = GetActorLocation();
			const FVector F = FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
			Along = FMath::Abs(F.X) > FMath::Abs(F.Y) ? FVector(FMath::Sign(F.X), 0.f, 0.f) : FVector(0.f, FMath::Sign(F.Y), 0.f);
			Reach = Random.FRandRange(700.f, 1600.f);
			Side = Random.FRand() < 0.5f ? 1.f : -1.f;
			Target = Home + Along * Side * Reach;
		}
		Mode = EMode::Pause;
		Wait = Random.FRandRange(2.2f, 5.f); // the height fit runs during the first ~2 s
	}
	if (UFBStreetNetwork* Net = UFBStreetNetwork::Get(this))
	{
		Net->Register(this);
	}
	TalkCapsule->SetWorldLocationAndRotation(FVector(GetActorLocation().X, GetActorLocation().Y, CurrentGround + 90.f), FRotator::ZeroRotator);
}

void AFBTownsperson::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UFBStreetNetwork* Net = UFBStreetNetwork::Get(this))
	{
		Net->Unregister(this);
	}
	if (TalkProxy)
	{
		TalkProxy->Destroy();
		TalkProxy = nullptr;
	}
	Super::EndPlay(Reason);
}

void AFBTownsperson::PlayAnim(UAnimationAsset* Anim, float Rate, bool bRandomStart)
{
	USkeletalMeshComponent* Skin = GetSkeletalMeshComponent();
	if (!Skin)
	{
		return;
	}
	// PlayAnimation hands the asset to the single-node player. (OverrideAnimationData only stores it for the
	// next re-init, which never comes for people spawned at runtime - they stood frozen in their bind pose.)
	if (!Anim)
	{
		if (WalkAnim)
		{
			Skin->PlayAnimation(WalkAnim, false); // frozen mid-step beats a T-pose
			Skin->SetPosition(0.3f, false);
			Skin->Stop();
		}
		return;
	}
	float Start = 0.f;
	if (bRandomStart)
	{
		if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Anim))
		{
			Start = Random.FRandRange(0.f, Seq->GetPlayLength() * 0.9f);
		}
	}
	Skin->PlayAnimation(Anim, true);
	Skin->SetPlayRate(Rate);
	if (Start > 0.f)
	{
		Skin->SetPosition(Start, false);
	}
}

void AFBTownsperson::Pause(float Seconds, bool bLookAround)
{
	Mode = EMode::Pause;
	Wait = Seconds;
	WalkVelocity = FVector::ZeroVector;
	CurSpeed = 0.f;
	PlayAnim(bLookAround && LookAnim ? LookAnim.Get() : IdleAnim.Get());
}

void AFBTownsperson::StartWalking()
{
	Mode = EMode::Walk;
	CurSpeed = Speed * 0.35f; // first steps are slower
	PlayAnim(WalkAnim, CurSpeed / WalkAnimSpeed);
}

void AFBTownsperson::SetStripTarget(bool bToEnd)
{
	const UFBStreetNetwork* Net = UFBStreetNetwork::Get(this);
	if (!Net || !Net->Strips().IsValidIndex(Strip))
	{
		return;
	}
	const FFBWalkStrip& S = Net->Strips()[Strip];
	const FVector D = ((S.B - S.A) * (bTowardsB ? 1.f : -1.f)).GetSafeNormal2D();
	const FVector Right(-D.Y, D.X, 0.f);
	const FVector P = bToEnd ? (bTowardsB ? S.B : S.A) : (bTowardsB ? S.A : S.B);
	Target = P + Right * Lateral;
}

void AFBTownsperson::ArriveAtTarget()
{
	UFBStreetNetwork* Net = UFBStreetNetwork::Get(this);
	if (bStroll || !Net || !Net->Strips().IsValidIndex(Strip))
	{
		Pause(Random.FRandRange(2.f, 8.f), Random.FRand() < 0.5f);
		Side = -Side;
		Target = Home + Along * Side * Reach * Random.FRandRange(0.6f, 1.f);
		return;
	}
	if (bOnStrip)
	{
		// End of the block: round the corner, on across the side street, or over the road.
		const FFBWalkStrip& S = Net->Strips()[Strip];
		const FVector End = bTowardsB ? S.B : S.A;
		bool bFromA = true;
		const int32 Next = Net->NextStrip(Strip, End, Random, bFromA);
		if (Next == INDEX_NONE)
		{
			bTowardsB = !bTowardsB; // dead end: back the way they came
			SetStripTarget(true);
		}
		else
		{
			Strip = Next;
			bTowardsB = bFromA;
			bOnStrip = false;
			Lateral = Net->Strips()[Strip].Width * Random.FRandRange(0.08f, 0.3f);
			SetStripTarget(false);
		}
		if (Random.FRand() < 0.3f)
		{
			Pause(Random.FRandRange(1.5f, 5.f), true); // look both ways / wait at the corner
		}
	}
	else
	{
		bOnStrip = true;
		SetStripTarget(true);
		PauseAtT = Random.FRand() < 0.22f ? Random.FRandRange(0.2f, 0.8f) : -1.f;
	}
}

void AFBTownsperson::UpdateGround(const FVector& Dir)
{
	// Under the feet, and a stride ahead: a curb coming up is stepped onto as they reach it, not after.
	const FVector At = GetActorLocation();
	float Under = CurrentGround;
	const bool bUnder = UFBStreetNetwork::GroundAt(GetWorld(), FVector(At.X, At.Y, CurrentGround), Under, this);
	float Ahead = Under;
	const bool bAhead = UFBStreetNetwork::GroundAt(GetWorld(), FVector(At.X, At.Y, CurrentGround) + Dir * 28.f, Ahead, this);
	if (!bUnder && !bAhead)
	{
		return;
	}
	float Want = bUnder ? Under : Ahead;
	if (bAhead && Ahead > Want + 3.f && Ahead - Want < 45.f)
	{
		Want = Ahead; // stepping up onto the curb
	}
	const float Delta = Want - CurrentGround;
	if (FMath::Abs(Delta) < 0.5f || FMath::Abs(Delta) > 90.f)
	{
		return;
	}
	if (Delta > 6.f)
	{
		StepSlow = 0.35f; // a little hitch in the stride going up
	}
	CurrentGround = Want;
}

void AFBTownsperson::SmoothGround(float Dt)
{
	// Up a curb in ~0.15 s (a step), down a little quicker; small bumps just follow.
	const float Rate = CurrentGround > ShownGround ? 110.f : 150.f;
	const float NewShown = FMath::Abs(CurrentGround - ShownGround) < 2.f ? CurrentGround
		: FMath::FInterpConstantTo(ShownGround, CurrentGround, Dt, Rate);
	if (NewShown != ShownGround)
	{
		AddActorWorldOffset(FVector(0.f, 0.f, NewShown - ShownGround));
		ShownGround = NewShown;
	}
}

bool AFBTownsperson::IsTalking() const
{
	if (!TalkProxy || !GetWorld())
	{
		return false;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AFBHUD* HUD = PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr;
	return HUD && HUD->GetDialogueNPC() == TalkProxy;
}

FText AFBTownsperson::DisplayName() const
{
	return (bAskedName && TalkProxy) ? TalkProxy->GetNPCName() : NSLOCTEXT("FB", "Townsperson", "townsperson");
}

FText AFBTownsperson::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	return FText::Format(NSLOCTEXT("FB", "QuestionPerson", "Talk to {0}"), DisplayName());
}

void AFBTownsperson::Interact_Implementation(AActor* InteractingActor)
{
	USkeletalMeshComponent* Skin = GetSkeletalMeshComponent();
	if (!TalkProxy && GetWorld())
	{
		float Hour = 12.f;
		for (TActorIterator<AFBAtmosphere> It(GetWorld()); It; ++It)
		{
			Hour = It->TimeOfDay;
			break;
		}
		const FBTownTalk::FPerson Person = FBTownTalk::Build(int32(GetUniqueID() * 2654435761u % 2147483647u), LooksFemale(Skin ? Skin->GetSkeletalMeshAsset() : nullptr), Hour);
		const FTransform T(FRotator::ZeroRotator, GetActorLocation());
		TalkProxy = GetWorld()->SpawnActorDeferred<AFBNPC>(AFBNPC::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (TalkProxy)
		{
			TalkProxy->ConfigureProxy(Person.Name, Person.Role, Person.Greeting, Person.Farewell, Person.Options);
			TalkProxy->FinishSpawning(T);
		}
	}
	if (!TalkProxy)
	{
		return;
	}
	bAskedName = true;
	const FVector At = GetActorLocation();
	TalkProxy->SetActorLocation(FVector(At.X, At.Y, CurrentGround + TargetHeight * (bSeated ? 0.68f : 0.93f) - 75.f));
	TalkProxy->Interact_Implementation(InteractingActor);
}

void AFBTownsperson::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	USkeletalMeshComponent* Skin = GetSkeletalMeshComponent();
	if (!Skin || !GetWorld())
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.25f);
	Age += Dt;
	FVector At = GetActorLocation();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const float PlayerDist = Player ? FVector::Dist2D(Player->GetActorLocation(), At) : 0.f;

	// Once the height fit is done: animate only when on screen, sit down, think less when far away.
	if (!bFitDone && Age > 2.f)
	{
		bFitDone = true;
		// Always animate: "only when rendered" froze everyone in place (their render time never registers),
		// so far-away people just animate less often instead (see the tick intervals below).
		Skin->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		if (Mode == EMode::Sit)
		{
			SetActorRotation(FRotator(0.f, SeatYaw - 90.f, 0.f));
			Yaw = SeatYaw;
			PlayAnim(SitAnim ? SitAnim.Get() : IdleAnim.Get());
		}
	}
	SetActorTickInterval(PlayerDist > 9000.f ? 0.2f : 0.f);

	auto UpdateCapsule = [&]()
	{
		const float Z = CurrentGround + (Mode == EMode::Sit ? 60.f : 90.f);
		TalkCapsule->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Z));
	};

	// ---- Talking with the player: stop, face them (sitters stay seated).
	if (IsTalking())
	{
		if (Mode != EMode::Talk)
		{
			ModeBeforeTalk = Mode;
			Mode = EMode::Talk;
			WalkVelocity = FVector::ZeroVector;
			if (ModeBeforeTalk != EMode::Sit && ModeBeforeTalk != EMode::Stand)
			{
				PlayAnim(IdleAnim);
			}
		}
		if (ModeBeforeTalk != EMode::Sit && Player)
		{
			const FVector ToPlayer = Player->GetActorLocation() - At;
			Yaw = FMath::FixedTurn(Yaw, ToPlayer.Rotation().Yaw, 200.f * Dt);
			SetActorRotation(FRotator(0.f, Yaw - 90.f, 0.f));
		}
		UpdateCapsule();
		return;
	}
	if (Mode == EMode::Talk)
	{
		Mode = ModeBeforeTalk;
		if (Mode == EMode::Walk || Mode == EMode::Pause)
		{
			Pause(Random.FRandRange(1.f, 2.5f), false);
		}
	}

	switch (Mode)
	{
	case EMode::Pause:
		Wait -= Dt;
		if (Wait <= 0.f)
		{
			if (bFitDone)
			{
				StartWalking();
			}
			else
			{
				Wait = 0.3f;
			}
		}
		break;

	case EMode::Walk:
	{
		FVector To = Target - At;
		To.Z = 0.f;
		const float Dist = To.Size();
		if (Dist < 30.f)
		{
			ArriveAtTarget();
			break;
		}
		const FVector Dir = To / Dist;
		const FVector Right(-Dir.Y, Dir.X, 0.f);
		float SpeedNow = Speed;
		FVector Nudge = FVector::ZeroVector;

		// You, standing right in front: wait.
		if (Player)
		{
			const FVector P = Player->GetActorLocation() - At;
			const float Ahead = FVector::DotProduct(FVector(P.X, P.Y, 0.f), Dir);
			const float Sideways = FMath::Abs(FVector::DotProduct(FVector(P.X, P.Y, 0.f), Right));
			if (Ahead > 0.f && Ahead < 170.f && Sideways < 70.f)
			{
				Pause(Random.FRandRange(0.8f, 1.6f), false);
				break;
			}
		}
		// Other people: slow down behind them, step round the ones coming the other way.
		if (const UFBStreetNetwork* Net = UFBStreetNetwork::Get(this))
		{
			for (const TWeakObjectPtr<AFBTownsperson>& W : Net->GetWalkers())
			{
				const AFBTownsperson* O = W.Get();
				if (!O || O == this)
				{
					continue;
				}
				const FVector D = O->GetActorLocation() - At;
				if (FMath::Abs(D.X) > 220.f || FMath::Abs(D.Y) > 220.f)
				{
					continue;
				}
				const float Ahead = FVector::DotProduct(FVector(D.X, D.Y, 0.f), Dir);
				const float Sideways = FVector::DotProduct(FVector(D.X, D.Y, 0.f), Right);
				if (Ahead > 0.f && Ahead < 140.f && FMath::Abs(Sideways) < 55.f)
				{
					const FVector OV = O->GetWalkVelocity();
					if (FVector::DotProduct(OV, Dir) > 40.f)
					{
						SpeedNow = FMath::Min(SpeedNow, OV.Size()); // same way: fall in behind
					}
					else
					{
						Nudge += Right * (Sideways > 0.f ? -1.f : 1.f) * 70.f; // step aside
					}
				}
			}
		}
		// A lamp post, a hydrant, a bench in the way: step round it (or turn back if it's a wall).
		{
			BlockedTimer = FMath::Max(0.f, BlockedTimer - Dt * 0.5f);
			const FVector Chest(At.X, At.Y, CurrentGround + 110.f);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(FBWalkAhead), false, this);
			if (Player)
			{
				Q.AddIgnoredActor(Player);
			}
			if (GetWorld()->LineTraceSingleByChannel(Hit, Chest, Chest + Dir * 70.f, ECC_Visibility, Q) && !Cast<AFBTownsperson>(Hit.GetActor()))
			{
				BlockedTimer += Dt * 2.f;
				Nudge += Right * 90.f;
				if (BlockedTimer > 3.f)
				{
					BlockedTimer = 0.f;
					if (bStroll)
					{
						Home = At;
						Pause(Random.FRandRange(1.f, 3.f), false);
					}
					else
					{
						bTowardsB = !bTowardsB;
						bOnStrip = true;
						SetStripTarget(true);
						Pause(Random.FRandRange(0.5f, 1.5f), false);
					}
					break;
				}
			}
		}

		// Turn smoothly towards where they walk; slow down for sharp turns, speed up and slow down gradually
		// (no instant starts and stops), and match the stride to the real speed so the feet never skate.
		const float WantYaw = Dir.Rotation().Yaw;
		const float TurnLeft = FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, WantYaw));
		float WantSpeed = SpeedNow * FMath::GetMappedRangeValueClamped(FVector2D(20.f, 110.f), FVector2D(1.f, 0.35f), TurnLeft);
		if (StepSlow > 0.f)
		{
			StepSlow -= Dt;
			WantSpeed *= 0.78f;
		}
		if (Dist < 90.f && !bOnStrip)
		{
			WantSpeed *= FMath::GetMappedRangeValueClamped(FVector2D(30.f, 90.f), FVector2D(0.6f, 1.f), Dist);
		}
		CurSpeed = FMath::FInterpConstantTo(CurSpeed, WantSpeed, Dt, WantSpeed > CurSpeed ? 150.f : 320.f);
		Yaw = FMath::FixedTurn(Yaw, WantYaw, FMath::Lerp(260.f, 150.f, FMath::Clamp(CurSpeed / 160.f, 0.f, 1.f)) * Dt);
		SetActorRotation(FRotator(0.f, Yaw - 90.f, 0.f));
		// They walk where they face (bodies don't slide sideways), bending towards the goal as they turn.
		const FVector Facing = FRotator(0.f, Yaw, 0.f).Vector();
		const FVector MoveDir = (Facing * 0.75f + Dir * 0.25f).GetSafeNormal2D();
		const FVector Step = MoveDir * FMath::Min(Dist, CurSpeed * Dt) + Nudge * Dt;
		SetActorLocation(At + Step);
		WalkVelocity = MoveDir * CurSpeed;
		if (UAnimationAsset* Playing = Skin->GetSingleNodeInstance() ? Skin->GetSingleNodeInstance()->GetCurrentAsset() : nullptr)
		{
			if (Playing == WalkAnim.Get())
			{
				Skin->SetPlayRate(FMath::Clamp(CurSpeed / WalkAnimSpeed, 0.4f, 1.5f));
			}
		}
		StepTimer -= Dt;
		if (StepTimer <= 0.f && PlayerDist < 1500.f && CurSpeed > 30.f)
		{
			StepTimer = 0.55f * 125.f / FMath::Max(60.f, CurSpeed);
			FBFootsteps::Play(this, FVector(At.X, At.Y, ShownGround + 2.f), StepSlow > 0.f ? 0.11f : 0.08f);
		}

		// A stop halfway down the block to look around.
		if (bOnStrip && PauseAtT >= 0.f)
		{
			if (const UFBStreetNetwork* Net = UFBStreetNetwork::Get(this))
			{
				const FFBWalkStrip& S = Net->Strips()[Strip];
				const FVector AB = S.B - S.A;
				float T = FVector::DotProduct(At - S.A, AB) / FMath::Max(1.f, AB.SizeSquared());
				T = bTowardsB ? T : 1.f - T;
				if (T > PauseAtT)
				{
					PauseAtT = -1.f;
					Pause(Random.FRandRange(3.f, 7.f), true);
				}
			}
		}

		GroundTimer -= Dt;
		if (GroundTimer <= 0.f)
		{
			GroundTimer = PlayerDist < 3500.f ? 0.f : 0.3f; // every frame where you can see their feet
			UpdateGround(Dir);
		}
		SmoothGround(Dt);
		break;
	}

	default:
		break;
	}
	UpdateCapsule();
}
