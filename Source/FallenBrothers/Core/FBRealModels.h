// Fallen Brothers - Real (downloaded) models for characters and items.
//
// Imported models live in /Game/FallenBrothers/<Folder>/<Name>/... (Unreal puts them in sub-folders such as
// StaticMeshes / SkeletalMeshes / Animations). These helpers find them by folder, so nothing has to be wired
// by hand: if the model is there it is used, otherwise the old grey shapes stay.
//
// NOTE for packaging: add /Game/FallenBrothers to "Additional Asset Directories to Cook" in Project Settings,
// because these assets are found by folder, not by a hard reference.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/ObjectLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"

namespace FBRealModels
{
	/** Every asset of class T under /Game/FallenBrothers/<Folder>/<Name> (all the usual import sub-folders). */
	template <class T>
	TArray<T*> FindAll(const TCHAR* Folder, const TCHAR* Name)
	{
		const FString Root = FString::Printf(TEXT("/Game/FallenBrothers/%s/%s"), Folder, Name);
		const FString Leaf = Name;
		const TArray<FString> Paths = {
			Root, Root / TEXT("StaticMeshes"), Root / TEXT("SkeletalMeshes"), Root / TEXT("Animations"),
			Root / Leaf, Root / Leaf / TEXT("StaticMeshes"), Root / Leaf / TEXT("SkeletalMeshes"), Root / Leaf / TEXT("Animations") };

		UObjectLibrary* Library = UObjectLibrary::CreateLibrary(T::StaticClass(), false, GIsEditor);
		Library->AddToRoot(); // keep it alive while we use it
		Library->LoadAssetsFromPaths(Paths);
		TArray<T*> Found;
		Library->GetObjects<T>(Found);
		Library->RemoveFromRoot();

		TArray<T*> Unique;
		for (T* Asset : Found)
		{
			if (Asset)
			{
				Unique.AddUnique(Asset);
			}
		}
		Unique.Sort([](const T& A, const T& B) { return A.GetName() < B.GetName(); });
		return Unique;
	}

	/** Rotation that turns a model's longest side to point forward (+X): for hand-held items. */
	inline FRotator LongestToForward(const TCHAR* Folder, const TCHAR* Name)
	{
		FBox Box(ForceInit);
		for (const UStaticMesh* P : FindAll<UStaticMesh>(Folder, Name))
		{
			Box += P->GetBoundingBox();
		}
		if (!Box.IsValid)
		{
			return FRotator::ZeroRotator;
		}
		const FVector Size = Box.GetSize();
		if (Size.Z >= Size.X && Size.Z >= Size.Y)
		{
			return FRotator(-90.f, 0.f, 0.f); // Z -> X
		}
		if (Size.Y >= Size.X)
		{
			return FRotator(0.f, -90.f, 0.f); // Y -> X
		}
		return FRotator::ZeroRotator;
	}

	/** Hides the grey-box shapes of an actor (static mesh components created in its constructor). */
	inline void HideGreyShapes(AActor* Owner)
	{
		TInlineComponentArray<UStaticMeshComponent*> Shapes(Owner);
		for (UStaticMeshComponent* C : Shapes)
		{
			if (C && !C->ComponentHasTag(TEXT("FBReal")))
			{
				C->SetVisibility(false);
				C->SetHiddenInGame(true);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}

	/**
	 * Attaches a downloaded static model (all its parts) to Parent.
	 * Size: the model's longest side becomes Length (or its height, if bByHeight).
	 * The model is centred on Parent (or, if bFeetOnOrigin, stands with its base at Parent's origin).
	 */
	inline bool AttachModel(AActor* Owner, USceneComponent* Parent, const TCHAR* Folder, const TCHAR* Name, float Length,
		const FRotator& Rotation = FRotator::ZeroRotator, const FVector& Offset = FVector::ZeroVector, bool bByHeight = false, bool bFeetOnOrigin = false)
	{
		if (!Owner || !Parent)
		{
			return false;
		}
		const TArray<UStaticMesh*> Parts = FindAll<UStaticMesh>(Folder, Name);
		if (Parts.Num() == 0)
		{
			return false;
		}

		FBox Box(ForceInit);
		for (const UStaticMesh* P : Parts)
		{
			Box += P->GetBoundingBox();
		}
		const FBox Turned = Box.TransformBy(FTransform(Rotation));
		const FVector Size = Turned.GetSize();
		const float S = Length / FMath::Max(1.f, bByHeight ? Size.Z : Size.GetMax());
		FVector Pivot = Turned.GetCenter();
		if (bFeetOnOrigin)
		{
			Pivot.Z = Turned.Min.Z;
		}

		for (UStaticMesh* P : Parts)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("RealModel")));
			C->ComponentTags.Add(TEXT("FBReal"));
			C->SetStaticMesh(P);
			C->SetupAttachment(Parent);
			C->SetRelativeRotation(Rotation);
			C->SetRelativeScale3D(FVector(S));
			C->SetRelativeLocation(Offset - Pivot * S);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(true);
			Owner->AddInstanceComponent(C);
			C->RegisterComponent();
		}
		return true;
	}

	/**
	 * Walk when moving, stand when not: switches a body between its walk animation (play rate follows the speed)
	 * and its idle (or a still frame of the walk if it has no idle). Runs on its own small ticker, stops by itself.
	 */
	inline void DriveLocomotion(USkeletalMeshComponent* Skin, UAnimSequence* Walk, UAnimSequence* Idle)
	{
		if (!Skin || !Walk)
		{
			return;
		}
		struct FState { bool bInit = false; bool bWalking = false; };
		TSharedRef<FState> State = MakeShared<FState>();
		TWeakObjectPtr<USkeletalMeshComponent> WeakSkin(Skin);
		TWeakObjectPtr<UAnimSequence> WeakWalk(Walk);
		TWeakObjectPtr<UAnimSequence> WeakIdle(Idle);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSkin, WeakWalk, WeakIdle, State](float) -> bool
		{
			USkeletalMeshComponent* S = WeakSkin.Get();
			UAnimSequence* W = WeakWalk.Get();
			if (!S || !W || !S->GetOwner() || !S->GetWorld())
			{
				return false; // the body is gone: stop ticking
			}
			const float Speed = S->GetOwner()->GetVelocity().Size2D();
			const bool bWalk = Speed > 30.f;
			if (!State->bInit || bWalk != State->bWalking)
			{
				State->bInit = true;
				State->bWalking = bWalk;
				// PlayAnimation hands the clip to the running single-node player. (OverrideAnimationData only
				// stores it for the next re-init, so after the first frame the body never switched clips:
				// walking in the idle pose, or standing in a frozen stride.)
				if (bWalk)
				{
					S->PlayAnimation(W, true);
				}
				else if (UAnimSequence* I = WeakIdle.Get())
				{
					S->PlayAnimation(I, true);
				}
				else
				{
					S->PlayAnimation(W, false); // feet-together frame, still
					S->SetPosition(0.f, false);
					S->Stop();
				}
			}
			if (bWalk)
			{
				S->SetPlayRate(FMath::Clamp(Speed / 140.f, 0.6f, 2.2f));
			}
			return true;
		}), 0.1f);
	}

	/**
	 * Some downloaded animations scale or move the whole rig (a man hundreds of metres tall in play).
	 * Once the animated pose is known, this measures the skeleton (lowest to highest bone), rescales Target
	 * so the body is Height tall, then puts the feet on the ground (GroundZ: world Z of the floor under it).
	 * Runs a few times over the first second of play.
	 */
	inline void FitSkeletonToHeight(USkeletalMeshComponent* Skin, USceneComponent* Target, float Height, TFunction<float()> GroundZ)
	{
		if (!Skin || !Target || !Skin->GetWorld() || !Skin->GetWorld()->IsGameWorld())
		{
			return;
		}
		TWeakObjectPtr<USkeletalMeshComponent> WeakSkin(Skin);
		TWeakObjectPtr<USceneComponent> WeakTarget(Target);
		float Wait = 0.f;
		int32 Runs = 0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSkin, WeakTarget, Height, GroundZ, Wait, Runs](float DeltaTime) mutable -> bool
		{
			USkeletalMeshComponent* S = WeakSkin.Get();
			USceneComponent* T = WeakTarget.Get();
			if (!S || !T || !S->GetWorld())
			{
				return false;
			}
			Wait += DeltaTime;
			if (Wait < 0.15f)
			{
				return true;
			}
			Wait = 0.f;
			const int32 N = S->GetNumBones();
			if (N < 2)
			{
				return false;
			}
			FBox Bones(ForceInit);
			for (int32 I = 0; I < N; ++I)
			{
				Bones += S->GetBoneTransform(I).GetLocation();
			}
			const FVector Size = Bones.GetSize();
			if (Size.Z < 1.f || Size.Z < FMath::Max(Size.X, Size.Y) * 0.5f)
			{
				return ++Runs < 10; // lying / not posed yet
			}
			const float Factor = Height / (Size.Z * 1.08f); // the top of the head is above the last bone
			if (FMath::Abs(Factor - 1.f) > 0.02f && Runs < 8)
			{
				T->SetWorldScale3D(T->GetComponentScale() * Factor);
				++Runs;
				return true;
			}
			// The right size: stand on the ground (the lowest bones are the toes / ankles, a few cm up).
			T->AddWorldOffset(FVector(0.f, 0.f, GroundZ() + 4.f - Bones.Min.Z));
			return false;
		}));
	}

	/**
	 * Gives a character actor a real, animated body: the rigged model in /Game/FallenBrothers/Characters/<Name>,
	 * scaled to Height, feet at FeetZ (relative to Parent). If the model has a walk animation, the body walks when
	 * the actor moves and stands still (idle) when it stops.
	 * bOwnerNoSee: the owner doesn't see it (first-person player) but it still casts a shadow.
	 * YawFix: imported glTF characters face -Y; +90 turns them to the actor's forward.
	 */
	inline USkeletalMeshComponent* DressCharacter(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, float Height, float FeetZ,
		bool bOwnerNoSee = false, float YawFix = 90.f)
	{
		if (!Owner || !Parent)
		{
			return nullptr;
		}
		const TArray<USkeletalMesh*> Meshes = FindAll<USkeletalMesh>(TEXT("Characters"), Name);
		if (Meshes.Num() == 0)
		{
			return nullptr;
		}
		USkeletalMesh* Mesh = Meshes[0];

		UAnimSequence* Walk = nullptr;
		UAnimSequence* Idle = nullptr;
		UAnimSequence* Any = nullptr;
		UAnimSequence* PoseAnim = nullptr;
		for (UAnimSequence* A : FindAll<UAnimSequence>(TEXT("Characters"), Name))
		{
			if (!A || A->GetSkeleton() != Mesh->GetSkeleton())
			{
				continue;
			}
			const FString N = A->GetName();
			if (N.Contains(TEXT("walk"), ESearchCase::IgnoreCase))
			{
				Walk = Walk ? Walk : A;
			}
			else if (N.Contains(TEXT("idle"), ESearchCase::IgnoreCase) || N.Contains(TEXT("stand"), ESearchCase::IgnoreCase) || N.Contains(TEXT("breath"), ESearchCase::IgnoreCase))
			{
				Idle = Idle ? Idle : A;
			}
			else if (N.Contains(TEXT("pose"), ESearchCase::IgnoreCase) && !N.Contains(TEXT("sit"), ESearchCase::IgnoreCase))
			{
				PoseAnim = PoseAnim ? PoseAnim : A;
			}
			else if (!N.Contains(TEXT("sit"), ESearchCase::IgnoreCase))
			{
				Any = Any ? Any : A;
			}
		}
		// "Pose" clips from these packs are arms-out bind poses: never used as idle (a still walk frame looks better).
		(void)PoseAnim;
		// Scans with a single unnamed clip ("Take 001") are walk cycles: use it as the walk.
		if (!Walk && Any && Any->GetPlayLength() > 0.5f)
		{
			Walk = Any;
		}

		// Size from the reference pose (arms out don't matter: only the height is used).
		const FBox B = Mesh->GetBounds().GetBox();
		const float S = Height / FMath::Max(1.f, B.GetSize().Z);

		USkeletalMeshComponent* Skin = NewObject<USkeletalMeshComponent>(Owner, MakeUniqueObjectName(Owner, USkeletalMeshComponent::StaticClass(), TEXT("RealBody")));
		Skin->SetSkeletalMeshAsset(Mesh);
		Skin->SetupAttachment(Parent);
		Skin->SetRelativeScale3D(FVector(S));
		Skin->SetRelativeRotation(FRotator(0.f, YawFix, 0.f));
		Skin->SetRelativeLocation(FVector(0.f, 0.f, FeetZ - B.Min.Z * S));
		Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skin->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		if (bOwnerNoSee)
		{
			Skin->SetOwnerNoSee(true);
			Skin->bCastHiddenShadow = true;
		}
		Skin->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (UAnimSequence* Start = Idle ? Idle : (Walk ? Walk : Any))
		{
			Skin->OverrideAnimationData(Start, true, Idle != nullptr || Walk == nullptr, 0.f, 1.f);
		}
		Owner->AddInstanceComponent(Skin);
		Skin->RegisterComponent();
		if (Walk)
		{
			DriveLocomotion(Skin, Walk, Idle);
		}
		HideGreyShapes(Owner);
		TWeakObjectPtr<USceneComponent> WeakParent(Parent);
		FitSkeletonToHeight(Skin, Skin, Height, [WeakParent, FeetZ]() { return WeakParent.IsValid() ? float(WeakParent->GetComponentLocation().Z) + FeetZ : 0.f; });
		return Skin;
	}
}
