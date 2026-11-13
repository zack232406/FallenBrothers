// Fallen Brothers - The town's street network (sidewalks + lanes), the crowd, and people sitting on benches.

#include "Town/FBStreetNetwork.h"

#include "Town/FBTownProps.h"
#include "Town/FBTownsperson.h"
#include "Town/FBAtmosphere.h"
#include "Town/FBPedestrian.h"
#include "Town/FBTownsfolk.h"
#include "Mission/FBCaseDirector.h"
#include "Town/FBBuilding.h"
#include "Components/CapsuleComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

namespace
{
	/** The downloaded people (Characters_New) and how tall they should be. Stand = keeps their own animation in place. */
	struct FPersonKind
	{
		const TCHAR* Mesh;
		float MinHeight;
		float MaxHeight;
		bool bStand;
	};
	const FPersonKind GPeople[] =
	{
		{ TEXT("/Game/FallenBrothers/Characters_New/Businessman/Businessman_Rigged/SkeletalMeshes/Businessman_Rigged.Businessman_Rigged"), 172.f, 186.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/CasualManRigged/CasualManRigged/SkeletalMeshes/CasualManRigged.CasualManRigged"), 170.f, 185.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/EverydayJane/everyday_jane_casual_modern_woman_rigged/SkeletalMeshes/everyday_jane_casual_modern_woman_rigged.everyday_jane_casual_modern_woman_rigged"), 158.f, 170.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/FemalePoliceOfficer/female_police_officer_fully_rigged_3d_character/SkeletalMeshes/female_police_office_y_rigged_3d_character.female_police_office_y_rigged_3d_character"), 162.f, 172.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/JoeRealistic/JoeRealistic_Rigged/SkeletalMeshes/JoeRealistic_Rigged.JoeRealistic_Rigged"), 172.f, 186.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/JustMan/JustMan/SkeletalMeshes/JustMan.JustMan"), 172.f, 188.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/MaleInSuit/MaleInSuit/SkeletalMeshes/MaleInSuit.MaleInSuit"), 172.f, 188.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/ManInCoat/man_in_coat_-_human_rigged_model/SkeletalMeshes/man_in_coat_-_human_rigged_model.man_in_coat_-_human_rigged_model"), 172.f, 186.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/MixedAdultMale/MixedAdultMale_Rigged/SkeletalMeshes/MixedAdultMale_Rigged.MixedAdultMale_Rigged"), 170.f, 186.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/OldManInCoat/old_man_in_coat_-_human_riged_model/SkeletalMeshes/old_man_in_coat_-_human_riged_model.old_man_in_coat_-_human_riged_model"), 164.f, 176.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/PoliceManUSA/police_man_from_the_u_s_a__t-pose_rig_mixamo/SkeletalMeshes/police_man_from_the_u_s_a__t-pose_rig_mixamo.police_man_from_the_u_s_a__t-pose_rig_mixamo"), 175.f, 188.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/WomanWalking/realistic_woman_walking_animated/SkeletalMeshes/realistic_woman_walking_animated.realistic_woman_walking_animated"), 160.f, 172.f, false },
		{ TEXT("/Game/FallenBrothers/Characters_New/OldRussianGuy/realistic_old_russian_guy_animated/SkeletalMeshes/realistic_old_russian_guy_animated.realistic_old_russian_guy_animated"), 168.f, 176.f, true },
		{ TEXT("/Game/FallenBrothers/Characters_New/TrafficCop/traffic_cop_animated/SkeletalMeshes/traffic_cop_animated.traffic_cop_animated"), 165.f, 175.f, true },
	};
	constexpr int32 GCrowdWalkers = 30;   // extra people walking the sidewalks
	constexpr int32 GMaxSeated = 10;      // people sitting on benches / in diner booths
	constexpr float GJoinRadius = 1500.f; // strip ends this close are connected (corners, crossings)

	FVector ClosestOnSegment(const FVector& P, const FVector& A, const FVector& B, float& OutT)
	{
		const FVector AB = B - A;
		const float L2 = AB.SizeSquared();
		OutT = L2 > 1.f ? FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
		return A + AB * OutT;
	}
}

UFBStreetNetwork* UFBStreetNetwork::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFBStreetNetwork>() : nullptr;
}

bool UFBStreetNetwork::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

bool UFBStreetNetwork::IsNight(const UWorld* World)
{
	// In a case the story decides: day investigation until the evening part starts.
	if (UFBCaseDirector* Director = World ? World->GetSubsystem<UFBCaseDirector>() : nullptr)
	{
		Director->EnsureStarted();
		if (Director->IsActive())
		{
			return Director->GetPhase() == EFBCasePhase::Evening;
		}
	}
	for (TActorIterator<AFBAtmosphere> It(World); It; ++It)
	{
		const float H = It->TimeOfDay;
		return H >= 20.5f || H < 5.5f;
	}
	return false;
}

void UFBStreetNetwork::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bNight = IsNight(&InWorld);
	if (bNight)
	{
		// A small town after dark: the streets are empty. The people placed in the level go home;
		// one or two walkers are all you meet (plus a few late customers inside).
		TArray<AFBTownsperson*> Placed;
		for (TActorIterator<AFBTownsperson> It(&InWorld); It; ++It)
		{
			if (!It->bRuntimeSpawned)
			{
				Placed.Add(*It);
			}
		}
		for (AFBTownsperson* P : Placed)
		{
			P->Destroy();
		}
		UE_LOG(LogTemp, Log, TEXT("FBStreetNetwork: night - %d placed townspeople sent home."), Placed.Num());
	}
	EnsureBuilt();
	// The old townspeople crowd is retired (SpawnCrowd / SeatPeople kept for reference): the new pedestrians.
	SpawnPedestrians();
}

void UFBStreetNetwork::SpawnPedestrians()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// The townsfolk: Epic's Game Animation Sample character (reparented onto AFBTownsfolk) wearing the town's people.
if (World->GetMapName().Contains(TEXT("Night")))
	{
		return; // the night level: nobody is out there
	}
		UClass* FolkClass = StaticLoadClass(AFBTownsfolk::StaticClass(), nullptr, TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"));
	if (!FolkClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("FBStreetNetwork: SandboxCharacter_CMC is missing or not reparented to FBTownsfolk - no townsfolk."));
		return;
	}
	const float Half = FolkClass->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FRandomStream Random(int32(FDateTime::Now().GetTicks() % 100000));

	// Every look once before any repeats (plus Kellan, the MetaHuman, as a whole visual override).
	const TArrayView<const AFBTownsfolk::FLookDef> Looks = AFBTownsfolk::Looks();
	TArray<int32> Order;
	for (int32 I = 0; I <= Looks.Num(); ++I)
	{
		Order.Add(I); // I == Looks.Num() -> Kellan
	}
	for (int32 I = Order.Num() - 1; I > 0; --I)
	{
		Order.Swap(I, Random.RandRange(0, I));
	}
	int32 NextLook = 0;
	auto Dress = [&](AFBTownsfolk* F, int32 LookIndex)
	{
		if (LookIndex >= Looks.Num())
		{
			F->FBLookActor = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/MetaHumans/Kellan/BP_Kellan.BP_Kellan_C")));
			F->FBHeight = 180.f;
			F->FBFemale = false;
			return;
		}
		const AFBTownsfolk::FLookDef& L = Looks[LookIndex];
		F->FBLookMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(L.Mesh));
		F->FBLookRetargeter = FName(L.Retargeter);
		F->FBLookYaw = L.Yaw;
		F->FBHeight = Random.FRandRange(L.MinHeight, L.MaxHeight);
		F->FBFemale = L.bFemale;
	};
	auto FindLook = [&](const TCHAR* Part) -> int32
	{
		for (int32 I = 0; I < Looks.Num(); ++I)
		{
			if (FCString::Strstr(Looks[I].Mesh, Part))
			{
				return I;
			}
		}
		return 0;
	};

	// The story people, where the case needs them (they stay put).
	struct FStory { const TCHAR* Preset; const TCHAR* Look; FVector At; float Yaw; };
	const FStory Story[] =
	{
		{ TEXT("MotelClerk"), TEXT("old_man_in_coat"), FVector(-16320.f, 5601.f, 124.f), 90.f },  // Wendell, motel desk
		{ TEXT("Martha"), TEXT("everyday_jane"), FVector(-8688.f, 6432.f, 92.f), -90.f },                   // Martha, the diner
	};
	for (const FStory& S : Story)
	{
		float Z = S.At.Z;
		if (!GroundAt(World, S.At - FVector(0.f, 0.f, 230.f), Z))
		{
			Z = S.At.Z - 90.f;
		}
		const FTransform T(FRotator(0.f, S.Yaw, 0.f), FVector(S.At.X, S.At.Y, Z + Half + 2.f));
		AFBTownsfolk* F = World->SpawnActorDeferred<AFBTownsfolk>(FolkClass, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (F)
		{
			Dress(F, FindLook(S.Look));
			F->FBStoryPreset = FName(S.Preset);
			F->FBWander = false;
			F->FinishSpawning(T);
		}
	}

	// The families and the library: in front of a house / the library, on the side that faces the sidewalk.
	auto FrontOf = [&](const AActor* Building, FVector& OutAt, float& OutYaw) -> bool
	{
		if (!Building || WalkStrips.Num() == 0)
		{
			return false;
		}
		FVector Origin, Ext;
		Building->GetActorBounds(true, Origin, Ext);
		const FVector Dirs[] = { Building->GetActorForwardVector(), -Building->GetActorForwardVector(), Building->GetActorRightVector(), -Building->GetActorRightVector() };
		float Best = 1e12f;
		for (const FVector& D0 : Dirs)
		{
			const FVector D = D0.GetSafeNormal2D();
			const float Reach = FMath::Abs(D.X) * Ext.X + FMath::Abs(D.Y) * Ext.Y + 140.f;
			FVector P = Origin + D * Reach;
			P.Z = Origin.Z - Ext.Z + 120.f;
			float T = 0.f;
			const int32 S = NearestStrip(P, 4000.f, &T);
			if (S == INDEX_NONE)
			{
				continue;
			}
			const FVector OnStrip = FMath::Lerp(WalkStrips[S].A, WalkStrips[S].B, T);
			const float Dist = FVector::Dist2D(P, OnStrip);
			if (Dist < Best)
			{
				Best = Dist;
				OutAt = P;
				OutYaw = D.Rotation().Yaw; // facing out, towards the street
			}
		}
		return Best < 1e12f;
	};
	auto SpawnStory = [&](const TCHAR* Preset, const TCHAR* Look, const FVector& At, float Yaw) -> bool
	{
		float Z = At.Z;
		if (!GroundAt(World, At, Z))
		{
			return false;
		}
		const FTransform T(FRotator(0.f, Yaw, 0.f), FVector(At.X, At.Y, Z + Half + 2.f));
		if (AFBTownsfolk* F = World->SpawnActorDeferred<AFBTownsfolk>(FolkClass, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			Dress(F, FindLook(Look));
			F->FBStoryPreset = FName(Preset);
			F->FBWander = false;
			F->FinishSpawning(T);
			return true;
		}
		return false;
	};
	if (UFBCaseDirector::IsCaseDay(World))
	{
		// The three families on their front walks (the houses nearest the diner), Crane's grand-nephew at the house
		// farthest from everyone - the town never let that family forget the name.
		TArray<const AActor*> Houses;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("FB_AutoHouse")))
			{
				Houses.Add(*It);
			}
		}
		const FVector Diner(-8688.f, 6432.f, 0.f);
		Houses.Sort([&](const AActor& A, const AActor& B)
		{
			return FVector::DistSquared2D(A.GetActorLocation(), Diner) < FVector::DistSquared2D(B.GetActorLocation(), Diner);
		});
		struct FFamily { const TCHAR* Preset; const TCHAR* Look; };
		const FFamily Families[] =
		{
			{ TEXT("Whitmore"), TEXT("npc_claudia") },
			{ TEXT("Decker"), TEXT("CasualManRigged") },
			{ TEXT("Barlow"), TEXT("npc_sophia") },
		};
		FVector At;
		float Yaw = 0.f;
		// One family per house: a house is several tagged actors, so keep the families well apart.
		TArray<FVector> Used;
		auto FarFromUsed = [&](const FVector& P)
		{
			for (const FVector& U : Used)
			{
				if (FVector::Dist2D(U, P) < 1500.f)
				{
					return false;
				}
			}
			return true;
		};
		int32 NextHouse = 0;
		for (const FFamily& Fam : Families)
		{
			while (NextHouse < Houses.Num())
			{
				if (FrontOf(Houses[NextHouse++], At, Yaw) && FarFromUsed(At) && SpawnStory(Fam.Preset, Fam.Look, At, Yaw))
				{
					Used.Add(At);
					break;
				}
			}
		}
		bool bCrane = false;
		for (int32 I = Houses.Num() - 1; I >= 0 && !bCrane; --I)
		{
			bCrane = FrontOf(Houses[I], At, Yaw) && FarFromUsed(At) && SpawnStory(TEXT("Crane"), TEXT("npc_old_man"), At, Yaw);
		}
		if (!bCrane)
		{
			// No free house: he waits at the far end of town, on the sidewalk farthest from the diner and the families.
			TArray<int32> ByDist;
			for (int32 I = 0; I < WalkStrips.Num(); ++I)
			{
				ByDist.Add(I);
			}
			ByDist.Sort([&](int32 A, int32 B)
			{
				return FVector::DistSquared2D(WalkStrips[A].A, Diner) > FVector::DistSquared2D(WalkStrips[B].A, Diner);
			});
			for (int32 I : ByDist)
			{
				const FVector P = (WalkStrips[I].A + WalkStrips[I].B) * 0.5f + FVector(0.f, 0.f, 120.f);
				if (FarFromUsed(P) && SpawnStory(TEXT("Crane"), TEXT("npc_old_man"), P, (Diner - P).Rotation().Yaw))
				{
					bCrane = true;
					break;
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("FBStreetNetwork: families placed (%d houses), Crane's relative %s."), Houses.Num(), bCrane ? TEXT("placed") : TEXT("NOT placed"));
		for (TActorIterator<AFBBuilding_Library> It(World); It; ++It)
		{
			if (FrontOf(*It, At, Yaw))
			{
				SpawnStory(TEXT("Librarian"), TEXT("npc_carla"), At, Yaw);
			}
			break;
		}
	}

	if (WalkStrips.Num() == 0)
	{
		return;
	}
	FVector Start = FVector(1e9f);
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Start = It->GetActorLocation();
		break;
	}
	const int32 Want = bNight ? Random.RandRange(1, 2) : 18;
	int32 Spawned = 0;
	for (int32 Tries = 0; Tries < Want * 6 && Spawned < Want; ++Tries)
	{
		const FFBWalkStrip& S = WalkStrips[Random.RandRange(0, WalkStrips.Num() - 1)];
		const FVector Dir = (S.B - S.A).GetSafeNormal2D();
		const FVector Right(-Dir.Y, Dir.X, 0.f);
		FVector P = FMath::Lerp(S.A, S.B, Random.FRandRange(0.1f, 0.9f)) + Right * S.Width * Random.FRandRange(-0.3f, 0.3f);
		if (FVector::Dist2D(P, Start) < 700.f)
		{
			continue;
		}
		float Z = P.Z;
		if (!GroundAt(World, P, Z))
		{
			continue;
		}
		const FTransform T((Random.FRand() < 0.5f ? Dir : -Dir).Rotation(), FVector(P.X, P.Y, Z + Half + 2.f));
		AFBTownsfolk* F = World->SpawnActorDeferred<AFBTownsfolk>(FolkClass, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
		if (!F)
		{
			continue;
		}
		Dress(F, Order[NextLook++ % Order.Num()]);
		F->FinishSpawning(T);
		++Spawned;
	}
	UE_LOG(LogTemp, Log, TEXT("FBStreetNetwork: %d townsfolk (%d looks) + story people."), Spawned, Looks.Num() + 1);
}

void UFBStreetNetwork::EnsureBuilt()
{
	if (bBuilt || !GetWorld())
	{
		return;
	}
	bBuilt = true;
	for (TActorIterator<AFBRoad> It(GetWorld()); It; ++It)
	{
		const AFBRoad* R = *It;
		FFBRoadPiece P;
		P.Start = R->GetActorLocation();
		P.Dir = R->GetActorForwardVector().GetSafeNormal2D();
		P.Right = R->GetActorRightVector().GetSafeNormal2D();
		P.Length = R->Length;
		P.HalfAsphalt = R->RoadWidth * 0.5f;
		P.LaneWidth = R->LaneWidth;
		P.Lanes = FMath::Max(1, R->LanesPerDirection);
		P.Sidewalk = R->SidewalkWidth;
		P.bTown = R->SidewalkWidth >= 120.f;
		P.bCrossStart = R->bCrosswalkAtStart;
		P.bCrossEnd = R->bCrosswalkAtEnd;
		RoadPieces.Add(P);

		if (P.bTown && P.Length > 300.f)
		{
			const float Off = P.HalfAsphalt + P.Sidewalk * 0.5f;
			for (const float Side : { 1.f, -1.f })
			{
				FFBWalkStrip S;
				S.A = P.Start + P.Right * Side * Off + P.Dir * 40.f + FVector(0.f, 0.f, 15.f);
				S.B = P.Start + P.Right * Side * Off + P.Dir * (P.Length - 40.f) + FVector(0.f, 0.f, 15.f);
				S.Width = P.Sidewalk;
				WalkStrips.Add(S);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("FBStreetNetwork: %d road pieces, %d sidewalk strips."), RoadPieces.Num(), WalkStrips.Num());
}

int32 UFBStreetNetwork::NearestStrip(const FVector& P, float MaxDist, float* OutT) const
{
	int32 Best = INDEX_NONE;
	float BestD = MaxDist * MaxDist;
	for (int32 I = 0; I < WalkStrips.Num(); ++I)
	{
		float T;
		const FVector C = ClosestOnSegment(P, WalkStrips[I].A, WalkStrips[I].B, T);
		const float D = FVector::DistSquared2D(C, P);
		if (D < BestD)
		{
			BestD = D;
			Best = I;
			if (OutT)
			{
				*OutT = T;
			}
		}
	}
	return Best;
}

int32 UFBStreetNetwork::NextStrip(int32 From, const FVector& End, FRandomStream& Random, bool& bOutFromA) const
{
	TArray<int32, TInlineAllocator<8>> Options;
	TArray<bool, TInlineAllocator<8>> FromA;
	for (int32 I = 0; I < WalkStrips.Num(); ++I)
	{
		if (I == From)
		{
			continue;
		}
		const float DA = FVector::Dist2D(WalkStrips[I].A, End);
		const float DB = FVector::Dist2D(WalkStrips[I].B, End);
		if (FMath::Min(DA, DB) < GJoinRadius)
		{
			Options.Add(I);
			FromA.Add(DA <= DB);
		}
	}
	if (Options.Num() == 0)
	{
		return INDEX_NONE;
	}
	const int32 Pick = Random.RandRange(0, Options.Num() - 1);
	bOutFromA = FromA[Pick];
	return Options[Pick];
}

bool UFBStreetNetwork::GroundAt(const UWorld* World, const FVector& P, float& OutZ, const AActor* Ignore)
{
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FBGround), false, Ignore);
	if (World->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 250.f), P - FVector(0.f, 0.f, 1500.f), Objects, Params))
	{
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

void UFBStreetNetwork::SpawnCrowd()
{
	UWorld* World = GetWorld();
	if (!World || WalkStrips.Num() == 0)
	{
		return;
	}
	FRandomStream Random(int32(FDateTime::Now().GetTicks() % 100000));
	FVector Start = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Start = It->GetActorLocation();
		break;
	}

	auto SpawnPerson = [&](const FPersonKind& Kind, const FVector& At, float FacingYaw, bool bStand) -> AFBTownsperson*
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Kind.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh)
		{
			return nullptr;
		}
		float GroundZ = At.Z;
		GroundAt(World, At, GroundZ);
		const FTransform T(FRotator(0.f, FacingYaw - 90.f, 0.f), FVector(At.X, At.Y, GroundZ));
		AFBTownsperson* P = World->SpawnActorDeferred<AFBTownsperson>(AFBTownsperson::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!P)
		{
			return nullptr;
		}
		P->bRuntimeSpawned = true;
		P->bStandStill = bStand;
		P->GroundZ = GroundZ;
		P->TargetHeight = Random.FRandRange(Kind.MinHeight, Kind.MaxHeight);
		if (USkeletalMeshComponent* Skin = P->GetSkeletalMeshComponent())
		{
			Skin->SetSkeletalMeshAsset(Mesh);
			Skin->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		}
		P->FinishSpawning(T);
		return P;
	};

	const int32 NumKinds = UE_ARRAY_COUNT(GPeople);
	int32 Spawned = 0;
	const int32 NumWalkers = bNight ? Random.RandRange(1, 2) : GCrowdWalkers;
	for (int32 Tries = 0; Tries < NumWalkers * 4 && Spawned < NumWalkers; ++Tries)
	{
		const FFBWalkStrip& S = WalkStrips[Random.RandRange(0, WalkStrips.Num() - 1)];
		const FVector Along = S.A + (S.B - S.A) * Random.FRandRange(0.05f, 0.95f);
		if (FVector::Dist2D(Along, Start) < 600.f)
		{
			continue; // not right on top of the player
		}
		const FPersonKind& Kind = GPeople[Random.RandRange(0, NumKinds - 3)]; // the walkers (last two stand)
		if (SpawnPerson(Kind, Along, (S.B - S.A).Rotation().Yaw, false))
		{
			++Spawned;
		}
	}
	// The two who stay put with their own animation: the traffic cop on a street corner, the old man by the road.
	for (int32 K = NumKinds - 2; K < NumKinds && !bNight; ++K)
	{
		const FFBWalkStrip& S = WalkStrips[Random.RandRange(0, WalkStrips.Num() - 1)];
		const FVector Corner = (K == NumKinds - 1) ? S.A : S.A + (S.B - S.A) * 0.5f;
		const FVector Road = FVector::CrossProduct(FVector::UpVector, (S.B - S.A).GetSafeNormal()); // one side
		SpawnPerson(GPeople[K], Corner, Road.Rotation().Yaw + (Random.FRand() < 0.5f ? 0.f : 180.f), true);
	}
	UE_LOG(LogTemp, Log, TEXT("FBStreetNetwork: crowd of %d walkers spawned."), Spawned);
}

void UFBStreetNetwork::SeatPeople()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FRandomStream Random(int32(FDateTime::Now().GetTicks() % 99991) + 7);
	const int32 NumKinds = UE_ARRAY_COUNT(GPeople);
	int32 Seated = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It && Seated < (bNight ? 3 : GMaxSeated); ++It)
	{
		const UStaticMeshComponent* SMC = It->GetStaticMeshComponent();
		const UStaticMesh* SM = SMC ? SMC->GetStaticMesh() : nullptr;
		if (!SM)
		{
			continue;
		}
		const FString Name = SM->GetName().ToLower();
		const bool bSeat = Name.Contains(TEXT("bench")) || Name.Contains(TEXT("couch")) || Name.Contains(TEXT("booth")) || Name.Contains(TEXT("sofa"));
		if (!bSeat || Random.FRand() > 0.45f)
		{
			continue;
		}
		const FBox Box = It->GetComponentsBoundingBox();
		if (!Box.IsValid || Box.GetSize().Z > 160.f || Box.GetSize().Z < 30.f)
		{
			continue;
		}
		// Which side is the seat? The long axis runs along the backrest; probe both sides of the short axis from
		// above - the backrest side is much higher than the cushion. People sit facing the cushion side.
		const FVector LocalExt = SM->GetBoundingBox().GetExtent() * SMC->GetComponentScale().GetAbs();
		const FQuat Q = SMC->GetComponentQuat();
		const bool bLongX = LocalExt.X >= LocalExt.Y;
		const FVector Depth = (bLongX ? Q.GetAxisY() : Q.GetAxisX()).GetSafeNormal2D();
		const FVector Along = (bLongX ? Q.GetAxisX() : Q.GetAxisY()).GetSafeNormal2D();
		const float HalfDepth = FMath::Min(LocalExt.X, LocalExt.Y);
		const float HalfLong = FMath::Max(LocalExt.X, LocalExt.Y);
		const FVector C = Box.GetCenter();
		float SideZ[2] = { -1.f, -1.f };
		for (int32 S = 0; S < 2; ++S)
		{
			const FVector P = C + Depth * (S == 0 ? 1.f : -1.f) * HalfDepth * 0.6f;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(FBSeatProbe), true);
			if (const_cast<UStaticMeshComponent*>(SMC)->LineTraceComponent(Hit, FVector(P.X, P.Y, Box.Max.Z + 20.f), FVector(P.X, P.Y, Box.Min.Z - 5.f), Params))
			{
				SideZ[S] = Hit.ImpactPoint.Z - Box.Min.Z;
			}
		}
		if (SideZ[0] < 0.f || SideZ[1] < 0.f || FMath::Abs(SideZ[0] - SideZ[1]) < 15.f)
		{
			continue; // no clear backrest - not a seat we understand
		}
		const FVector Front = SideZ[0] < SideZ[1] ? Depth : -Depth;
		const FVector Seat = FVector(C.X, C.Y, Box.Min.Z) + Front * HalfDepth * 0.15f + Along * Random.FRandRange(-0.25f, 0.25f) * HalfLong;
		const float Yaw = Front.Rotation().Yaw;
		UE_LOG(LogTemp, Log, TEXT("FBStreetNetwork: seat on %s sideZ %.0f/%.0f yaw %.0f"), *It->GetName(), SideZ[0], SideZ[1], Yaw);
		const FPersonKind& Kind = GPeople[Random.RandRange(0, NumKinds - 3)];
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Kind.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh)
		{
			continue;
		}
		const FTransform T(FRotator(0.f, Yaw - 90.f, 0.f), Seat);
		AFBTownsperson* P = World->SpawnActorDeferred<AFBTownsperson>(AFBTownsperson::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!P)
		{
			continue;
		}
		P->bRuntimeSpawned = true;
		P->bSeated = true;
		P->SeatYaw = Yaw;
		P->GroundZ = Seat.Z;
		P->TargetHeight = Random.FRandRange(Kind.MinHeight, Kind.MaxHeight);
		USkeletalMeshComponent* Skin = P->GetSkeletalMeshComponent();
		Skin->SetSkeletalMeshAsset(Mesh);
		Skin->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		P->FinishSpawning(T);
		++Seated;
	}
	UE_LOG(LogTemp, Log, TEXT("FBStreetNetwork: %d people sitting on benches and booths."), Seated);
}
