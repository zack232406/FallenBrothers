// Fallen Brothers - Map generator for case 1 (Harlow Falls).

#include "Town/FBMapGenerator.h"
#include "Town/FBTownsperson.h"
#include "WaterBodyLakeActor.h"
#include "WaterSplineComponent.h"
#include "WaterZoneActor.h"
#include "WaterBodyComponent.h"
#include "WaterWaves.h"
#include "GerstnerWaterWaves.h"

#include "Core/FBRealModels.h"

#include "Environment/FBMountains.h"
#include "Environment/FBTerrain.h"
#include "Environment/FBWeatherZones.h"
#include "Town/FBAtmosphere.h"
#include "Town/FBBuilding.h"
#include "Town/FBStreetLamp.h"
#include "Town/FBTownProps.h"

#include "Companion/FBCompanion.h"
#include "Creatures/FBCreature.h"
#include "Dialogue/FBNPC.h"
#include "Interaction/FBCarTrunk.h"
#include "Interaction/FBDoor.h"
#include "Interaction/FBEvidenceItem.h"
#include "Inventory/FBItemPickup.h"
#include "Investigation/FBCaseFile.h"
#include "Investigation/FBResearchComputer.h"
#include "Mission/FBMissionManager.h"
#include "Paranormal/FBGhost.h"
#include "Paranormal/FBGraveSite.h"
#include "Paranormal/FBParanormalHotspot.h"
#include "Paranormal/FBUVTrace.h"
#include "Physics/FBChandelier.h"
#include "Physics/FBPhysicsProp.h"

#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Engine/ObjectLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/PointLight.h"
#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "Town/FBWeather.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogFBMapGen, Log, All);

namespace FBGen
{
	static const FName GeneratedTag = TEXT("FBGenerated");

	// Main street runs along X at Y = 0. Buildings' fronts sit this far from a road's centre line
	// (400 half road + 250 sidewalk + 200 front yard).
	constexpr float FrontLine = 850.f;

	/** Sets a (possibly protected) UPROPERTY by name, using the same text format as copy/paste in Details. */
	static void SetProp(UObject* Object, FName Name, const FString& Value)
	{
		FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(Name) : nullptr;
		if (!Property || !Property->ImportText_InContainer(*Value, Object, Object, PPF_None))
		{
			UE_LOG(LogFBMapGen, Warning, TEXT("Could not set %s on %s"), *Name.ToString(), *GetNameSafe(Object));
		}
	}

	static void SetObjectProp(UObject* Object, FName Name, UObject* Value)
	{
		if (FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(Object ? Object->GetClass()->FindPropertyByName(Name) : nullptr))
		{
			Property->SetObjectPropertyValue_InContainer(Object, Value);
		}
	}

	static FString Text(const TCHAR* S) { return FString::Printf(TEXT("INVTEXT(\"%s\")"), S); }

	static float YawFacing(const FVector& Direction) { return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)); }

	static int32 AxisOf(const FVector& V) { return FMath::Abs(V.X) > 0.5f ? 0 : 1; }
}

AFBMapGenerator::AFBMapGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	bIsEditorOnlyActor = true; // Not needed in the game itself.
}

// ------------------------------------------------------------------ Entry points

void AFBMapGenerator::ClearGenerated()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(FBGen::GeneratedTag))
		{
			ToDestroy.Add(*It);
		}
	}
	for (AActor* A : ToDestroy)
	{
		A->Destroy();
	}
	UE_LOG(LogFBMapGen, Log, TEXT("Removed %d generated actors."), ToDestroy.Num());
}

// ------------------------------------------------------------------ Downloaded models (used when the lists in Details are empty)

namespace FBDefaults
{
	struct FHouse
	{
		const TCHAR* Name;   // folder + asset name under /Game/FallenBrothers/Buildings/
		float Height;        // real height to scale it to (cm)
		float FrontYaw;      // turns the model so its front faces the street
		float Weight;
		bool bEnterable;     // has rooms inside: gets exact (per-triangle) collision so you can walk in
	};

	static const FHouse Houses[] = {
		// abandoned_house is left out: the scan includes 60 m of yard and trees around it (too big for a town lot).
		{ TEXT("victorian_house"),      1350.f,  0.f, 1.0f, false },
		// house_photogrammetry is left out: a low-detail scan, its walls look melted up close.
		{ TEXT("american_house"),        950.f,  0.f, 1.0f, false },
		{ TEXT("old_house"),             950.f,  0.f, 1.0f, false },
		// small_victorian_house is left out: its materials render as flat black boxes.
		// Classic American houses (white clapboard colonial with shutters, a suburban house, a country home).
		{ TEXT("house_colonial"),       1000.f,  0.f, 1.4f, false },
		{ TEXT("house_suburban2"),       760.f,  0.f, 1.2f, false },
		{ TEXT("house_countryside"),     750.f,  0.f, 1.0f, false },
		// A dark, ivy-covered gothic house (every horror town has one) and an old roadside gas station.
		// (house_gothic left out: its ivy renders as a huge melted green blob.)
		{ TEXT("shop_gas_station"),      520.f,  0.f, 0.35f, false }, // stands on a concrete forecourt (see SpawnModelHouse)
		// Houses you can go into.
		{ TEXT("house_enterable"),       880.f,  0.f, 1.2f, true },
		// shed_interior is left out: the scan stands on its own slab of lawn that floats above the lot.
		{ TEXT("shop_enterable"),        700.f,  0.f, 0.7f, true },
	};

	// Real models that replace the grey-box story buildings (the grey box stays, hidden, as the anchor for
	// gameplay positions). Height = real height (cm); Yaw turns the model's front to the building's front (+X).
	struct FStoryModel
	{
		const TCHAR* Name;
		float Height;
		float Yaw;
		float Back = 0.f; // moves the model back from the street (cm, along the building's -X)
		float Sink = 0.f; // pushes the model down into the ground (cm): scans that come with a sloped piece of ground
		bool bShellOnly = false; // only the outside: the hidden grey box keeps its walls' collision and its door (interior stays)
	};
	// Kell's Diner: art-deco diner exterior around the furnished grey-box diner.
	static const FStoryModel DinerModel   = { TEXT("diner_lincoln"),    570.f, 90.f, -220.f, 0.f, true }; // long side along the street
	static const FStoryModel MotelModel   = { TEXT("motel_modular"),    470.f, 90.f }; // doors 2.25 m, walls 2.45 m: room 7 (1:1) fits inside // long side along the street
	static const FStoryModel SheriffModel = { TEXT("sheriff_office"),   480.f, -90.f };
	static const FStoryModel LibraryModel = { TEXT("old_street_store"), 750.f, 0.f };
	// Kuusisto church (1792) with its bell tower and churchyard walls; height = top of the tower's cross.
	static const FStoryModel ChurchModel  = { TEXT("church_kuusisto"),  1500.f, 0.f, 0.f, 140.f };

	// Outskirts: the haunted mill (a barn you can walk through), the barn and the hunter's cabin.
	static const FStoryModel MillModel    = { TEXT("mill_barn_interior"), 900.f, 0.f };
	static const FStoryModel BarnModel    = { TEXT("barn_old_shop"),      950.f, 0.f };
	static const FStoryModel CabinModel   = { TEXT("cabin_worn_shed"),    420.f, 0.f };

	// The burned bungalow (real 3D scan, inside and out) on the road to the mill. Height includes its big tree.
	static const TCHAR* BurnedHouse = TEXT("chicago_fire_316_interior_exterior_bungalow_fire");
	static const float BurnedHouseHeight = 1050.f;

	/** Meshes that were made walkable this session (so it is done once). */
	static TSet<TWeakObjectPtr<UStaticMesh>> WalkableDone;

	/**
	 * Lets you walk inside a model: collision = its real triangles instead of a box around it,
	 * and Nanite off for it (Nanite collision uses a simplified copy that can close the doorways).
	 */
	static void MakeWalkable(UStaticMesh* Mesh)
	{
#if WITH_EDITOR
		if (!Mesh || WalkableDone.Contains(Mesh))
		{
			return;
		}
		WalkableDone.Add(Mesh);

		bool bChanged = false;
		if (FStructProperty* NaniteProp = CastField<FStructProperty>(UStaticMesh::StaticClass()->FindPropertyByName(TEXT("NaniteSettings"))))
		{
			void* Settings = NaniteProp->ContainerPtrToValuePtr<void>(Mesh);
			if (FBoolProperty* Enabled = CastField<FBoolProperty>(NaniteProp->Struct->FindPropertyByName(TEXT("bEnabled"))))
			{
				if (Enabled->GetPropertyValue_InContainer(Settings))
				{
					Mesh->Modify();
					Enabled->SetPropertyValue_InContainer(Settings, false);
					bChanged = true;
				}
			}
		}

		UBodySetup* Body = Mesh->GetBodySetup();
		if (!Body)
		{
			// Some imports come with no collision at all: give them a body (the exact triangles collide).
			Mesh->Modify();
			Mesh->CreateBodySetup();
			Body = Mesh->GetBodySetup();
			bChanged = true;
		}
		if (Body && Body->CollisionTraceFlag != CTF_UseComplexAsSimple)
		{
			Mesh->Modify();
			Body->Modify();
			Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
			bChanged = true;
		}

		if (bChanged)
		{
			Mesh->PostEditChange(); // rebuilds the mesh and its collision
			if (UBodySetup* NewBody = Mesh->GetBodySetup())
			{
				NewBody->CollisionTraceFlag = CTF_UseComplexAsSimple;
				NewBody->InvalidatePhysicsData();
				NewBody->CreatePhysicsMeshes();
			}
			Mesh->MarkPackageDirty();
			UE_LOG(LogFBMapGen, Log, TEXT("Model %s: made walkable (exact collision, Nanite off). Save it with File > Save All."), *Mesh->GetName());
		}
#endif
	}

	/**
	 * A downloaded model is one or more static meshes ("parts") somewhere under /Game/FallenBrothers/<Folder>/<Name>
	 * (Unreal imports a glTF either merged into one mesh or as one mesh per object). The parts share one origin,
	 * so placing them all with the same transform rebuilds the model. The first part is used as the model's key.
	 */
	static TMap<TWeakObjectPtr<UStaticMesh>, TArray<TWeakObjectPtr<UStaticMesh>>> PartsOf;

	static TArray<UStaticMesh*> Parts(UStaticMesh* Key)
	{
		TArray<UStaticMesh*> Out;
		if (const TArray<TWeakObjectPtr<UStaticMesh>>* Found = PartsOf.Find(Key))
		{
			for (const TWeakObjectPtr<UStaticMesh>& P : *Found)
			{
				if (P.IsValid())
				{
					Out.Add(P.Get());
				}
			}
		}
		if (Out.Num() == 0 && Key)
		{
			Out.Add(Key);
		}
		return Out;
	}

	/** Bounds of the whole model (all parts). */
	static FBox ModelBox(UStaticMesh* Key)
	{
		FBox Box(ForceInit);
		for (UStaticMesh* P : Parts(Key))
		{
			Box += P->GetBoundingBox();
		}
		return Box;
	}

	/** A flat ground plane that came with a scan (we have our own ground). */
	static bool IsGroundPlane(const UStaticMesh* Mesh)
	{
		const FVector Size = Mesh->GetBoundingBox().GetSize();
		const FString Name = Mesh->GetName();
		return Size.Z < 0.03f * FMath::Max(Size.X, Size.Y) || Name.Contains(TEXT("Ground")) || Name.StartsWith(TEXT("Plane"));
	}

	/** All parts of a model, ground planes left out. Returns the key part (nullptr if not imported yet). */
	static UStaticMesh* LoadModel(const TCHAR* Folder, const TCHAR* Name)
	{
		const FString Root = FString::Printf(TEXT("/Game/FallenBrothers/%s/%s"), Folder, Name);

		// Merged import: <Root>/StaticMeshes/<Name>.
		if (UStaticMesh* Single = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/StaticMeshes/%s.%s"), *Root, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Single;
		}

		// One mesh per object: load everything under the folder.
		UObjectLibrary* Library = UObjectLibrary::CreateLibrary(UStaticMesh::StaticClass(), false, GIsEditor);
		Library->LoadAssetsFromPaths({ Root, Root / TEXT("StaticMeshes"), Root / Name / TEXT("StaticMeshes") });
		TArray<UStaticMesh*> All;
		Library->GetObjects<UStaticMesh>(All);
		TArray<UStaticMesh*> Found;
		for (UStaticMesh* M : All)
		{
			Found.AddUnique(M);
		}
		// Drop the ground planes that come with some scans (by name only: a flat part can be a real floor).
		Found.RemoveAll([](const UStaticMesh* M)
		{
			if (!M)
			{
				return true;
			}
			const FString N = M->GetName();
			return N.Contains(TEXT("Ground")) || N.StartsWith(TEXT("Plane")) || N.Contains(TEXT("Dirt_Generique")) || N.Contains(TEXT("Grass_Grass"))
				|| N.Contains(TEXT("Grond")) || N.Contains(TEXT("Plane_Eau"));
		});
		Found.Sort([](const UStaticMesh& A, const UStaticMesh& B) { return A.GetName() < B.GetName(); });
		if (Found.Num() == 0)
		{
			UE_LOG(LogFBMapGen, Log, TEXT("Model not imported yet (skipped): %s"), *Root);
			return nullptr;
		}

		TArray<TWeakObjectPtr<UStaticMesh>>& List = PartsOf.FindOrAdd(Found[0]);
		List.Reset();
		for (UStaticMesh* M : Found)
		{
			List.Add(M);
		}
		return Found[0];
	}

	/** A townsperson model: rigged mesh + its own animation (idle / talking / walking in place). */
	struct FPerson { TWeakObjectPtr<USkeletalMesh> Mesh; TWeakObjectPtr<UAnimationAsset> Anim; TSoftClassPtr<AActor> Look; };
	static TArray<FPerson> People;
	/** The research desk went into the brothers' motel room (else: the library). */
	static bool bComputerPlaced = false;
	/** Where the motel guest book lies (on the Room 7 desk, borrowed from the office), world space; zero = none. */
	static FVector GuestBookAt = FVector::ZeroVector;
	/** The brothers' black sedan in the motel lot (world centre + nose direction); zero = not placed. */
	static FVector BrothersCarAt = FVector::ZeroVector;
	static FVector BrothersCarFacing = FVector(1.f, 0.f, 0.f);
	static float BrothersCarHalfLength = 270.f;
	static float GuestBookYaw = 0.f;

	static void LoadPeople()
	{
		People.Reset();
		// (npc_bearded_man left out: shirtless under an open cardigan - looks wrong walking down Main Street)
		for (const TCHAR* Name : { /* npc_old_man left out: his coat skins like a creature when he walks */ TEXT("npc_sophia"), TEXT("npc_eric"), TEXT("npc_carla"), TEXT("npc_claudia") })
		{
			const FString Root = FString::Printf(TEXT("/Game/FallenBrothers/People/%s"), Name);
			const TArray<FString> Paths = { Root, Root / TEXT("SkeletalMeshes"), Root / Name / TEXT("SkeletalMeshes"), Root / TEXT("Animations"), Root / Name / TEXT("Animations"), Root / Name };

			UObjectLibrary* Meshes = UObjectLibrary::CreateLibrary(USkeletalMesh::StaticClass(), false, GIsEditor);
			Meshes->LoadAssetsFromPaths(Paths);
			TArray<USkeletalMesh*> FoundMeshes;
			Meshes->GetObjects<USkeletalMesh>(FoundMeshes);

			UObjectLibrary* Anims = UObjectLibrary::CreateLibrary(UAnimSequence::StaticClass(), false, GIsEditor);
			Anims->LoadAssetsFromPaths(Paths);
			TArray<UAnimSequence*> FoundAnims;
			Anims->GetObjects<UAnimSequence>(FoundAnims);

			if (FoundMeshes.Num() == 0)
			{
				UE_LOG(LogFBMapGen, Log, TEXT("Townsperson not imported yet (skipped): %s"), *Root);
				continue;
			}
			FPerson P;
			P.Mesh = FoundMeshes[0];
			// Prefer the motion-captured idle made for this rig (<mesh>_idle, retargeted from the Game Animation Sample).
			for (UAnimSequence* A : FoundAnims)
			{
				if (A && A->GetSkeleton() == FoundMeshes[0]->GetSkeleton() && A->GetName().EndsWith(TEXT("_idle")))
				{
					P.Anim = A;
					break;
				}
			}
			for (UAnimSequence* A : P.Anim.IsValid() ? TArray<UAnimSequence*>() : FoundAnims)
			{
				// Their own idle / talking clip; the "_walk" made for strolling is picked up by the townsperson itself.
				if (A && A->GetSkeleton() == FoundMeshes[0]->GetSkeleton() && !A->GetName().EndsWith(TEXT("_walk")))
				{
					P.Anim = A;
					break;
				}
			}
			People.Add(P);
			UE_LOG(LogFBMapGen, Log, TEXT("Townsperson %s: mesh %s, animation %s"), Name, *FoundMeshes[0]->GetName(), P.Anim.IsValid() ? *P.Anim->GetName() : TEXT("none"));
		}

		// MetaHumans: their body (with its mocap idle / walk next to it) plus the blueprint that dresses it.
		struct FMetaHuman { const TCHAR* Body; const TCHAR* Look; };
		for (const FMetaHuman& MH : { FMetaHuman{ TEXT("/Game/MetaHumans/Kellan/Male/Medium/NormalWeight/Body/m_med_nrw_body"), TEXT("/Game/MetaHumans/Kellan/BP_Kellan.BP_Kellan_C") } })
		{
			const FString BodyPath = MH.Body;
			const FString BodyName = FPaths::GetBaseFilename(BodyPath);
			USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *BodyPath, *BodyName), nullptr, LOAD_NoWarn | LOAD_Quiet);
			UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s_idle.%s_idle"), *BodyPath, *BodyName), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Body || !Idle)
			{
				UE_LOG(LogFBMapGen, Log, TEXT("MetaHuman not ready (skipped): %s"), MH.Body);
				continue;
			}
			FPerson P;
			P.Mesh = Body;
			P.Anim = Idle;
			P.Look = TSoftClassPtr<AActor>(FSoftObjectPath(MH.Look));
			People.Add(P);
			UE_LOG(LogFBMapGen, Log, TEXT("Townsperson (MetaHuman) %s"), MH.Look);
		}
	}

	/** House lots of the last generation: a point on the road and the direction from the house to it. */
	/** House lots of the last generation: a point on the street's centre line, the direction from the house to it, the street. */
	struct FLot { FVector RoadPoint; FVector ToRoad; int32 Street = -1; FBox2D Foot = FBox2D(ForceInit); };
	/** Driveways with room for a parked car (Base-relative position, nose direction): filled in when the cars go out. */
	struct FCarSpot { FVector At; FVector Facing; };
	static TArray<FCarSpot> DrivewayCars;
	/** The middle of each backyard (Base-relative): a big old tree goes there first. */
	static TArray<FVector2D> BackyardTrees;
	/**
	 * The diner's furniture is laid out in its grey box's local space; the real model is shifted and sized differently,
	 * so local positions are mapped into the model's own footprint (else booths and the jukebox end up outside its walls).
	 */
	static FVector2D DinerShift = FVector2D::ZeroVector;
	static FVector2D DinerScale = FVector2D(1.f, 1.f);
	static FVector DinerLocal(const FVector& L) { return FVector(DinerShift.X + L.X * DinerScale.X, DinerShift.Y + L.Y * DinerScale.Y, L.Z); }
	static TArray<FLot> Lots;

	/**
	 * The street network (Base-relative). E-W streets run along X at Y = Pos, from X = From to To;
	 * N-S streets run along Y at X = Pos. Where two cross there is an intersection.
	 */
	struct FStreet
	{
		bool bAlongX;
		float Pos, From, To;
		float Asphalt;        // full asphalt width (travel lanes + parking lanes)
		float Sidewalk;       // each side (0 = country road)
		int32 Lanes;          // per direction
		float Parking;        // parking lane width each side
		bool bTown;           // markings, crosswalks, lots, lamps
		int32 Surface;        // 0 asphalt, 1 dirt, 2 forest trail
		float Half() const { return Asphalt * 0.5f; }
		float Edge() const { return Asphalt * 0.5f + Sidewalk; }   // outer edge of the sidewalk
		FVector2D Dir() const { return bAlongX ? FVector2D(1.f, 0.f) : FVector2D(0.f, 1.f); }
		FVector2D At(float Along, float Across) const { return bAlongX ? FVector2D(Along, Pos + Across) : FVector2D(Pos + Across, Along); }
	};
	static TArray<FStreet> Streets;

	// Harlow Falls: a real small-town grid. Main Street has two lanes each way plus parking, the avenues and the
	// cross streets one lane each way plus parking. All crossings are proper four-way intersections.
	static const float CrossX[] = { -11000.f, -4200.f, 2200.f, 8000.f };
	static const float AvenueY = 6600.f; // deep enough for two rows of houses back to back
	static const float TownWest = -11000.f, TownEast = 8000.f, CrossEnd = 10500.f;

	static void MakeStreets()
	{
		Streets.Reset();
		// Main Street (Y = 0): 2 + 2 lanes of 3.5 m, 2.5 m parking each side, 3 m sidewalks.
		Streets.Add({ true, 0.f, TownWest, TownEast, 1900.f, 300.f, 2, 250.f, true, 0 });
		// North and South Avenue.
		Streets.Add({ true,  AvenueY, TownWest, TownEast, 1200.f, 250.f, 1, 250.f, true, 0 });
		Streets.Add({ true, -AvenueY, TownWest, TownEast, 1200.f, 250.f, 1, 250.f, true, 0 });
		// Cross streets (Oak, Elm, Church, Mill Street), past both avenues.
		for (const float X : CrossX)
		{
			Streets.Add({ false, X, -CrossEnd, CrossEnd, 1200.f, 250.f, 1, 250.f, true, 0 });
		}
		// Out of town: the highway west to the mountain pass, the dirt road east to the mill, the trail north.
		Streets.Add({ true, 0.f, -26000.f, TownWest, 750.f, 0.f, 1, 0.f, false, 0 });
		Streets.Add({ true, 0.f, TownEast, 22000.f, 700.f, 0.f, 1, 0.f, false, 1 });
		Streets.Add({ false, CrossX[1], CrossEnd, 16500.f, 300.f, 0.f, 1, 0.f, false, 2 });
	}

	/** Where streets cross (Base-relative XY) and which two streets (along X, along Y). */
	struct FCrossing { FVector2D At; int32 StreetX; int32 StreetY; };
	static TArray<FCrossing> Crossings()
	{
		TArray<FCrossing> Out;
		for (int32 I = 0; I < Streets.Num(); ++I)
		{
			const FStreet& EW = Streets[I];
			if (!EW.bAlongX || EW.Sidewalk <= 0.f)
			{
				continue;
			}
			for (int32 J = 0; J < Streets.Num(); ++J)
			{
				const FStreet& NS = Streets[J];
				if (NS.bAlongX || NS.Sidewalk <= 0.f)
				{
					continue;
				}
				if (NS.Pos >= EW.From - 1.f && NS.Pos <= EW.To + 1.f && EW.Pos >= NS.From - 1.f && EW.Pos <= NS.To + 1.f)
				{
					Out.Add({ FVector2D(NS.Pos, EW.Pos), I, J });
				}
			}
		}
		return Out;
	}

	/** Every flat footprint that is taken (roads, buildings, the cemetery...), Base-relative. */
	static TArray<FBox2D> Occupied;
	/** The motel's extra wings (same model, rooms you can't go into) and its parking lot (world-relative to Base). */
	static TArray<FVector> MotelWings;
	static FBox2D MotelParking(ForceInit);

	static bool IsFree(const FBox2D& Box, float Margin)
	{
		const FBox2D Test(Box.Min - FVector2D(Margin), Box.Max + FVector2D(Margin));
		for (const FBox2D& O : Occupied)
		{
			if (Test.Intersect(O))
			{
				return false;
			}
		}
		return true;
	}

	/** Turns each car model so its nose points along the driving direction (checked in the editor per model). */
	static float CarYaw(const TCHAR* Name)
	{
		static const TMap<FString, float> Yaws = {
			{ TEXT("car_old_worn"), 0.f }, { TEXT("car_rusty_pickup"), 0.f }, { TEXT("car_cadillac"), 0.f },
			{ TEXT("car_bmw"), 0.f }, { TEXT("car_wagon"), 0.f }, { TEXT("car_hatchback"), 0.f }, { TEXT("car_police"), 0.f } };
		const float* Y = Yaws.Find(Name);
		return Y ? *Y : 0.f;
	}
	static const float SignalYaw = 0.f;       // traffic light model: turns its lamps towards the driver
	static const float StopSignYaw = 0.f;     // stop sign model: turns its red face towards the driver
	static const float StaticPersonYaw = -90.f; // the glTF people scans face +Y

	/** Static (not rigged) people scans: they stand still, perfectly posed. */
	static const TCHAR* StaticPeople[] = { TEXT("npc_mother_child") }; // (npc_dennis is an arms-out bind pose: not used)

	static const TCHAR* Church = TEXT("historic_wooden_church");
	static const float ChurchHeight = 822.f;   // the scan is real-world size: keep it 1:1
	static const float ChurchYaw = 90.f;       // long side along the road behind the cemetery

	static const TCHAR* Trees[] = {
		TEXT("/Game/Megaplant_Library/Tree_Black_Alder/Tree_Black_Alder_01/SK_Black_Alder_01_A"),
		TEXT("/Game/Megaplant_Library/Tree_Black_Alder/Tree_Black_Alder_01/SK_Black_Alder_01_B"),
		TEXT("/Game/Megaplant_Library/Tree_Black_Alder/Tree_Black_Alder_01/SK_Black_Alder_01_C"),
		TEXT("/Game/Megaplant_Library/Tree_Black_Alder/Tree_Black_Alder_01/SK_Black_Alder_01_D"),
		TEXT("/Game/Megaplant_Library/Tree_Goat_Willow/Tree_Goat_Willow_01/SK_Goat_Willow_01_A"),
		TEXT("/Game/Megaplant_Library/Tree_Goat_Willow/Tree_Goat_Willow_01/SK_Goat_Willow_01_B"),
		TEXT("/Game/Megaplant_Library/Tree_Goat_Willow/Tree_Goat_Willow_01/SK_Goat_Willow_01_C"),
		TEXT("/Game/Megaplant_Library/Tree_Goat_Willow/Tree_Goat_Willow_01/SK_Goat_Willow_01_D"),
		TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_A"),
		TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_B"),
		TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_C"),
		TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D"),
	};

	static UStaticMesh* LoadBuilding(const TCHAR* Name)
	{
		return LoadModel(TEXT("Buildings"), Name);
	}

	/** True while the downloaded houses are in use: then every lot gets a real house (no grey boxes). */
	static bool bUsingDownloadedHouses = false;

	/** Scale that makes the model Height tall, but never wider than MaxFootprint (so it fits its lot). */
	static float FitScale(const UStaticMesh* Mesh, float Height, float MaxFootprint)
	{
		const FVector Size = ModelBox(const_cast<UStaticMesh*>(Mesh)).GetSize();
		const float ByHeight = Height / FMath::Max(1.f, Size.Z);
		const float ByFootprint = MaxFootprint / FMath::Max(1.f, FMath::Max(Size.X, Size.Y));
		const float S = FMath::Clamp(FMath::Min(ByHeight, ByFootprint), 0.001f, 1000.f);

		UE_LOG(LogFBMapGen, Log, TEXT("Model %s: raw size %.0f x %.0f x %.0f cm -> scale %.3f -> %.0f x %.0f x %.0f cm%s"),
			*Mesh->GetName(), Size.X, Size.Y, Size.Z, S, Size.X * S, Size.Y * S, Size.Z * S,
			ByFootprint < ByHeight ? TEXT("  (limited by lot size)") : TEXT(""));
		return S;
	}
}

/** Size of a story model once placed (scaled, turned by BuildingYaw + its own yaw). Invalid if not imported. */
static FBox FBStoryBox(const FBDefaults::FStoryModel& Def, float BuildingYaw)
{
	UStaticMesh* Key = FBDefaults::LoadBuilding(Def.Name);
	if (!Key)
	{
		return FBox(ForceInit);
	}
	const FBox Model = FBDefaults::ModelBox(Key);
	const float S = Def.Height / FMath::Max(1.f, Model.GetSize().Z);
	return Model.TransformBy(FTransform(FRotator(0.f, BuildingYaw + Def.Yaw, 0.f), FVector::ZeroVector, FVector(S)));
}

/** Puts a real model where a grey-box building stands; the grey box stays as an invisible anchor (tag FBReplaced). */
static bool FBReplaceWithModel(AFBBuilding* B, const FBDefaults::FStoryModel& Def, TFunctionRef<AStaticMeshActor*(const FVector&, float, UStaticMesh*)> SpawnPart)
{
	UStaticMesh* Key = B ? FBDefaults::LoadBuilding(Def.Name) : nullptr;
	if (!Key)
	{
		return false;
	}
	// The model's exact shape collides (walls, counter, booths) - also under a shell, whose grey walls no longer block.
	for (UStaticMesh* Part : FBDefaults::Parts(Key))
	{
		FBDefaults::MakeWalkable(Part);
	}

	const float Yaw = B->GetActorRotation().Yaw + Def.Yaw;
	const FBox Model = FBDefaults::ModelBox(Key);
	const float S = Def.Height / FMath::Max(1.f, Model.GetSize().Z);
	const FBox Box = Model.TransformBy(FTransform(FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, FVector(S)));
	const FVector Where = B->GetActorLocation() - B->GetActorForwardVector() * Def.Back;
	const FVector At(Where.X - Box.GetCenter().X, Where.Y - Box.GetCenter().Y, Where.Z - Box.Min.Z - 3.f - Def.Sink);

	AStaticMeshActor* First = nullptr;
	for (UStaticMesh* Part : FBDefaults::Parts(Key))
	{
		AStaticMeshActor* A = SpawnPart(At, Yaw, Part);
		if (!A)
		{
			continue;
		}
		A->SetActorScale3D(FVector(S));

		if (!First)
		{
			First = A;
#if WITH_EDITOR
			A->SetActorLabel(FString::Printf(TEXT("HF_%s"), Def.Name));
#endif
		}
		else
		{
			A->AttachToActor(First, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	if (Def.bShellOnly)
	{
		// Keep the grey box for its collision and door, but the model's own interior is what you see:
		// the grey walls go invisible (they doubled up with the model's walls, windows opening onto walls),
		// and the floor slab grows to the model's whole footprint (no grass showing at the edges).
		// Stored on the building itself (its construction rebuilds the boxes, so hiding components here would not stick).
		const FBox WorldBox = Box.ShiftBy(At);
		const FBox LocalBox = WorldBox.InverseTransformBy(B->GetActorTransform());
		B->bShellOnly = true;
		B->ShellFloorCenter = FVector2D(LocalBox.GetCenter());
		B->ShellFloorSize = FVector2D(LocalBox.GetSize()) - FVector2D(40.f, 40.f);
		if (FString(Def.Name).Contains(TEXT("diner")))
		{
			const float HalfDepth = FMath::Max(100.f, B->Depth * 0.5f), HalfWidth = FMath::Max(100.f, B->Width * 0.5f);
			FBDefaults::DinerShift = FVector2D(LocalBox.GetCenter());
			FBDefaults::DinerScale = FVector2D(LocalBox.GetExtent().X * 0.86f / HalfDepth, LocalBox.GetExtent().Y * 0.9f / HalfWidth);
			UE_LOG(LogFBMapGen, Log, TEXT("Diner furniture map: shift %s, scale %s."), *FBDefaults::DinerShift.ToString(), *FBDefaults::DinerScale.ToString());
		}
		B->RerunConstructionScripts();
		UE_LOG(LogFBMapGen, Log, TEXT("%s dressed with the model %s (scale %.3f)."), *B->GetName(), Def.Name, S);
		return true;
	}

	// The grey box: invisible, no collision, but still there for gameplay positions.
	B->Tags.AddUnique(TEXT("FBReplaced"));
	B->SetActorHiddenInGame(true);
	B->SetActorEnableCollision(false);
	FBGen::SetProp(B, TEXT("bHiddenEd"), TEXT("True"));
	FBGen::SetProp(B, TEXT("bInteriorLight"), TEXT("False"));
	B->RerunConstructionScripts();
	TInlineComponentArray<USceneComponent*> Comps(B);
	for (USceneComponent* C : Comps)
	{
		if (C)
		{
			C->SetVisibility(false, false);
			C->SetHiddenInGame(true, false);
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(C))
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
#if WITH_EDITOR
	B->SetIsTemporarilyHiddenInEditor(true);
#endif
	UE_LOG(LogFBMapGen, Log, TEXT("%s replaced by the model %s (scale %.3f)."), *B->GetName(), Def.Name, S);
	return true;
}

void AFBMapGenerator::GenerateMap()
{
	ClearGenerated();
	LoadMaterials();
	Base = GetActorLocation();
	SpawnedCount = 0;

	// Fill the model lists from the downloaded assets if you haven't set your own.
	FBDefaults::bUsingDownloadedHouses = false;

	// A list we filled ourselves last time is refreshed (so size fixes in the code apply).
	const bool bOnlyOurHouses = HouseModels.Num() > 0 && !HouseModels.ContainsByPredicate([](const FFBTownBuildingType& T)
	{
		return !T.Mesh || !T.Mesh->GetPathName().StartsWith(TEXT("/Game/FallenBrothers/Buildings/"));
	});
	if (bOnlyOurHouses)
	{
		HouseModels.Reset();
	}

	if (HouseModels.Num() == 0)
	{
		FBDefaults::bUsingDownloadedHouses = true;
		for (const FBDefaults::FHouse& H : FBDefaults::Houses)
		{
			if (UStaticMesh* Mesh = FBDefaults::LoadBuilding(H.Name))
			{
				if (H.bEnterable)
				{
					for (UStaticMesh* Part : FBDefaults::Parts(Mesh))
					{
						FBDefaults::MakeWalkable(Part);
					}
				}
				FFBTownBuildingType T;
				T.Mesh = Mesh;
				T.FrontYaw = H.FrontYaw;
				T.Weight = H.Weight;
				T.Scale = FBDefaults::FitScale(Mesh, H.Height, 1750.f);
				HouseModels.Add(T);
			}
		}
	}
	if (TreeModels.Num() == 0 && SkeletalTreeModels.Num() == 0)
	{
		for (const TCHAR* Path : FBDefaults::Trees)
		{
			if (USkeletalMesh* Tree = LoadObject<USkeletalMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				SkeletalTreeModels.Add(Tree);
			}
		}
	}

	FRandomStream Random(Seed);
	FBDefaults::MakeStreets();
	FBDefaults::Occupied.Reset();
	BuildGround();

	// The lake: real water physics (FBWater: things float or sink by density, currents, slow loud wading) and a
	// Single Layer Water surface (refraction, depth colour, sun glints, drifting ripples: M_FB_LakeWater).
	// The box covers the whole basin; its top is the water surface (the ground hides it outside the shore).
	if (bLake && Terrain.IsValid())
	{
		const AFBTerrain* T = Terrain.Get();
		const float SurfaceZ = T->GetActorLocation().Z + T->WaterLevel;
		const float Depth = T->LakeDepth + 200.f;
		const FVector Scale(T->LakeRadius.X * 1.3f / 400.f, T->LakeRadius.Y * 1.3f / 400.f, Depth / 400.f); // FBWater box: 400 x 400 x 200 half-size
		AFBWater* Lake = Spawn<AFBWater>(FVector(T->LakeCenter.X, T->LakeCenter.Y, SurfaceZ - Depth * 0.5f), 0.f, TEXT("Nature/Water"), [&](AFBWater* W)
		{
			W->WaterMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FallenBrothers/Materials/M_FB_LakeWater.M_FB_LakeWater"));
			W->Drag = 3.f;
			W->Flow = FVector(12.f, -6.f, 0.f); // a faint drift towards the outflow
		});
		if (Lake)
		{
			Lake->SetActorScale3D(Scale); // after spawning (FinishSpawning resets the scale)
#if WITH_EDITOR
			Lake->SetActorLabel(TEXT("HF_Lake"));
#endif
		}
	}

	BuildRoads();
	BuildTown(Random);
	BuildOutskirts();

	// The old wooden church (scan) behind the cemetery.
	// (Only when the story church didn't already get the real scan.)
	UStaticMesh* ChurchMesh = nullptr; // (the old interior-only church scan is no longer used)
	if (ChurchMesh)
	{
		const float S = FBDefaults::FitScale(ChurchMesh, FBDefaults::ChurchHeight, 4000.f);
		const float Yaw = FBDefaults::ChurchYaw;
		const FBox Box = ChurchMesh->GetBoundingBox().TransformBy(FTransform(FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, FVector(S)));
		const FVector Where = Base + FVector(6400.f - Box.GetCenter().X, 6250.f - Box.GetCenter().Y, -Box.Min.Z - 5.f);
		AStaticMeshActor* OldChurch = Spawn<AStaticMeshActor>(Where, Yaw, TEXT("Outskirts"), [&](AStaticMeshActor* A)
		{
			A->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
			A->GetStaticMeshComponent()->SetStaticMesh(ChurchMesh);
		});
		if (OldChurch)
		{
			OldChurch->SetActorScale3D(FVector(S));
#if WITH_EDITOR
			OldChurch->SetActorLabel(TEXT("HF_OldChurch"));
#endif
		}
	}

	// Puts a model with its bottom on the ground, centred on Where (world). Scale = Height / model height.
	// bLongSideAlongY: turns it so its longer side runs along the building's Y (else along X).
	auto PlaceModel = [this](UStaticMesh* Mesh, const FVector& Where, float Yaw, float Height, const TCHAR* Folder, const TCHAR* Label) -> AStaticMeshActor*
	{
		if (!Mesh)
		{
			return nullptr;
		}
		const FBox Model = FBDefaults::ModelBox(Mesh);
		const float S = Height / FMath::Max(1.f, Model.GetSize().Z);
		const FBox Box = Model.TransformBy(FTransform(FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, FVector(S)));
		const FVector At(Where.X - Box.GetCenter().X, Where.Y - Box.GetCenter().Y, Where.Z - Box.Min.Z);

		// Every part with the same transform; the first one is the "main" actor the others are attached to.
		AStaticMeshActor* First = nullptr;
		for (UStaticMesh* Part : FBDefaults::Parts(Mesh))
		{
			AStaticMeshActor* A = Spawn<AStaticMeshActor>(At, Yaw, Folder, [&](AStaticMeshActor* M)
			{
				M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
				M->GetStaticMeshComponent()->SetStaticMesh(Part);
			});
			if (!A)
			{
				continue;
			}
			A->SetActorScale3D(FVector(S));
			if (!First)
			{
				First = A;
#if WITH_EDITOR
				if (Label)
				{
					A->SetActorLabel(Label);
				}
#endif
			}
			else
			{
				A->AttachToActor(First, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
		return First;
	};

	// Yaw that lays a model's long side along the given axis (0 = X, 1 = Y), plus Extra.
	auto LongSide = [](const UStaticMesh* Mesh, int32 Axis, float Extra) -> float
	{
		if (!Mesh)
		{
			return Extra;
		}
		const FVector Size = FBDefaults::ModelBox(const_cast<UStaticMesh*>(Mesh)).GetSize();
		const int32 Long = Size.X >= Size.Y ? 0 : 1;
		return (Long == Axis ? 0.f : 90.f) + Extra;
	};

	// ---- The burned bungalow (3D scan, you can walk inside) on the south side of the road to the mill.
	if (UStaticMesh* Burned = FBDefaults::LoadBuilding(FBDefaults::BurnedHouse))
	{
		for (UStaticMesh* Part : FBDefaults::Parts(Burned))
		{
			FBDefaults::MakeWalkable(Part);
		}
		PlaceModel(Burned, Base + FVector(11500.f, -2100.f, -5.f), 0.f, FBDefaults::BurnedHouseHeight, TEXT("Outskirts"), TEXT("HF_BurnedHouse"));
	}

	// ---- Kell's Diner furniture (real models). Diner space: +X = front (street), walls at X +-500, Y +-800, floor Z = 0.
	if (AFBBuilding* D = Diner.Get())
	{
		UStaticMesh* Counter = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_counter"));
		UStaticMesh* Couch   = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_couch"));
		UStaticMesh* Table   = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_table"));
		UStaticMesh* Chair   = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_chair"));
		UStaticMesh* Bench   = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_bench"));
		UStaticMesh* Jukebox = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_jukebox"));
		UStaticMesh* Sign    = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_sign"));

		const float DYaw = D->GetActorRotation().Yaw;
		auto At = [D](float X, float Y) { return D->GetActorTransform().TransformPosition(FBDefaults::DinerLocal(FVector(X, Y, 0.f))); };
		const TCHAR* F = TEXT("Town/Diner");

		// Counter in front of Martha (she stands at X -150, Y 300), running along the back half.
		PlaceModel(Counter, At(-20.f, 300.f), DYaw + LongSide(Counter, 1, 0.f), 110.f, F, TEXT("Diner_Counter"));

		// Window booths: table between two couches, at right angles to the front windows.
		for (const float Y : { -650.f, -380.f, 650.f })
		{
			PlaceModel(Table, At(370.f, Y), DYaw + LongSide(Table, 0, 0.f), 76.f, F, nullptr);
			PlaceModel(Couch, At(370.f, Y - 105.f), DYaw + LongSide(Couch, 0, 0.f), 115.f, F, nullptr);
			PlaceModel(Couch, At(370.f, Y + 105.f), DYaw + LongSide(Couch, 0, 180.f), 115.f, F, nullptr);
		}

		// Tables with chairs in the middle.
		for (const FVector2D T : { FVector2D(80.f, -520.f), FVector2D(80.f, -220.f), FVector2D(-250.f, -480.f) })
		{
			PlaceModel(Table, At(T.X, T.Y), DYaw + LongSide(Table, 0, 0.f), 76.f, F, nullptr);
			PlaceModel(Chair, At(T.X, T.Y - 85.f), DYaw, 90.f, F, nullptr);
			PlaceModel(Chair, At(T.X, T.Y + 85.f), DYaw + 180.f, 90.f, F, nullptr);
		}

		// Leather bench along the left wall, jukebox against the back wall.
		PlaceModel(Bench, At(-250.f, -745.f), DYaw + LongSide(Bench, 0, 0.f), 110.f, F, nullptr);
		PlaceModel(Jukebox, At(-445.f, -150.f), DYaw, 160.f, F, TEXT("Diner_Jukebox"));

		// Behind the counter: the griddle against the back wall, the soda fountain on the counter.
		UStaticMesh* Grill = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_grill"));
		UStaticMesh* Soda  = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_soda"));
		PlaceModel(Grill, At(-430.f, 420.f), DYaw + LongSide(Grill, 1, 0.f), 95.f, F, TEXT("Diner_Griddle"));
		PlaceModel(Soda, At(-20.f, 140.f) + FVector(0.f, 0.f, 108.f), DYaw + 180.f, 60.f, F, TEXT("Diner_SodaFountain"));

		// Road sign out front, by the sidewalk.
		PlaceModel(Sign, At(860.f, -1150.f), DYaw + LongSide(Sign, 1, 0.f), 900.f, F, TEXT("Diner_RoadSign"));
	}

	BuildForests();
	BuildLamps();
	if (bMountains)
	{
		BuildMountains();
	}

	// ---- Street life: parked cars, mailboxes, signs, traffic lights, poles, benches, hydrants, dumpsters, townspeople.
	{
		FRandomStream SR(Seed + 777);
		UStaticMesh* Pole     = FBDefaults::LoadModel(TEXT("Props"), TEXT("prop_utility_pole"));
		UStaticMesh* Mailbox  = FBDefaults::LoadModel(TEXT("Props"), TEXT("prop_mailbox"));
		UStaticMesh* Bench    = FBDefaults::LoadModel(TEXT("Props"), TEXT("prop_bench"));
		UStaticMesh* Hydrant  = FBDefaults::LoadModel(TEXT("Props"), TEXT("prop_hydrant"));
		UStaticMesh* Dumpster = FBDefaults::LoadModel(TEXT("Props"), TEXT("prop_dumpster"));
		UStaticMesh* Crates   = FBDefaults::LoadModel(TEXT("Props"), TEXT("prop_crates_barrels"));
		UStaticMesh* StopSign = FBDefaults::LoadModel(TEXT("Props"), TEXT("sign_stop"));
		UStaticMesh* Signal   = FBDefaults::LoadModel(TEXT("Props"), TEXT("traffic_light"));

		// Cars: Height = real height; FrontYaw turns the model so its nose points along the driving direction.
		struct FCar { UStaticMesh* Mesh; float Height; float FrontYaw; };
		TArray<FCar> Cars;
		for (const FCar C : { FCar{ FBDefaults::LoadModel(TEXT("Props"), TEXT("car_old_worn")),     145.f, FBDefaults::CarYaw(TEXT("car_old_worn")) },
		                      FCar{ FBDefaults::LoadModel(TEXT("Props"), TEXT("car_rusty_pickup")), 180.f, FBDefaults::CarYaw(TEXT("car_rusty_pickup")) },
		                      // car_cadillac left out too: its import is a gutted shell with crumpled panels.
		                      // car_bmw left out: its import lost the body panels (only the chassis/interior shows).
		                      FCar{ FBDefaults::LoadModel(TEXT("Props"), TEXT("car_wagon")),        148.f, FBDefaults::CarYaw(TEXT("car_wagon")) },
		                      FCar{ FBDefaults::LoadModel(TEXT("Props"), TEXT("car_hatchback")),    140.f, FBDefaults::CarYaw(TEXT("car_hatchback")) } })
		{
			if (C.Mesh)
			{
				Cars.Add(C);
			}
		}
		const FCar PoliceCar{ FBDefaults::LoadModel(TEXT("Props"), TEXT("car_police")), 150.f, FBDefaults::CarYaw(TEXT("car_police")) };
		const TCHAR* F = TEXT("Town/Props");

		// Yaw that lays a model's long side along a direction.
		auto Along = [&LongSide](UStaticMesh* M, const FVector& Dir, float Extra) { return FBGen::YawFacing(Dir) + LongSide(M, 0, Extra); };
		auto PlaceCar = [&](const FCar& C, const FVector& Where, const FVector& Facing)
		{
			if (C.Mesh)
			{
				PlaceModel(C.Mesh, Where, Along(C.Mesh, Facing, C.FrontYaw) + SR.FRandRange(-2.5f, 2.5f), C.Height, F, nullptr);
			}
		};
		auto RandomCar = [&](const FVector& Where, const FVector& Facing)
		{
			if (Cars.Num() > 0)
			{
				PlaceCar(Cars[SR.RandRange(0, Cars.Num() - 1)], Where, Facing);
			}
		};

		// Cars in the driveways (nose in, beside the house).
		for (const FBDefaults::FCarSpot& Spot : FBDefaults::DrivewayCars)
		{
			RandomCar(Base + Spot.At, Spot.Facing);
		}

		// Each lot: parked car at its kerb (in the parking lane, nose in the driving direction), a mailbox.
		for (const FBDefaults::FLot& Lot : FBDefaults::Lots)
		{
			if (!FBDefaults::Streets.IsValidIndex(Lot.Street))
			{
				continue;
			}
			const FBDefaults::FStreet& St = FBDefaults::Streets[Lot.Street];
			const FVector RoadDir(-Lot.ToRoad.Y, Lot.ToRoad.X, 0.f); // right-hand traffic: the kerb is on the car's right
			if (St.Parking > 0.f && SR.FRand() < 0.5f)
			{
				RandomCar(Base + Lot.RoadPoint - Lot.ToRoad * (St.Half() - St.Parking * 0.5f) + RoadDir * SR.FRandRange(-180.f, 180.f), RoadDir);
			}
			else if (St.Sidewalk <= 0.f && SR.FRand() < 0.5f)
			{
				// Country road: the car stands in the farm yard.
				RandomCar(Base + Lot.RoadPoint - Lot.ToRoad * (St.Half() + 450.f) + RoadDir * 300.f, Lot.ToRoad);
			}
			if (Lot.Street != 0) // no mailboxes on Main Street
			{
				PlaceModel(Mailbox, Base + Lot.RoadPoint - Lot.ToRoad * (St.Edge() + 70.f) + RoadDir * 320.f, FBGen::YawFacing(Lot.ToRoad), 125.f, F, nullptr);
			}
		}

		// Main Street parking in front of the shops (both sides, gaps at the crosswalks), the sheriff's cruiser.
		{
			const FBDefaults::FStreet& M = FBDefaults::Streets[0];
			for (float X = M.From + 1500.f; X < M.To - 1200.f; X += 650.f)
			{
				for (const float Side : { 1.f, -1.f })
				{
					bool bNearCross = false;
					for (const float CX : FBDefaults::CrossX)
					{
						bNearCross |= FMath::Abs(X - CX) < 1500.f;
					}
					if (bNearCross || SR.FRand() > 0.32f)
					{
						continue;
					}
					const FVector Facing(Side > 0.f ? 1.f : -1.f, 0.f, 0.f); // north kerb (+Y, on the right of +X): eastbound
					RandomCar(Base + FVector(X, Side * (M.Half() - M.Parking * 0.5f), 0.f), Facing);
				}
			}
			if (Sheriff.IsValid() && PoliceCar.Mesh)
			{
				const FVector P = Sheriff->GetActorLocation();
				PlaceCar(PoliceCar, FVector(P.X - 250.f, Base.Y + M.Half() - M.Parking * 0.5f, Base.Z), FVector(1.f, 0.f, 0.f));
			}
		}

		// The motel's two extra wings (same model; the doors stay shut, nobody gets in) and its parking lot.
		if (Motel.IsValid())
		{
			UStaticMesh* MotelKey = FBDefaults::LoadBuilding(FBDefaults::MotelModel.Name);
			const float WingYaw = Motel->GetActorRotation().Yaw + FBDefaults::MotelModel.Yaw;
			for (const FVector& W : FBDefaults::MotelWings)
			{
				AStaticMeshActor* Wing = PlaceModel(MotelKey, Base + FVector(W.X, W.Y, -3.f), WingYaw + (W.Z > 0.5f ? 180.f : 0.f), FBDefaults::MotelModel.Height, TEXT("Town/Story/MotelWings"), nullptr);
				// A stone footing under the wing, so it sits on the ground instead of hovering over it.
				static UStaticMesh* FootCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
				// The wing is the whole model: its copy of the office and road sign go (one office per motel), and the row
				// slides along so its rooms sit straight across the lot from the motel's rooms.
				AStaticMeshActor* WingCabin = nullptr;
				if (Wing)
				{
					TArray<AActor*> Kids;
					Wing->GetAttachedActors(Kids, true, true);
					Kids.Add(Wing);
					for (AActor* K : Kids)
					{
						AStaticMeshActor* SK = Cast<AStaticMeshActor>(K);
						const UStaticMesh* M = SK ? SK->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
						const FString N = M ? M->GetName() : FString();
						if (N.StartsWith(TEXT("motelCabin_bareMotel")))
						{
							WingCabin = SK;
						}
						else if (N.StartsWith(TEXT("Reception")) || N.StartsWith(TEXT("motelSign")))
						{
							SK->SetActorHiddenInGame(true);
							SK->SetActorEnableCollision(false);
#if WITH_EDITOR
							SK->SetIsTemporarilyHiddenInEditor(true);
#endif
						}
					}
					AStaticMeshActor* MainCabin = nullptr;
					for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
					{
						const UStaticMesh* M = It->GetStaticMeshComponent()->GetStaticMesh();
						if (M && M->GetName().StartsWith(TEXT("motelCabin_bareMotel")) && !Kids.Contains(*It) && It->ActorHasTag(FBGen::GeneratedTag))
						{
#if WITH_EDITOR
							if (It->GetFolderPath().ToString().Contains(TEXT("MotelWings")))
							{
								continue;
							}
#endif
							MainCabin = *It;
							break;
						}
					}
					if (WingCabin && MainCabin && W.Z > 0.5f)
					{
						const float DX = MainCabin->GetComponentsBoundingBox().GetCenter().X - WingCabin->GetComponentsBoundingBox().GetCenter().X;
						Wing->AddActorWorldOffset(FVector(DX, 0.f, 0.f));
					}
				}
				if (Wing && FootCube)
				{
					const FBox WB = WingCabin ? WingCabin->GetComponentsBoundingBox() : Wing->GetComponentsBoundingBox(true, true);
					const FVector WS = WB.GetSize();
					const FVector WC = WB.GetCenter();
					const float Z0 = Base.Z - 45.f;
					const float Z1 = WB.Min.Z + 10.f;
					AStaticMeshActor* Foot = Spawn<AStaticMeshActor>(FVector(WC.X, WC.Y, (Z0 + Z1) * 0.5f), 0.f, TEXT("Town/Story/MotelWings"), [&](AStaticMeshActor* M)
					{
						M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
						M->GetStaticMeshComponent()->SetStaticMesh(FootCube);
						if (Mats.Rock)
						{
							M->GetStaticMeshComponent()->SetMaterial(0, Mats.Rock);
						}
					});
					if (Foot)
					{
						Foot->SetActorScale3D(FVector(WS.X * 0.985f / 100.f, WS.Y * 0.985f / 100.f, FMath::Max(10.f, Z1 - Z0) / 100.f));
					}
				}
			}
			const FBox2D& PL = FBDefaults::MotelParking;
			static UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			static UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
			if (PL.bIsValid && Cube)
			{
				auto Slab = [&](const FVector2D& C, const FVector2D& Size, float Z0, float Z1, UMaterialInterface* Mat)
				{
					AStaticMeshActor* A = Spawn<AStaticMeshActor>(Base + FVector(C.X, C.Y, (Z0 + Z1) * 0.5f), 0.f, TEXT("Town/Story/MotelParking"), [&](AStaticMeshActor* M)
					{
						M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
						M->GetStaticMeshComponent()->SetStaticMesh(Cube);
						if (Mat)
						{
							M->GetStaticMeshComponent()->SetMaterial(0, Mat);
						}
					});
					if (A)
					{
						A->SetActorScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, (Z1 - Z0) / 100.f));
					}
				};
				// Asphalt lot, a touch above the lawn.
				Slab(PL.GetCenter(), PL.GetSize(), -30.f, 4.f, Mats.Asphalt);
				// The way in: a driveway from the lot to the nearest street (a lot with no road to it makes no sense).
				{
					float BestGap = TNumericLimits<float>::Max();
					FVector2D DriveC, DriveSize;
					for (const FBDefaults::FStreet& St : FBDefaults::Streets)
					{
						if (St.Surface != 0)
						{
							continue;
						}
						const FVector2D Mid = PL.GetCenter();
						const float AlongMid = St.bAlongX ? Mid.X : Mid.Y;
						if (AlongMid < St.From || AlongMid > St.To)
						{
							continue;
						}
						// Edges of the lot and of the street's asphalt across the street direction.
						const float LotLo = St.bAlongX ? PL.Min.Y : PL.Min.X, LotHi = St.bAlongX ? PL.Max.Y : PL.Max.X;
						const float RoadLo = St.Pos - St.Half(), RoadHi = St.Pos + St.Half();
						const float Gap = RoadLo > LotHi ? RoadLo - LotHi : (LotLo > RoadHi ? LotLo - RoadHi : -1.f);
						if (Gap < 0.f || Gap >= BestGap || Gap > 4000.f)
						{
							continue;
						}
						BestGap = Gap;
						const float A0 = RoadLo > LotHi ? LotHi - 20.f : RoadHi;
						const float A1 = RoadLo > LotHi ? RoadLo : LotLo + 20.f;
						const float AcrossMid = (A0 + A1) * 0.5f, AcrossLen = A1 - A0 + 40.f;
						// Along the street: near the lot end closest to the office side (stall 1), 6 m wide.
						const float LotAlongLo = St.bAlongX ? PL.Min.X : PL.Min.Y;
						const float LotAlongHi = St.bAlongX ? PL.Max.X : PL.Max.Y;
						const float DriveAlong = (LotAlongHi - LotAlongLo) > 1600.f ? LotAlongLo + 520.f : (LotAlongLo + LotAlongHi) * 0.5f;
						DriveC = St.bAlongX ? FVector2D(DriveAlong, AcrossMid) : FVector2D(AcrossMid, DriveAlong);
						DriveSize = St.bAlongX ? FVector2D(600.f, AcrossLen) : FVector2D(AcrossLen, 600.f);
					}
					if (BestGap < TNumericLimits<float>::Max())
					{
						Slab(DriveC, DriveSize, -30.f, 3.f, Mats.Asphalt);
						UE_LOG(LogFBMapGen, Log, TEXT("Motel lot driveway: %.0f m to the street."), BestGap / 100.f);
					}
				}
				// Stalls along the back (away from the street), nose in; a painted line between each.
				const bool bStreetAtMinY = FMath::Abs(PL.Min.Y) < FMath::Abs(PL.Max.Y);
				const float BackY = bStreetAtMinY ? PL.Max.Y : PL.Min.Y;
				const float InY = bStreetAtMinY ? -1.f : 1.f; // from the back edge towards the street
				constexpr float StallW = 280.f, StallD = 520.f;
				const int32 Stalls = FMath::FloorToInt((PL.GetSize().X - 100.f) / StallW);
				const float X0 = PL.GetCenter().X - Stalls * StallW * 0.5f;
				for (int32 K = 0; K <= Stalls; ++K)
				{
					Slab(FVector2D(X0 + K * StallW, BackY + InY * (StallD * 0.5f + 40.f)), FVector2D(12.f, StallD), 4.f, 5.f, Paint);
				}
				FBDefaults::BrothersCarAt = FVector::ZeroVector;
				for (int32 K = 0; K < Stalls; ++K)
				{
					const FVector StallAt = Base + FVector(X0 + (K + 0.5f) * StallW, BackY + InY * (StallD * 0.5f + 40.f), 5.f);
					if (K == FMath::Min(1, Stalls - 1))
					{
						// The brothers' car: a long black 60s sedan, nose in (trunk to the lane, easy to get at), right by the office.
						UStaticMesh* Sedan = FBDefaults::LoadModel(TEXT("Props"), TEXT("car_brothers"));
						if (!Sedan)
						{
							Sedan = FBDefaults::LoadModel(TEXT("Props"), TEXT("car_old_worn")); // stand-in until car_brothers is imported
						}
						if (AStaticMeshActor* Car = Sedan ? PlaceModel(Sedan, StallAt, Along(Sedan, FVector(0.f, -InY, 0.f), 0.f), 150.f, TEXT("Town/Story/BrothersCar"), TEXT("BrothersCar")) : nullptr)
						{
							FBox CarBox = Car->GetComponentsBoundingBox(true);
							TArray<AActor*> Kids;
							Car->GetAttachedActors(Kids, true, true);
							Car->Tags.AddUnique(TEXT("FBBrothersCar"));
							for (AActor* Kid : Kids)
							{
								CarBox += Kid->GetComponentsBoundingBox(true);
								Kid->Tags.AddUnique(TEXT("FBBrothersCar"));
							}
							FBDefaults::BrothersCarAt = CarBox.GetCenter();
							FBDefaults::BrothersCarFacing = FVector(0.f, -InY, 0.f);
							FBDefaults::BrothersCarHalfLength = CarBox.GetExtent().Y;
							UE_LOG(LogFBMapGen, Log, TEXT("Brothers' car: %s at %s, %.0f x %.0f x %.0f."), *Sedan->GetName(), *CarBox.GetCenter().ToString(), CarBox.GetSize().X, CarBox.GetSize().Y, CarBox.GetSize().Z);
						}
						continue;
					}
					if (SR.FRand() < 0.55f)
					{
						RandomCar(StallAt, FVector(0.f, -InY, 0.f));
					}
				}
			}
		}

		// Behind the story buildings: dumpsters, crates, staff cars.
		auto Front = [](const AActor* B, float X, float Y) { return B ? B->GetActorTransform().TransformPosition(FVector(X, Y, 0.f)) : FVector::ZeroVector; };
		if (Motel.IsValid())
		{
			PlaceModel(Dumpster, Front(Motel.Get(), -900.f, 2300.f), Along(Dumpster, Motel->GetActorRightVector(), 0.f), 150.f, F, nullptr);
		}
		if (Diner.IsValid())
		{
			PlaceModel(Dumpster, Front(Diner.Get(), -900.f, 900.f), Along(Dumpster, Diner->GetActorRightVector(), 0.f), 150.f, F, nullptr);
			PlaceModel(Crates, Front(Diner.Get(), -900.f, -900.f), Diner->GetActorRotation().Yaw, 130.f, F, nullptr);
		}
		if (Library.IsValid())
		{
			PlaceModel(Dumpster, Front(Library.Get(), -1400.f, 700.f), Along(Dumpster, Library->GetActorRightVector(), 0.f), 150.f, F, nullptr);
		}
		if (Barn.IsValid())
		{
			PlaceModel(Crates, Front(Barn.Get(), 300.f, 900.f), Barn->GetActorRotation().Yaw, 130.f, F, nullptr);
			RandomCar(Front(Barn.Get(), 900.f, -900.f), Barn->GetActorRightVector());
		}

		// Intersections: traffic lights on Main Street, all-way stop signs on the avenues.
		int32 Signs = 0;
		for (const FBDefaults::FCrossing& C : FBDefaults::Crossings())
		{
			const FBDefaults::FStreet& EW = FBDefaults::Streets[C.StreetX];
			const FBDefaults::FStreet& NS = FBDefaults::Streets[C.StreetY];
			const bool bSignals = C.StreetX == 0 && Signal != nullptr;
			// The four approaches: driving direction, the street driven on, the street crossed.
			struct FApproach { FVector2D Dir; const FBDefaults::FStreet* On; const FBDefaults::FStreet* Crossed; };
			const FApproach Approaches[] = {
				{ FVector2D(1.f, 0.f), &EW, &NS }, { FVector2D(-1.f, 0.f), &EW, &NS },
				{ FVector2D(0.f, 1.f), &NS, &EW }, { FVector2D(0.f, -1.f), &NS, &EW } };
			for (const FApproach& Ap : Approaches)
			{
				// Is there a street to come from on this side?
				const FBDefaults::FStreet& On = *Ap.On;
				const float Here = On.bAlongX ? C.At.X : C.At.Y;
				const float Back = On.bAlongX ? Here - Ap.Dir.X * 1000.f : Here - Ap.Dir.Y * 1000.f;
				if (Back < On.From || Back > On.To)
				{
					continue;
				}
				const FVector2D Right(-Ap.Dir.Y, Ap.Dir.X);
				const float StopDist = (On.bAlongX ? Ap.Crossed->Half() : Ap.Crossed->Edge()) + 140.f;
				const FVector2D P = C.At - Ap.Dir * StopDist + Right * (On.Half() + 75.f);
				const float FaceYaw = FBGen::YawFacing(FVector(-Ap.Dir.X, -Ap.Dir.Y, 0.f)); // faces the oncoming driver
				if (bSignals)
				{
					PlaceModel(Signal, Base + FVector(P.X, P.Y, 15.f), FaceYaw + FBDefaults::SignalYaw, 520.f, F, nullptr);
				}
				else if (StopSign)
				{
					PlaceModel(StopSign, Base + FVector(P.X, P.Y, 15.f), FaceYaw + FBDefaults::StopSignYaw, 250.f, F, nullptr);
				}
				++Signs;
			}
		}

		// Wooden utility poles behind the sidewalks of the side streets and along the country road.
		for (const FBDefaults::FStreet& St : FBDefaults::Streets)
		{
			if (!St.bTown || St.Lanes > 1)
			{
				continue;
			}
			for (float A = St.From + 1800.f; A < St.To - 1000.f; A += 3600.f)
			{
				bool bNearCross = false;
				for (const FBDefaults::FCrossing& C : FBDefaults::Crossings())
				{
					const float CA = St.bAlongX ? C.At.X : C.At.Y;
					const float CL = St.bAlongX ? C.At.Y : C.At.X;
					bNearCross |= FMath::Abs(CL - St.Pos) < 1.f && FMath::Abs(CA - A) < 1600.f;
				}
				if (bNearCross)
				{
					continue;
				}
				const FVector2D P = St.At(A, St.Edge() + 90.f);
				PlaceModel(Pole, Base + FVector(P.X, P.Y, 0.f), Along(Pole, FVector(St.Dir().X, St.Dir().Y, 0.f), 0.f), 1050.f, F, nullptr);
			}
		}
		for (float X = FBDefaults::TownEast + 1500.f; X <= 20000.f; X += 3800.f)
		{
			PlaceModel(Pole, Base + FVector(X, 650.f, 0.f), Along(Pole, FVector(1.f, 0.f, 0.f), 0.f), 1000.f, F, nullptr);
		}

		// Main Street sidewalks: benches against the shop fronts, hydrants at the kerb.
		{
			const FBDefaults::FStreet& M = FBDefaults::Streets[0];
			int32 K = 0;
			for (float X = M.From + 2000.f; X < M.To - 1500.f; X += 2900.f, ++K)
			{
				bool bNearCross = false;
				for (const float CX : FBDefaults::CrossX)
				{
					bNearCross |= FMath::Abs(X - CX) < 1700.f;
				}
				if (bNearCross)
				{
					continue;
				}
				const float Side = (K % 2 == 0) ? 1.f : -1.f;
				PlaceModel(Bench, Base + FVector(X, Side * (M.Edge() - 55.f), 15.f), Along(Bench, FVector(1.f, 0.f, 0.f), 0.f), 85.f, F, nullptr);
				PlaceModel(Hydrant, Base + FVector(X + 900.f, -Side * (M.Half() + 45.f), 15.f), SR.FRandRange(0.f, 360.f), 75.f, F, nullptr);
			}
			for (const FBDefaults::FStreet& St : FBDefaults::Streets)
			{
				if (!St.bTown || St.Lanes > 1)
				{
					continue;
				}
				for (float A = St.From + 2600.f; A < St.To - 1000.f; A += 5200.f)
				{
					const FVector2D P = St.At(A, -(St.Half() + 45.f));
					PlaceModel(Hydrant, Base + FVector(P.X, P.Y, 15.f), SR.FRandRange(0.f, 360.f), 75.f, F, nullptr);
				}
			}
		}

		// Townspeople: locals on the sidewalks, by the diner, the sheriff's office, the church, on their front walks.
		FBDefaults::LoadPeople();
		TArray<UStaticMesh*> Statues; // people scans that stand still (perfectly posed)
		for (const TCHAR* N : FBDefaults::StaticPeople)
		{
			if (UStaticMesh* M = FBDefaults::LoadModel(TEXT("People"), N))
			{
				Statues.Add(M);
			}
		}
		const int32 Kinds = FBDefaults::People.Num() + Statues.Num();
		auto Person = [&](const FVector& Where, float Yaw, int32 Index)
		{
			if (Kinds == 0)
			{
				return;
			}
			const int32 Pick = Index % Kinds;
			if (Pick >= FBDefaults::People.Num())
			{
				PlaceModel(Statues[Pick - FBDefaults::People.Num()], Where + FVector(0.f, 0.f, 15.f), Yaw + FBDefaults::StaticPersonYaw, SR.FRandRange(168.f, 182.f), TEXT("Gameplay/Townsfolk"), nullptr);
				return;
			}
			const FBDefaults::FPerson& P = FBDefaults::People[Pick];
			USkeletalMesh* M = P.Mesh.Get();
			if (!M)
			{
				return;
			}
			const FBox B = M->GetBounds().GetBox();
			const float Tall = SR.FRandRange(165.f, 186.f);
			const float S = Tall / FMath::Max(1.f, B.GetSize().Z);
			const FVector At(Where.X, Where.Y, Where.Z + 15.f - B.Min.Z * S);
			// The imported (glTF) people face -Y; +90 turns them to face the given yaw.
			// In play each one re-measures its animated skeleton (some animations scale the rig into a giant).
			AFBTownsperson* A = Spawn<AFBTownsperson>(At, Yaw + 90.f, TEXT("Gameplay/Townsfolk"), [&](AFBTownsperson* Actor)
			{
				Actor->TargetHeight = Tall;
				Actor->Look = P.Look;
				Actor->GroundZ = Where.Z + 15.f;
				USkeletalMeshComponent* C = Actor->GetSkeletalMeshComponent();
				C->SetSkeletalMeshAsset(M);
				if (UAnimationAsset* Anim = P.Anim.Get())
				{
					C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
					C->OverrideAnimationData(Anim, true, true, SR.FRandRange(0.f, 2.f), SR.FRandRange(0.9f, 1.1f));
				}
				C->SetCollisionProfileName(TEXT("Pawn"));
#if WITH_EDITOR
				C->SetUpdateAnimationInEditor(true); // show them moving in the editor too, not in a T-pose
#endif
			});
			if (A)
			{
				A->SetActorScale3D(FVector(S));
			}
		};
		int32 Who = 0;
		const FBDefaults::FStreet& M = FBDefaults::Streets[0];
		const float Walk = M.Half() + M.Sidewalk * 0.55f; // the middle of Main Street's sidewalk
		auto OnSidewalk = [&](const AActor* B, float DX, float Yaw)
		{
			if (B)
			{
				const FVector P = B->GetActorLocation();
				Person(FVector(P.X + DX, Base.Y + (P.Y > Base.Y ? Walk : -Walk), Base.Z), Yaw, Who++);
			}
		};
		OnSidewalk(Diner.Get(), -600.f, -90.f);
		OnSidewalk(Diner.Get(), -450.f, 160.f);
		OnSidewalk(Sheriff.Get(), 150.f, -60.f);
		OnSidewalk(Library.Get(), 500.f, 90.f);
		OnSidewalk(Motel.Get(), 900.f, -120.f);
		OnSidewalk(Church.Get(), -900.f, -90.f);
		OnSidewalk(Church.Get(), -750.f, 180.f);
		// More life on Main Street.
		OnSidewalk(Diner.Get(), 700.f, 90.f);
		OnSidewalk(Sheriff.Get(), -700.f, 120.f);
		OnSidewalk(Library.Get(), -400.f, -90.f);
		OnSidewalk(Motel.Get(), 1600.f, 60.f);
		OnSidewalk(Motel.Get(), -900.f, -150.f);
		OnSidewalk(Church.Get(), 800.f, 30.f);
		for (int32 i = 0; i < FBDefaults::Lots.Num() && Who < 28; i += 2)
		{
			const FBDefaults::FLot& Lot = FBDefaults::Lots[i];
			if (!FBDefaults::Streets.IsValidIndex(Lot.Street))
			{
				continue;
			}
			const FBDefaults::FStreet& St = FBDefaults::Streets[Lot.Street];
			const float Out = St.Sidewalk > 0.f ? St.Half() + St.Sidewalk * 0.5f : St.Half() + 300.f;
			Person(Base + Lot.RoadPoint - Lot.ToRoad * Out, FBGen::YawFacing(Lot.ToRoad) + SR.FRandRange(-40.f, 40.f), Who++);
		}
		UE_LOG(LogFBMapGen, Log, TEXT("Street life: %d lots, %d car kinds, %d intersection signs, %d people kinds."), FBDefaults::Lots.Num(), Cars.Num(), Signs, Kinds);
	}

	// ---- Grass and rocks (real scans), scattered over the open ground. Done last so it avoids everything built above.
	{
		// Places that must stay clear: buildings, roads, fences, the cemetery, lamps, placed models.
		TArray<FBox> Keep;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* A = *It;
			if (!A->ActorHasTag(FBGen::GeneratedTag) || A->IsA<AFBForest>() || A->IsA<AFBMountains>() || A->IsA<AFBAtmosphere>())
			{
				continue;
			}
#if WITH_EDITOR
			if (A->GetActorLabel() == TEXT("HF_Ground"))
			{
				continue;
			}
#endif
			const bool bBlocks = A->IsA<AFBBuilding>() || A->IsA<AFBRoad>() || A->IsA<AFBIntersection>() || A->IsA<AFBFence>() || A->IsA<AFBCemetery>()
				|| A->IsA<AFBStreetLamp>() || A->IsA<AStaticMeshActor>() || A->IsA<AFBDoor>();
			if (bBlocks)
			{
				// Bounds of freshly built pieces aren't updated until the next frame: refresh them first.
				FBox Box(ForceInit);
				TInlineComponentArray<UPrimitiveComponent*> Prims(A);
				for (UPrimitiveComponent* C : Prims)
				{
					if (C && C->IsRegistered())
					{
						C->UpdateBounds();
						Box += C->Bounds.GetBox();
					}
				}
				if (Box.IsValid)
				{
					Keep.Add(Box.ExpandBy(FVector(120.f, 120.f, 0.f)));
				}
			}
		}
		auto IsFree = [&Keep](const FVector& P)
		{
			for (const FBox& B : Keep)
			{
				if (P.X > B.Min.X && P.X < B.Max.X && P.Y > B.Min.Y && P.Y < B.Max.Y)
				{
					return false;
				}
			}
			return true;
		};

		// ---- Trees in the town's open ground (yards, verges, the empty lots): real Megaplant trees, not too close
		// to houses, roads or each other.
		if (SkeletalTreeModels.Num() > 0)
		{
			FRandomStream TR(Seed * 31 + 7);
			TArray<FVector> Planted;
			auto ClearAround = [&](const FVector& P, float R)
			{
				for (const FVector& D : { FVector(0.f, 0.f, 0.f), FVector(R, 0.f, 0.f), FVector(-R, 0.f, 0.f), FVector(0.f, R, 0.f), FVector(0.f, -R, 0.f) })
				{
					if (!IsFree(P + D))
					{
						return false;
					}
				}
				for (const FVector& Q : Planted)
				{
					if (FVector::DistSquared2D(P, Q) < FMath::Square(750.f))
					{
						return false;
					}
				}
				return true;
			};
			TArray<FVector> Candidates;
			for (const FVector2D& Y : FBDefaults::BackyardTrees)
			{
				Candidates.Add(FVector(Y.X, Y.Y, 0.f));
			}
			for (float X = FBDefaults::TownWest - 1500.f; X < FBDefaults::CrossEnd; X += 850.f)
			{
				for (float Y = -FBDefaults::CrossEnd; Y < FBDefaults::CrossEnd; Y += 850.f)
				{
					if (TR.FRand() <= 0.7f)
					{
						Candidates.Add(FVector(X + TR.FRandRange(-320.f, 320.f), Y + TR.FRandRange(-320.f, 320.f), 0.f));
					}
				}
			}
			for (const FVector& Cand : Candidates)
			{
				if (Planted.Num() >= 120)
				{
					break;
				}
				{
					FVector P = Base + Cand;
					if (!ClearAround(P, 300.f))
					{
						continue;
					}
					P.Z = GroundAt(P) - 10.f;
					if (P.Z < Base.Z - 60.f)
					{
						continue; // the lake basin
					}
					USkeletalMesh* TreeMesh = SkeletalTreeModels[TR.RandRange(0, SkeletalTreeModels.Num() - 1)];
					if (!TreeMesh)
					{
						continue;
					}
					ASkeletalMeshActor* T = Spawn<ASkeletalMeshActor>(P, TR.FRandRange(0.f, 360.f), TEXT("Nature/TownTrees"), [&](ASkeletalMeshActor* A)
					{
						USkeletalMeshComponent* SK = A->GetSkeletalMeshComponent();
						SK->SetSkeletalMeshAsset(TreeMesh);
						SK->SetCollisionProfileName(TEXT("BlockAll"));
						SK->bReceivesDecals = false;
						// Skinned trees in ray tracing each keep their own geometry in video memory: leave them out
						// (they still cast virtual shadow map shadows and get Lumen light), and don't tick a pose.
						SK->bVisibleInRayTracing = false;
						SK->bAffectDistanceFieldLighting = false;
						SK->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
					});
					if (T)
					{
						const float Scale = TR.FRandRange(0.8f, 1.15f);
						T->SetActorScale3D(FVector(Scale));
						Planted.Add(P);
						// Its trunk keeps the grass and rocks away.
						Keep.Add(FBox(P - FVector(150.f, 150.f, 0.f), P + FVector(150.f, 150.f, 2000.f)));
					}
				}
			}
			UE_LOG(LogFBMapGen, Log, TEXT("Town trees: %d."), Planted.Num());
		}

		// The ground is 500 m x 450 m around X = 3000; the mountains start at the ellipse (23500, 21000).
		const FVector Mid = Base + FVector(3000.f, 0.f, 0.f);
		auto InMap = [&Mid](const FVector& P, float Margin)
		{
			const float DX = (P.X - Mid.X) / (23500.f - Margin);
			const float DY = (P.Y - Mid.Y) / (21000.f - Margin);
			return DX * DX + DY * DY < 1.f;
		};

		// One container actor per kind, holding instanced meshes (fast, one draw per mesh).
		auto MakeScatter = [this](const TCHAR* Label) -> AStaticMeshActor*
		{
			AStaticMeshActor* A = Spawn<AStaticMeshActor>(Base, 0.f, TEXT("Nature"), [](AStaticMeshActor* M)
			{
				M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
			});
#if WITH_EDITOR
			if (A)
			{
				A->SetActorLabel(Label);
			}
#endif
			return A;
		};
		auto AddLayer = [](AStaticMeshActor* Holder, UStaticMesh* Mesh, bool bCollision, bool bShadows, float CullEnd) -> UHierarchicalInstancedStaticMeshComponent*
		{
			if (!Holder || !Mesh)
			{
				return nullptr;
			}
			UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder, MakeUniqueObjectName(Holder, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Mesh->GetFName()));
			H->SetStaticMesh(Mesh);
			H->SetMobility(EComponentMobility::Static);
			H->SetupAttachment(Holder->GetRootComponent());
			H->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			H->SetCastShadow(bShadows);
			if (CullEnd > 0.f)
			{
				H->SetCullDistances(FMath::RoundToInt(CullEnd * 0.7f), FMath::RoundToInt(CullEnd));
			}
			H->CreationMethod = EComponentCreationMethod::Instance;
			Holder->AddInstanceComponent(H);
			H->RegisterComponent();
			return H;
		};

		FRandomStream R(Seed + 4242);
		const FVector2D Half(24000.f, 21500.f);
		auto RandomPoint = [&]() { return Mid + FVector(R.FRandRange(-Half.X, Half.X), R.FRandRange(-Half.Y, Half.Y), 0.f); };

		// Adds one instance of a mesh part so that the part itself (not the shared origin of its model)
		// stands at P with its bottom on the ground, turned by Rot, at scale S.
		auto AddAt = [this](UHierarchicalInstancedStaticMeshComponent* H, const UStaticMesh* Part, const FVector& P, const FRotator& Rot, float S, float Sink)
		{
			const FBox B = Part->GetBoundingBox();
			const FVector Pivot(B.GetCenter().X, B.GetCenter().Y, B.Min.Z);
			const FVector Loc = (P - Base) - Rot.RotateVector(Pivot * S) - FVector(0.f, 0.f, Sink);
			H->AddInstance(FTransform(Rot, Loc, FVector(S)));
		};

		// Every part of a scan becomes its own variant (a rock pile = several single rocks).
		auto Variants = [](const TCHAR* Name) -> TArray<UStaticMesh*>
		{
			TArray<UStaticMesh*> Out;
			if (UStaticMesh* Key = FBDefaults::LoadModel(TEXT("Nature"), Name))
			{
				for (UStaticMesh* P : FBDefaults::Parts(Key))
				{
					if (!FBDefaults::IsGroundPlane(P))
					{
						Out.Add(P);
					}
				}
			}
			return Out;
		};

		// Grass: tufts in clumps (meadow patches, thinner in the middle of town), no collision.
		{
			const TArray<UStaticMesh*> Grass = Variants(TEXT("grass_patches"));
			if (Grass.Num() > 0)
			{
				AStaticMeshActor* Holder = MakeScatter(TEXT("HF_Grass"));
				TArray<UHierarchicalInstancedStaticMeshComponent*> VariantLayers;
				for (UStaticMesh* G : Grass)
				{
					VariantLayers.Add(AddLayer(Holder, G, false, false, 9000.f));
				}
				int32 Added = 0;
				for (int32 Try = 0; Try < 120000 && Added < 18000; ++Try)
				{
					FVector P = RandomPoint();
					const float Clump = FMath::PerlinNoise2D(FVector2D(P.X, P.Y) / 2500.f + FVector2D(Seed * 0.37f, 3.1f));
					const bool bTown = FMath::Abs(P.Y - Base.Y) < 11000.f && P.X > Base.X + FBDefaults::TownWest - 3800.f && P.X < Base.X + FBDefaults::TownEast + 3800.f;
					if (Clump < (bTown ? 0.15f : -0.2f) || !InMap(P, 1500.f) || !IsFree(P))
					{
						continue;
					}
					if (Terrain.IsValid() && Terrain->IsUnderWater(FVector2D(P.X, P.Y), 10.f))
					{
						continue;
					}
					P.Z = GroundAt(P);
					const int32 V = R.RandRange(0, Grass.Num() - 1);
					if (!VariantLayers[V])
					{
						continue;
					}
					// Real tufts are 40-110 cm across; the scan's patch width sets the scale.
					const FVector Size = Grass[V]->GetBoundingBox().GetSize();
					const float S = R.FRandRange(60.f, 120.f) / FMath::Max(1.f, FMath::Max(Size.X, Size.Y));
					AddAt(VariantLayers[V], Grass[V], P, FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), S, 3.f);
					++Added;
				}
				UE_LOG(LogFBMapGen, Log, TEXT("Grass: %d tufts (%d kinds)."), Added, Grass.Num());
			}
		}

		// Rocks: single boulders everywhere (more in the woods and near the mountains), the rocks of the scanned
		// piles on the forest floor, a few huge outcrops at the foot of the mountains. They block the player.
		{
			AStaticMeshActor* Holder = MakeScatter(TEXT("HF_Rocks"));
			struct FRockKind { const TCHAR* Name; int32 Count; float MinH, MaxH; float EdgeBias; };
			const FRockKind Kinds[] = {
				{ TEXT("rock_gray_big"),  260,  40.f, 260.f, 0.3f },
				{ TEXT("rock_pile_dry"),  220,  25.f, 120.f, 0.5f },
				{ TEXT("rock_moss_pile"), 160,  40.f, 160.f, 0.6f },
				{ TEXT("rock_stranska"),   24, 500.f, 1400.f, 1.0f },
			};
			for (const FRockKind& K : Kinds)
			{
				const TArray<UStaticMesh*> Rocks = Variants(K.Name);
				TArray<UHierarchicalInstancedStaticMeshComponent*> VariantLayers;
				for (UStaticMesh* Rock : Rocks)
				{
					VariantLayers.Add(AddLayer(Holder, Rock, true, true, 0.f));
				}
				if (Rocks.Num() == 0)
				{
					continue;
				}
				int32 Added = 0;
				for (int32 Try = 0; Try < K.Count * 40 && Added < K.Count; ++Try)
				{
					FVector P = RandomPoint();
					if (Terrain.IsValid() && Terrain->IsUnderWater(FVector2D(P.X, P.Y), 200.f))
					{
						continue; // no rocks in or right at the lake (they'd block the water)
					}
					P.Z = GroundAt(P);
					// Distance out from the centre (0 = middle of town, 1 = foot of the mountains).
					const float DX = (P.X - Mid.X) / 23500.f;
					const float DY = (P.Y - Mid.Y) / 21000.f;
					const float Out = FMath::Sqrt(DX * DX + DY * DY);
					if (R.FRand() > FMath::Lerp(1.f - K.EdgeBias, 1.f, FMath::Clamp(Out, 0.f, 1.f)) || Out < 0.25f)
					{
						continue; // keep them out of the town, and towards the edges for the big ones
					}
					const FVector RelP = P - Base;
					if (RelP.X > FBDefaults::TownWest - 3000.f && RelP.X < FBDefaults::TownEast + 3000.f && FMath::Abs(RelP.Y) < FBDefaults::CrossEnd + 1500.f)
					{
						continue; // never in town (yards, sidewalks, corners)
					}
					if (!InMap(P, K.EdgeBias >= 1.f ? -1500.f : 900.f) || !IsFree(P))
					{
						continue;
					}
					const int32 V = R.RandRange(0, Rocks.Num() - 1);
					if (!VariantLayers[V])
					{
						continue;
					}
					const float RockZ = FMath::Max(1.f, Rocks[V]->GetBoundingBox().GetSize().Z);
					const float S = R.FRandRange(K.MinH, K.MaxH) / RockZ;
					const FRotator Rot(R.FRandRange(-6.f, 6.f), R.FRandRange(0.f, 360.f), R.FRandRange(-6.f, 6.f));
					AddAt(VariantLayers[V], Rocks[V], P, Rot, S, RockZ * S * 0.15f); // partly buried, like real rocks
					++Added;
				}
				UE_LOG(LogFBMapGen, Log, TEXT("Rocks %s: %d (%d kinds)."), K.Name, Added, Rocks.Num());
			}
		}
	}

	if (bGameplayActors)
	{
		BuildGameplay();
	}

	UE_LOG(LogFBMapGen, Log, TEXT("Harlow Falls generated: %d actors."), SpawnedCount);
}

// ------------------------------------------------------------------ Helpers

void AFBMapGenerator::LoadMaterials()
{
	auto Load = [](const TCHAR* Name) -> UMaterialInterface*
	{
		const FString Path = FString::Printf(TEXT("/Game/FallenBrothers/Materials/%s.%s"), Name, Name);
		return Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	};
	Mats.Asphalt = Load(TEXT("MI_Asphalt"));
	Mats.Paving = Load(TEXT("MI_Paving"));
	Mats.Grass = Load(TEXT("MI_Grass"));
	Mats.Ground = Load(TEXT("MI_Ground"));
	Mats.ForestGround = Load(TEXT("MI_ForestGround"));
	Mats.WoodSiding = Load(TEXT("MI_WoodSiding"));
	Mats.Bricks = Load(TEXT("MI_Bricks"));
	Mats.Planks = Load(TEXT("MI_Planks"));
	Mats.Roofing = Load(TEXT("MI_Roofing"));
	Mats.WoodFloor = Load(TEXT("MI_WoodFloor"));
	Mats.Rock = Load(TEXT("MI_Rock"));
	Mats.Snow = Load(TEXT("MI_Snow"));
}

template <class T>
T* AFBMapGenerator::Spawn(const FVector& Location, float Yaw, const TCHAR* Folder, TFunctionRef<void(T*)> Setup)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FTransform Xf(FRotator(0.f, Yaw, 0.f), Location);
	T* Actor = World->SpawnActorDeferred<T>(T::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}

	Setup(Actor); // Properties are set before construction, so blockout pieces build with them.
	Actor->Tags.AddUnique(FBGen::GeneratedTag);
	Actor->FinishSpawning(Xf);
#if WITH_EDITOR
	Actor->SetFolderPath(FName(*FString::Printf(TEXT("HarlowFalls/%s"), Folder)));
#endif
	++SpawnedCount;
	return Actor;
}

FVector AFBMapGenerator::InBuilding(const AActor* Building, const FVector& Local)
{
	if (!Building)
	{
		return Local;
	}
	FVector P = Building->GetActorTransform().TransformPosition(Local);

	// A building replaced by a real model: put things that lie on the floor onto the model's real floor.
	if (Building->ActorHasTag(TEXT("FBReplaced")) && Local.Z < 100.f && Building->GetWorld())
	{
		FCollisionQueryParams Params(TEXT("FBFloor"), true, Building);
		FHitResult Hit;
		const FVector From = P + FVector(0.f, 0.f, 260.f);
		const FVector To = P - FVector(0.f, 0.f, 200.f);
		if (Building->GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
		{
			P.Z = Hit.ImpactPoint.Z + FMath::Max(Local.Z, 2.f);
		}
	}
	return P;
}

FVector AFBMapGenerator::FreeSpotInBox(UWorld* World, const FBox& Box, float Floor0, float BaseYaw, const FVector& HalfSize, float* OutYaw, const AActor* Ignore)
{
	const FVector Wanted = Box.GetCenter();
	if (OutYaw)
	{
		*OutYaw = BaseYaw;
	}
	const FVector Origin = Box.GetCenter(), Extent = Box.GetExtent();
	if (!World || Extent.X < 60.f || Extent.Y < 60.f)
	{
		return FVector(Wanted.X, Wanted.Y, Floor0);
	}
	TArray<FVector> Candidates;
	for (int32 ix = -8; ix <= 8; ++ix)
	{
		for (int32 iy = -8; iy <= 8; ++iy)
		{
			Candidates.Add(FVector(Origin.X + Extent.X * 0.85f * ix / 8.f, Origin.Y + Extent.Y * 0.85f * iy / 8.f, Floor0));
		}
	}
	Candidates.Sort([&Wanted](const FVector& A, const FVector& B) { return FVector::DistSquared2D(A, Wanted) < FVector::DistSquared2D(B, Wanted); });

	// Real models often only have per-polygon (complex) collision, so everything here is a complex line trace.
	FCollisionQueryParams Params(TEXT("FBFreeSpot"), true);
	if (Ignore)
	{
		Params.AddIgnoredActor(Ignore);
	}
	auto Clear = [&](const FVector& From, const FVector& Dir, float Dist) -> bool
	{
		FHitResult H;
		return !World->LineTraceSingleByChannel(H, From, From + Dir * Dist, ECC_Visibility, Params);
	};
	for (const FVector& C : Candidates)
	{
		FHitResult Down;
		if (!World->LineTraceSingleByChannel(Down, C + FVector(0.f, 0.f, 220.f), C - FVector(0.f, 0.f, 150.f), ECC_Visibility, Params))
		{
			continue;
		}
		const FVector FloorAt = Down.ImpactPoint;
		if (FMath::Abs(FloorAt.Z - Floor0) > 120.f)
		{
			continue;
		}
		FHitResult Up;
		if (!World->LineTraceSingleByChannel(Up, FloorAt + FVector(0.f, 0.f, 60.f), FloorAt + FVector(0.f, 0.f, 900.f), ECC_Visibility, Params)
			|| Up.ImpactPoint.Z - FloorAt.Z < 230.f)
		{
			continue; // outdoors, or too low
		}
		for (int32 Turn = 0; Turn < 4; ++Turn)
		{
			const float Yaw = BaseYaw + 90.f * Turn;
			const FRotator R(0.f, Yaw, 0.f);
			const FVector Fwd = R.Vector(), Right = R.RotateVector(FVector::RightVector);
			bool bFree = true;
			// One flat floor under all of it (and under the chair): not a gap inside a wall, not half on a bed or a step.
			for (const FVector2D& K : { FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1), FVector2D(2.4f, 0) })
			{
				const FVector Q = FloorAt + Fwd * (HalfSize.X * 0.9f * K.X) + Right * (HalfSize.Y * 0.9f * K.Y);
				FHitResult F;
				if (!World->LineTraceSingleByChannel(F, Q + FVector(0.f, 0.f, 150.f), Q - FVector(0.f, 0.f, 60.f), ECC_Visibility, Params)
					|| FMath::Abs(F.ImpactPoint.Z - FloorAt.Z) > 4.f)
				{
					bFree = false;
					break;
				}
			}
			for (const float H : { 25.f, 70.f, 130.f })
			{
				if (!bFree)
				{
					break;
				}
				const FVector At = FloorAt + FVector(0.f, 0.f, H);
				// Its own size all round (corners too), and 1.8 m free in front (-X) for the chair and the player.
				// Its back (-X) may touch a wall; 1.8 m free in front (+X) for the chair and the player.
				bFree = Clear(At, -Fwd, HalfSize.X + 10.f) && Clear(At, Right, HalfSize.Y + 10.f) && Clear(At, -Right, HalfSize.Y + 10.f)
					&& Clear(At, Fwd, 180.f)
					&& Clear(At, (-Fwd * HalfSize.X + Right * HalfSize.Y).GetSafeNormal(), FVector2D(HalfSize.X, HalfSize.Y).Size() + 10.f)
					&& Clear(At, (-Fwd * HalfSize.X - Right * HalfSize.Y).GetSafeNormal(), FVector2D(HalfSize.X, HalfSize.Y).Size() + 10.f)
					&& Clear(At + Fwd * 120.f, Right, 60.f) && Clear(At + Fwd * 120.f, -Right, 60.f);
				if (!bFree)
				{
					break;
				}
			}
			if (bFree)
			{
				if (OutYaw)
				{
					*OutYaw = Yaw;
				}
				return FloorAt + FVector(0.f, 0.f, 1.f);
			}
		}
	}
	return FVector::ZeroVector; // nothing free
}


FVector AFBMapGenerator::FreeSpotInside(const AActor* Building, const FVector& Local, const FVector& HalfSize, float* OutYaw)
{
	const FVector Wanted = InBuilding(Building, Local);
	if (OutYaw)
	{
		*OutYaw = Building ? Building->GetActorRotation().Yaw : 0.f;
	}
	if (!Building || !Building->GetWorld())
	{
		return Wanted;
	}
	FVector Origin, Extent;
	Building->GetActorBounds(false, Origin, Extent);
	const FVector Spot = FreeSpotInBox(Building->GetWorld(), FBox(Origin - Extent, Origin + Extent), Building->GetActorLocation().Z,
		Building->GetActorRotation().Yaw, HalfSize, OutYaw, Building->ActorHasTag(TEXT("FBReplaced")) ? Building : nullptr);
	return Spot.IsZero() ? Wanted : Spot;
}

// ------------------------------------------------------------------ Ground + roads

void AFBMapGenerator::BuildGround()
{
	// Rolling ground: the town and the roads flat, hills around, a lake in the north-east woods.
	const FVector Mid = Base + FVector(3000.f, 0.f, 0.f); // same centre as the mountains
	const FVector2D B2(Base.X, Base.Y);
	AFBTerrain* T = Spawn<AFBTerrain>(Mid, 0.f, TEXT("Ground"), [&](AFBTerrain* G)
	{
		G->Size = FVector2D(50000.f, 45000.f);
		G->EdgeRadius = FVector2D(23500.f, 21000.f);
		G->Seed = Seed;
		G->HillHeight = bHills ? 2600.f : 0.f;
		G->GrassMaterial = Mats.Grass;
		G->SlopeMaterial = Mats.ForestGround ? Mats.ForestGround : Mats.Ground;
		G->ShoreMaterial = Mats.Ground ? Mats.Ground : Mats.Grass;

		// The town (all streets and lots) is flat, plus the yards of the outskirts buildings.
		G->FlatAreas.Add(FBox2D(B2 + FVector2D(FBDefaults::TownWest - 3800.f, -FBDefaults::CrossEnd - 1500.f), B2 + FVector2D(FBDefaults::TownEast + 3800.f, FBDefaults::CrossEnd + 1500.f)));
		G->FlatAreas.Add(FBox2D(B2 + FVector2D(9000.f, -5200.f), B2 + FVector2D(22500.f, 3600.f)));   // farms, burned house, barn, mill
		G->FlatAreas.Add(FBox2D(B2 + FVector2D(-6000.f, 14200.f), B2 + FVector2D(-2400.f, 17600.f))); // the hunter's cabin
		for (const FBDefaults::FStreet& S : FBDefaults::Streets)
		{
			const FVector2D A = B2 + S.At(S.From, 0.f);
			const FVector2D E = B2 + S.At(S.To, 0.f);
			FFBFlatStrip Strip;
			Strip.A = A;
			Strip.B = E;
			Strip.HalfWidth = S.Edge() + 400.f;
			G->FlatStrips.Add(Strip);
		}

		G->bLake = bLake;
		G->LakeCenter = B2 + FVector2D(13500.f, 9200.f);
		G->LakeRadius = FVector2D(4800.f, 3200.f);
		G->LakeDepth = 700.f;
		G->WaterLevel = -40.f;
		if (bLake)
		{
			// Keep the shore gentle (no hill right at the water).
			G->FlatAreas.Add(FBox2D(G->LakeCenter - G->LakeRadius * 1.25f, G->LakeCenter + G->LakeRadius * 1.25f));
		}
	});
	Terrain = T;
#if WITH_EDITOR
	if (T)
	{
		T->SetActorLabel(TEXT("HF_Terrain"));
	}
#endif
}

float AFBMapGenerator::GroundAt(const FVector& World) const
{
	if (const AFBTerrain* T = Terrain.Get())
	{
		return T->GetActorLocation().Z + T->HeightAt(FVector2D(World.X, World.Y));
	}
	return Base.Z;
}

void AFBMapGenerator::BuildMountains()
{
	// Centred on the ground (500 m x 450 m around X = 3000). Foothills start just inside its edges,
	// so there is never a gap between the ground and the slopes.
	Spawn<AFBMountains>(Base + FVector(3000.f, 0.f, 0.f), 0.f, TEXT("Mountains"), [&](AFBMountains* M)
	{
		M->InnerRadius = FVector2D(23500.f, 21000.f);
		M->PeakHeight = MountainHeight;
		M->Seed = Seed;
		M->RockMaterial = MountainRockMaterial ? MountainRockMaterial.Get() : (Mats.Rock ? Mats.Rock : Mats.Ground);
		M->SnowMaterial = MountainSnowMaterial ? MountainSnowMaterial.Get() : Mats.Snow;
		M->SnowLine = MountainHeight * 0.34f;   // snow caps on the higher ridges
		M->TreeLine = MountainHeight * 0.42f;   // forests up the slopes, bare rock and snow above
		M->TreeCount = 40000;
		M->TreeDepth = 0.5f;
		M->TreeMeshes = SkeletalTreeModels;
	});
}

void AFBMapGenerator::BuildRoads()
{
	using FBDefaults::FStreet;
	const TArray<FBDefaults::FCrossing> Cross = FBDefaults::Crossings();
	auto SurfaceOf = [this](const FStreet& S) -> UMaterialInterface*
	{
		if (S.Surface == 1)
		{
			return Mats.Ground ? Mats.Ground : Mats.Asphalt;
		}
		if (S.Surface == 2)
		{
			return Mats.ForestGround ? Mats.ForestGround : Mats.Ground;
		}
		return Mats.Asphalt;
	};

	// ---- Street pieces between the intersections.
	for (int32 Index = 0; Index < FBDefaults::Streets.Num(); ++Index)
	{
		const FStreet& S = FBDefaults::Streets[Index];

		// Cut out every intersection this street passes through.
		TArray<FVector2D> Cuts;
		for (const FBDefaults::FCrossing& C : Cross)
		{
			const FStreet& EW = FBDefaults::Streets[C.StreetX];
			const FStreet& NS = FBDefaults::Streets[C.StreetY];
			if (S.bAlongX && FMath::Abs(C.At.Y - S.Pos) < 1.f && C.At.X >= S.From - NS.Half() - 1.f && C.At.X <= S.To + NS.Half() + 1.f)
			{
				Cuts.Add(FVector2D(C.At.X - NS.Half(), C.At.X + NS.Half()));
			}
			else if (!S.bAlongX && FMath::Abs(C.At.X - S.Pos) < 1.f && C.At.Y >= S.From - EW.Edge() - 1.f && C.At.Y <= S.To + EW.Edge() + 1.f)
			{
				Cuts.Add(FVector2D(C.At.Y - EW.Edge(), C.At.Y + EW.Edge()));
			}
		}
		Cuts.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });

		float Cursor = S.From;
		bool bAfterCut = false;
		auto Piece = [&](float From, float To, bool bStartCross, bool bEndCross)
		{
			if (To - From < 50.f)
			{
				return;
			}
			const FVector2D P = S.At(From, 0.f);
			Spawn<AFBRoad>(Base + FVector(P.X, P.Y, 0.f), S.bAlongX ? 0.f : 90.f, TEXT("Roads"), [&](AFBRoad* R)
			{
				R->Length = To - From;
				R->RoadWidth = S.Asphalt;
				R->SidewalkWidth = S.Sidewalk;
				R->bCenterLine = S.Surface == 0;
				R->bRealMarkings = S.Surface == 0;
				R->LanesPerDirection = S.Lanes;
				R->LaneWidth = 350.f;
				R->ParkingWidth = S.Parking;
				R->bCrosswalkAtStart = S.bTown && bStartCross;
				R->bCrosswalkAtEnd = S.bTown && bEndCross;
				R->AsphaltMaterial = SurfaceOf(S);
				R->SidewalkMaterial = Mats.Paving;
			});
		};
		for (const FVector2D& Cut : Cuts)
		{
			Piece(Cursor, FMath::Min(Cut.X, S.To), bAfterCut, true);
			Cursor = FMath::Max(Cursor, Cut.Y);
			bAfterCut = true;
		}
		Piece(Cursor, S.To, bAfterCut, false);

		// The street's whole strip is taken (asphalt + sidewalks + 1 m).
		const FVector2D A = S.At(S.From, -S.Edge() - 100.f);
		const FVector2D E = S.At(S.To, S.Edge() + 100.f);
		FBDefaults::Occupied.Add(FBox2D(FVector2D(FMath::Min(A.X, E.X), FMath::Min(A.Y, E.Y)), FVector2D(FMath::Max(A.X, E.X), FMath::Max(A.Y, E.Y))));
	}

	// ---- Intersection squares: the cross street's asphalt through the main street's asphalt and sidewalks.
	for (const FBDefaults::FCrossing& C : Cross)
	{
		const FStreet& EW = FBDefaults::Streets[C.StreetX];
		const FStreet& NS = FBDefaults::Streets[C.StreetY];
		Spawn<AFBIntersection>(Base + FVector(C.At.X, C.At.Y, 0.f), 0.f, TEXT("Roads"), [&](AFBIntersection* I)
		{
			I->AsphaltSize = FVector2D(NS.Asphalt, EW.Asphalt + 2.f * EW.Sidewalk);
			I->Sidewalk = FVector2D::ZeroVector;
			I->AsphaltMaterial = Mats.Asphalt;
			I->SidewalkMaterial = Mats.Paving;
		});
	}
}

// ------------------------------------------------------------------ Town

AFBBuilding* AFBMapGenerator::SpawnHouse(const FVector& Center, float Yaw, FRandomStream& Random)
{
	return Spawn<AFBBuilding_House>(Base + Center, Yaw, TEXT("Town/Houses"), [&](AFBBuilding_House* H)
	{
		H->Width = FMath::RoundToFloat(Random.FRandRange(900.f, 1300.f) / 50.f) * 50.f;
		H->Depth = FMath::RoundToFloat(Random.FRandRange(800.f, 1100.f) / 50.f) * 50.f;
		H->Floors = Random.FRand() < 0.65f ? 2 : 1;
		H->RoofRise = Random.FRandRange(220.f, 330.f);
		H->WindowSpacing = Random.FRandRange(260.f, 330.f);
		H->WallColor = FLinearColor::MakeFromHSV8((uint8)Random.RandRange(0, 255), (uint8)Random.RandRange(10, 50), (uint8)Random.RandRange(150, 200));
		H->WallMaterial = Random.FRand() < 0.7f ? Mats.WoodSiding : Mats.Bricks;
		H->RoofMaterial = Mats.Roofing;
		H->FloorMaterial = Mats.WoodFloor;
		H->bInteriorLight = Random.FRand() < 0.55f; // Some houses are empty and dark.
		H->InteriorLightIntensity = Random.FRandRange(6.f, 16.f);
	});
}

bool AFBMapGenerator::SpawnModelHouse(const FFBTownBuildingType& Type, const FVector& RoadPoint, const FVector& ToRoad, float FrontDistance, bool bTestOnly, FBox2D* OutFootprint)
{
	const float Yaw = FBGen::YawFacing(ToRoad) + Type.FrontYaw;
	const FBox Box = FBDefaults::ModelBox(Type.Mesh).TransformBy(FTransform(FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, FVector(Type.Scale)));

	const int32 N = FBGen::AxisOf(ToRoad);
	const int32 A = 1 - N;
	FVector L = FVector::ZeroVector;
	L[A] = RoadPoint[A] - Box.GetCenter()[A];
	L[N] = (ToRoad[N] > 0.f) ? RoadPoint[N] - FrontDistance - Box.Max[N]
	                         : RoadPoint[N] + FrontDistance - Box.Min[N];
	// Every house stands on a stone foundation (a real house never sits straight on the lawn).
	constexpr float FoundationRaise = 35.f;
	L.Z = RoadPoint.Z - Box.Min.Z - 5.f + FoundationRaise;

	const FBox2D Footprint(FVector2D(L.X + Box.Min.X, L.Y + Box.Min.Y), FVector2D(L.X + Box.Max.X, L.Y + Box.Max.Y));
	if (OutFootprint)
	{
		*OutFootprint = Footprint;
	}
	if (bTestOnly)
	{
		return true;
	}

	// All parts of the model with the same transform (merged models have just one part).
	AStaticMeshActor* First = nullptr;
	for (UStaticMesh* Part : FBDefaults::Parts(Type.Mesh))
	{
		AStaticMeshActor* House = Spawn<AStaticMeshActor>(Base + L, Yaw, TEXT("Town/Houses"), [&](AStaticMeshActor* M)
		{
			M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
			M->GetStaticMeshComponent()->SetStaticMesh(Part);
		});
		if (!House)
		{
			continue;
		}
		House->SetActorScale3D(FVector(Type.Scale));
		if (!First)
		{
			First = House;
			// The foundation: a stone plinth under the whole footprint, from below the ground up into the floor.
			static UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (Cube)
			{
				// A gas station stands on a concrete forecourt (bigger than the model, over its own patch of ground).
				const bool bForecourt = Type.Mesh->GetName().Contains(TEXT("gas_station"));
				FVector2D C = Footprint.GetCenter();
				FVector2D Sz = Footprint.GetSize() * (bForecourt ? 1.12f : 0.97f);
				if (bForecourt)
				{
					// The forecourt runs on to the sidewalk (a gas station has no front lawn): grow it on the road side.
					const int32 NA = FMath::Abs(ToRoad.X) > FMath::Abs(ToRoad.Y) ? 0 : 1;
					const float Grow = 80.f; // with the 1.12 margin this reaches the sidewalk edge (yard = 160)
					Sz[NA] += Grow;
					C[NA] += (ToRoad[NA] > 0.f ? 0.5f : -0.5f) * Grow;
				}
				const float Bottom = RoadPoint.Z - 40.f;
				const float Top = RoadPoint.Z + FoundationRaise + (bForecourt ? 22.f : 8.f);
				AStaticMeshActor* Plinth = Spawn<AStaticMeshActor>(Base + FVector(C.X, C.Y, (Bottom + Top) * 0.5f), 0.f, TEXT("Town/Houses/Foundations"), [&](AStaticMeshActor* M)
				{
					M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
					M->GetStaticMeshComponent()->SetStaticMesh(Cube);
					if (UMaterialInterface* PlinthMat = bForecourt ? (Mats.Paving ? Mats.Paving : Mats.Asphalt) : Mats.Rock)
					{
						M->GetStaticMeshComponent()->SetMaterial(0, PlinthMat);
					}
				});
				if (Plinth)
				{
					Plinth->SetActorScale3D(FVector(Sz.X / 100.f, Sz.Y / 100.f, (Top - Bottom) / 100.f));
				}
			}
		}
		else
		{
			House->AttachToActor(First, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	return First != nullptr;
}

void AFBMapGenerator::SpawnDoor(AFBBuilding* Building, bool bLocked, FName KeyId)
{
	if (!Building || Building->FrontDoors <= 0)
	{
		return;
	}

	const float X = Building->Depth * 0.5f - Building->WallThickness * 0.5f;
	for (int32 i = 0; i < Building->FrontDoors; ++i)
	{
		const float Center = -Building->Width * 0.5f + Building->Width * (i + 0.5f) / Building->FrontDoors;
		// FBDoor: hinge at its origin, 100 cm panel towards +Y.
		const FVector Where = InBuilding(Building, FVector(X, Center - 50.f, 0.f));
		Spawn<AFBDoor>(Where, Building->GetActorRotation().Yaw, TEXT("Doors"), [&](AFBDoor* D)
		{
			if (bLocked)
			{
				FBGen::SetProp(D, TEXT("bLocked"), TEXT("True"));
				if (!KeyId.IsNone())
				{
					FBGen::SetProp(D, TEXT("RequiredKeyId"), KeyId.ToString());
				}
			}
		});
	}
}

void AFBMapGenerator::BuildTown(FRandomStream& Random)
{
	const FBDefaults::FStreet& Main = FBDefaults::Streets[0];
	const float FrontLine = Main.Edge() + 150.f; // story buildings: 1.5 m behind the sidewalk

	// Where a story building's grey box goes so that the real model's front sits on the front line.
	// Side +1 = north of Main Street (front faces -Y), -1 = south.
	auto StorySpot = [&](const FBDefaults::FStoryModel& Def, float X, float Side, float GreyDepth, FBox2D& OutFoot) -> FVector
	{
		const float Yaw = Side > 0.f ? -90.f : 90.f;
		const FBox M = FBStoryBox(Def, Yaw);
		if (M.IsValid)
		{
			const FVector Size = M.GetSize();
			const float CenterY = Side * (FrontLine + Size.Y * 0.5f);
			OutFoot = FBox2D(FVector2D(X - Size.X * 0.5f, CenterY - Size.Y * 0.5f), FVector2D(X + Size.X * 0.5f, CenterY + Size.Y * 0.5f));
			return FVector(X, CenterY - Side * Def.Back, 0.f); // the model sits Back behind the grey box's centre
		}
		const float CenterY = Side * (FrontLine + GreyDepth * 0.5f);
		OutFoot = FBox2D(FVector2D(X - 800.f, CenterY - GreyDepth * 0.5f), FVector2D(X + 800.f, CenterY + GreyDepth * 0.5f));
		return FVector(X, CenterY, 0.f);
	};

	// ---- Story buildings on Main Street (north side faces -Y, south side faces +Y).
	FBox2D Foot;
	const FVector MotelAt = StorySpot(FBDefaults::MotelModel, -7600.f, 1.f, 1000.f, Foot);
	FBDefaults::Occupied.Add(Foot);
	// The motel gets two more wings (rooms you can't enter) and a parking lot: keep their ground free of houses.
	FBDefaults::MotelWings.Reset();
	FBDefaults::MotelParking = FBox2D(ForceInit);
	{
		const FBox MB = FBStoryBox(FBDefaults::MotelModel, -90.f);
		if (MB.IsValid)
		{
			const FVector2D Sz(MB.GetSize().X, MB.GetSize().Y);
			auto Free = [](const FBox2D& Area)
			{
				for (const FBox2D& O : FBDefaults::Occupied)
				{
					if (O.Intersect(Area))
					{
						return false;
					}
				}
				return true;
			};
			const FVector2D Mc = Foot.GetCenter();
			const float Away = FMath::Sign(Mc.Y); // away from Main Street
			// Wings: beside the motel if there's room, else rows behind it (a classic motel court).
			const FVector2D Spots[] = {
				Mc + FVector2D(-(Sz.X + 120.f), 0.f), Mc + FVector2D(Sz.X + 120.f, 0.f),
				Mc + FVector2D(0.f, Away * (Sz.Y + 900.f)), Mc + FVector2D(-(Sz.X + 120.f), Away * (Sz.Y + 900.f)), Mc + FVector2D(Sz.X + 120.f, Away * (Sz.Y + 900.f)) };
			for (const FVector2D& C : Spots)
			{
				const FBox2D Area(C - Sz * 0.5f, C + Sz * 0.5f);
				if (FBDefaults::MotelWings.Num() < 2 && Free(Area))
				{
					FBDefaults::Occupied.Add(Area);
					// Z = 1: a row across the court, turned around so its doors face the motel's (the lot between them).
					FBDefaults::MotelWings.Add(FVector(C.X, C.Y, FMath::Abs(C.Y - Mc.Y) > 1.f ? 1.f : 0.f));
				}
			}
			// Parking: beside the motel (as deep as it), else in the court between the rows.
			const float PW = FMath::Min(1600.f, Sz.X);
			const FBox2D ParkSpots[] = {
				FBox2D(FVector2D(Mc.X + Sz.X * 0.5f + 150.f, Mc.Y - Sz.Y * 0.5f), FVector2D(Mc.X + Sz.X * 0.5f + 150.f + PW, Mc.Y + Sz.Y * 0.5f)),
				FBox2D(FVector2D(Mc.X - Sz.X * 0.5f - 150.f - PW, Mc.Y - Sz.Y * 0.5f), FVector2D(Mc.X - Sz.X * 0.5f - 150.f, Mc.Y + Sz.Y * 0.5f)),
				FBox2D(FVector2D(Mc.X - Sz.X * 0.5f, FMath::Min(Mc.Y + Away * (Sz.Y * 0.5f + 100.f), Mc.Y + Away * (Sz.Y * 0.5f + 800.f))),
				       FVector2D(Mc.X + Sz.X * 0.5f, FMath::Max(Mc.Y + Away * (Sz.Y * 0.5f + 100.f), Mc.Y + Away * (Sz.Y * 0.5f + 800.f)))) };
			for (const FBox2D& Area : ParkSpots)
			{
				if (Free(Area))
				{
					FBDefaults::Occupied.Add(Area);
					FBDefaults::MotelParking = Area;
					break;
				}
			}
			UE_LOG(LogFBMapGen, Log, TEXT("Motel footprint %.0f x %.0f at (%.0f, %.0f)."), Sz.X, Sz.Y, Mc.X, Mc.Y);
			UE_LOG(LogFBMapGen, Log, TEXT("Motel: %d extra wings, parking %s."), FBDefaults::MotelWings.Num(), FBDefaults::MotelParking.bIsValid ? TEXT("yes") : TEXT("no room"));
		}
	}
	Motel = Spawn<AFBBuilding_Motel>(Base + MotelAt, -90.f, TEXT("Town/Story"), [&](AFBBuilding_Motel* B)
	{
		B->WallMaterial = Mats.Bricks;
		B->FloorMaterial = Mats.Paving;
	});
	const FVector DinerAt = StorySpot(FBDefaults::DinerModel, -1750.f, 1.f, 1000.f, Foot);
	FBDefaults::Occupied.Add(Foot);
	Diner = Spawn<AFBBuilding_Diner>(Base + DinerAt, -90.f, TEXT("Town/Story"), [&](AFBBuilding_Diner* B)
	{
		B->WallMaterial = Mats.WoodSiding;
		B->FloorMaterial = Mats.WoodFloor;
	});
	const FVector SheriffAt = StorySpot(FBDefaults::SheriffModel, 600.f, 1.f, 1000.f, Foot);
	FBDefaults::Occupied.Add(Foot);
	Sheriff = Spawn<AFBBuilding_Sheriff>(Base + SheriffAt, -90.f, TEXT("Town/Story"), [&](AFBBuilding_Sheriff* B)
	{
		B->WallMaterial = Mats.Bricks;
		B->FloorMaterial = Mats.WoodFloor;
	});
	const FVector LibraryAt = StorySpot(FBDefaults::LibraryModel, -1750.f, -1.f, 1100.f, Foot);
	FBDefaults::Occupied.Add(Foot);
	Library = Spawn<AFBBuilding_Library>(Base + LibraryAt, 90.f, TEXT("Town/Story"), [&](AFBBuilding_Library* B)
	{
		B->WallMaterial = Mats.Bricks;
		B->FloorMaterial = Mats.WoodFloor;
	});
	const FVector ChurchAt = StorySpot(FBDefaults::ChurchModel, 5100.f, 1.f, 1200.f, Foot);
	FBDefaults::Occupied.Add(Foot);
	Church = Spawn<AFBBuilding_Church>(Base + ChurchAt, -90.f, TEXT("Town/Story"), [&](AFBBuilding_Church* B)
	{
		B->WallMaterial = Mats.WoodSiding;
		B->RoofMaterial = Mats.Roofing;
		B->FloorMaterial = Mats.WoodFloor;
	});

	// Story buildings that have a real model: the model goes in, the grey box becomes an invisible anchor.
	auto SpawnStoryPart = [this](const FVector& At, float Yaw, UStaticMesh* Part) -> AStaticMeshActor*
	{
		return Spawn<AStaticMeshActor>(At, Yaw, TEXT("Town/Story"), [&](AStaticMeshActor* M)
		{
			M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
			M->GetStaticMeshComponent()->SetStaticMesh(Part);
		});
	};
	auto Replace = [&](AFBBuilding* B, const FBDefaults::FStoryModel& Def) { return FBReplaceWithModel(B, Def, SpawnStoryPart); };
	const bool bMotelReal = Replace(Motel.Get(), FBDefaults::MotelModel);
	const bool bSheriffReal = Replace(Sheriff.Get(), FBDefaults::SheriffModel);
	const bool bLibraryReal = Replace(Library.Get(), FBDefaults::LibraryModel);
	const bool bChurchReal = Replace(Church.Get(), FBDefaults::ChurchModel);
	Replace(Diner.Get(), FBDefaults::DinerModel);

	if (!bMotelReal)
	{
		SpawnDoor(Motel.Get(), false);
	}
	else
	{
		// ---- Room 7: the brothers' motel room. The real room model (1:1, bed, bathroom, desk) fills the motel cabin
		// behind its right-hand door; that door becomes a real one and the room gets its ceiling light.
		FBDefaults::bComputerPlaced = false;
		FBDefaults::GuestBookAt = FVector::ZeroVector;
		TArray<UStaticMesh*> RoomParts = FBRealModels::FindAll<UStaticMesh>(TEXT("Buildings"), TEXT("motel_room"));
		RoomParts.RemoveAll([](const UStaticMesh* P) { return !P || P->GetBoundingBox().GetSize().IsNearlyZero(); }); // empty nodes
		AStaticMeshActor* MotelCabin = nullptr;
		TArray<AStaticMeshActor*> DoorPanels;
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			UStaticMesh* M = It->ActorHasTag(FBGen::GeneratedTag) ? It->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
			if (!M || !M->GetPathName().Contains(TEXT("motel_modular")))
			{
				continue;
			}
			const FString N = M->GetName();
			if (N.StartsWith(TEXT("motelCabin_bareMotel")))
			{
				MotelCabin = *It;
			}
			else if (N.Contains(TEXT("simpleWoodenDoor")) && !N.StartsWith(TEXT("Frame")))
			{
				DoorPanels.Add(*It);
			}
		}
		if (MotelCabin && DoorPanels.Num() > 0 && RoomParts.Num() > 0)
		{
			const FBox Cb = MotelCabin->GetComponentsBoundingBox();
			// The door nearest the middle of the cabin (the room's door is near its middle too).
			DoorPanels.Sort([&](const AStaticMeshActor& A, const AStaticMeshActor& B)
			{
				return FVector::DistSquared2D(A.GetComponentsBoundingBox().GetCenter(), Cb.GetCenter()) < FVector::DistSquared2D(B.GetComponentsBoundingBox().GetCenter(), Cb.GetCenter());
			});
			AStaticMeshActor* Panel = DoorPanels[0];
			const FBox Db = Panel->GetComponentsBoundingBox();
			const FVector Dc = Db.GetCenter();

			// Which wall the door is in: its outward normal (world, along X or Y).
			const float Dist[4] = { float(Cb.Max.X - Dc.X), float(Dc.X - Cb.Min.X), float(Cb.Max.Y - Dc.Y), float(Dc.Y - Cb.Min.Y) };
			int32 Side = 0;
			for (int32 I = 1; I < 4; ++I)
			{
				Side = Dist[I] < Dist[Side] ? I : Side;
			}
			const FVector Nw = Side == 0 ? FVector(1, 0, 0) : Side == 1 ? FVector(-1, 0, 0) : Side == 2 ? FVector(0, 1, 0) : FVector(0, -1, 0);
			const bool bNormalAlongX = Side < 2;

			// The room model: its door (threshold part, "Object_6") is in one of its long walls.
			FBox Rb(ForceInit);
			FVector Tc = FVector::ZeroVector;
			bool bThreshold = false;
			for (const UStaticMesh* P : RoomParts)
			{
				Rb += P->GetBoundingBox();
				if (P->GetName() == TEXT("Object_6"))
				{
					Tc = P->GetBoundingBox().GetCenter();
					bThreshold = true;
				}
			}
			const bool bShortAlongX = Rb.GetSize().X < Rb.GetSize().Y;
			if (!bThreshold)
			{
				Tc = Rb.GetCenter() - (bShortAlongX ? FVector(Rb.GetExtent().X, 0, 0) : FVector(0, Rb.GetExtent().Y, 0));
			}
			const FVector Nr = bShortAlongX ? FVector(FMath::Sign(Tc.X - Rb.GetCenter().X), 0, 0) : FVector(0, FMath::Sign(Tc.Y - Rb.GetCenter().Y), 0);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Nw.Y, Nw.X) - FMath::Atan2(Nr.Y, Nr.X));
			const FRotator Rot(0.f, Yaw, 0.f);

			// The room's own doorway has to sit right behind the motel door, or its wall blocks the way in.
			// The room is nearly as long as the cabin, so try it mirrored and/or a little narrower along the wall
			// until the two doorways line up (within 12 cm) and the room still fits inside the cabin.
			const float Margin = 2.f; // the room is only a few cm smaller than the cabin
			FVector RoomScale(1.f);
			FVector P = FVector::ZeroVector;
			FBox Rr(ForceInit);
			float BestMiss = TNumericLimits<float>::Max();
			for (const float Mirror : { 1.f, -1.f })
			{
				for (const float Narrow : { 1.f, 0.95f, 0.9f, 0.85f, 0.8f })
				{
					FVector Sc(1.f);
					(bShortAlongX ? Sc.Y : Sc.X) = Mirror * Narrow; // along the door wall, in the room's own space
					const FTransform RT(Rot, FVector::ZeroVector, Sc);
					const FBox R = Rb.TransformBy(RT);
					const FVector T = RT.TransformPosition(Tc);
					FVector Q = FVector::ZeroVector;
					if (bNormalAlongX)
					{
						Q.Y = FMath::Clamp(Dc.Y - T.Y, Cb.Min.Y + Margin - R.Min.Y, Cb.Max.Y - Margin - R.Max.Y);
						Q.X = Nw.X > 0.f ? (Cb.Max.X - Margin - R.Max.X) : (Cb.Min.X + Margin - R.Min.X);
					}
					else
					{
						Q.X = FMath::Clamp(Dc.X - T.X, Cb.Min.X + Margin - R.Min.X, Cb.Max.X - Margin - R.Max.X);
						Q.Y = Nw.Y > 0.f ? (Cb.Max.Y - Margin - R.Max.Y) : (Cb.Min.Y + Margin - R.Min.Y);
					}
					const float Miss = bNormalAlongX ? FMath::Abs(Q.Y + T.Y - Dc.Y) : FMath::Abs(Q.X + T.X - Dc.X);
					// Prefer the least change: a perfect fit unmirrored at full size wins, then the first good one.
					if (Miss < BestMiss - 1.f)
					{
						BestMiss = Miss;
						RoomScale = Sc;
						P = Q;
						Rr = R;
					}
					if (Miss <= 12.f)
					{
						break;
					}
				}
				if (BestMiss <= 12.f)
				{
					break;
				}
			}
			P.Z = MotelCabin->GetActorLocation().Z + 1.f; // both models: floor at their origin
			const bool bFits = Rr.GetSize().X < Cb.GetSize().X - 2.f * Margin + 1.f && Rr.GetSize().Y < Cb.GetSize().Y - 2.f * Margin + 1.f;
			UE_LOG(LogFBMapGen, Log, TEXT("Room 7: room %.0f x %.0f in cabin %.0f x %.0f (%s), yaw %.0f, scale %s, doorways %.0f cm apart."),
				Rr.GetSize().X, Rr.GetSize().Y, Cb.GetSize().X, Cb.GetSize().Y, bFits ? TEXT("fits") : TEXT("TOO BIG"), Yaw, *RoomScale.ToString(), BestMiss);

			AStaticMeshActor* FirstPart = nullptr;
			const FTransform RoomT(FRotator(0.f, Yaw, 0.f), P, RoomScale);
			int32 SkippedShells = 0;
			for (UStaticMesh* Part : RoomParts)
			{
				const FBox PB = Part->GetBoundingBox();
				const FVector PS = PB.GetSize();
				if (PS.X > Rb.GetSize().X * 0.6f && PS.Y > Rb.GetSize().Y * 0.6f && PS.Z > 150.f)
				{
					++SkippedShells; // a room-size wall shell: the cabin's own walls are the room's walls
					continue;
				}
				const FBox WPB = PB.TransformBy(RoomT);
				if (FVector::Dist2D(WPB.GetCenter(), Dc) < 160.f && WPB.GetSize().Z < 140.f)
				{
					++SkippedShells; // a low cabinet standing right in the doorway
					continue;
				}
				if (WPB.Min.X < Cb.Min.X + 10.f || WPB.Max.X > Cb.Max.X - 10.f || WPB.Min.Y < Cb.Min.Y + 10.f || WPB.Max.Y > Cb.Max.Y - 10.f)
				{
					++SkippedShells; // would poke out through the cabin's wall
					continue;
				}
				FBDefaults::MakeWalkable(Part);
				AStaticMeshActor* A = Spawn<AStaticMeshActor>(P, Yaw, TEXT("Town/Story"), [&](AStaticMeshActor* M)
				{
					M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
					M->GetStaticMeshComponent()->SetStaticMesh(Part);
				});
				if (A)
				{
					A->SetActorScale3D(RoomScale);
				}
				if (A && !FirstPart)
				{
					FirstPart = A;
#if WITH_EDITOR
					A->SetActorLabel(TEXT("HF_MotelRoom7"));
#endif
				}
				else if (A)
				{
					A->AttachToActor(FirstPart, FAttachmentTransformRules::KeepWorldTransform);
				}
			}

			// The bare cabin has a dirt floor: a wooden one for the room.
			if (UStaticMesh* FloorCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
			{
				if (AStaticMeshActor* Fl = Spawn<AStaticMeshActor>(FVector(Cb.GetCenter().X, Cb.GetCenter().Y, P.Z - 9.f), 0.f, TEXT("Town/Story"), [&](AStaticMeshActor* M)
				{
					M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
					M->GetStaticMeshComponent()->SetStaticMesh(FloorCube);
					M->GetStaticMeshComponent()->SetMaterial(0, Mats.Planks);
				}))
				{
					Fl->SetActorScale3D(FVector((Cb.GetSize().X - 30.f) / 100.f, (Cb.GetSize().Y - 30.f) / 100.f, 0.2f));
				}
			}
			UE_LOG(LogFBMapGen, Log, TEXT("Room 7: %d wall-shell / doorway parts left out (the cabin's walls are the room's)."), SkippedShells);

			// The real door: the model's door panel goes, an FBDoor (opens, can be locked) takes its place.
			// The model has the panel twice (an inner and an outer face): both go, or the second one blocks the doorway.
			for (AStaticMeshActor* Twin : DoorPanels)
			{
				if (Twin == Panel || FVector::Dist2D(Twin->GetComponentsBoundingBox().GetCenter(), Dc) < 140.f)
				{
					Twin->SetActorHiddenInGame(true);
					Twin->SetActorEnableCollision(false);
					Twin->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
#if WITH_EDITOR
					Twin->SetIsTemporarilyHiddenInEditor(true);
#endif
				}
			}
			const float DoorYaw = FMath::RadiansToDegrees(FMath::Atan2(Nw.Y, Nw.X));
			const FVector PanelDir = FRotator(0.f, DoorYaw, 0.f).RotateVector(FVector(0.f, 1.f, 0.f)); // FBDoor: panel towards its +Y
			AFBDoor* RoomDoor = Spawn<AFBDoor>(FVector(Dc.X, Dc.Y, P.Z) - PanelDir * 50.f, DoorYaw, TEXT("Doors"), [](AFBDoor* D)
			{
				// Room 7 opens with the key card from the motel office (the clerk hands it over).
				FBGen::SetProp(D, TEXT("bLocked"), TEXT("True"));
				FBGen::SetProp(D, TEXT("RequiredKeyId"), TEXT("Room7Card"));
				FBGen::SetProp(D, TEXT("bConsumeKey"), TEXT("False"));
				FBGen::SetProp(D, TEXT("LockedMessage"), FBGen::Text(TEXT("Card lock. The office has the key cards.")));
			});
			// It looks like the motel's own wooden door (the model's panel, swinging on the FBDoor's hinge), not a grey box.
			UStaticMeshComponent* PanelComp = Panel->GetStaticMeshComponent();
			if (RoomDoor && PanelComp && PanelComp->GetStaticMesh())
			{
				USceneComponent* HingeComp = nullptr;
				UStaticMeshComponent* Cube = nullptr;
				TInlineComponentArray<USceneComponent*> Comps(RoomDoor);
				for (USceneComponent* C : Comps)
				{
					if (C->GetFName() == TEXT("Hinge")) HingeComp = C;
					else if (C->GetFName() == TEXT("DoorMesh")) Cube = Cast<UStaticMeshComponent>(C);
				}
				if (HingeComp && Cube)
				{
					UStaticMeshComponent* Look = NewObject<UStaticMeshComponent>(RoomDoor, TEXT("DoorLook"), RF_Transactional);
					Look->CreationMethod = EComponentCreationMethod::Instance;
					Look->SetStaticMesh(PanelComp->GetStaticMesh());
					for (int32 M = 0; M < PanelComp->GetNumMaterials(); ++M)
					{
						Look->SetMaterial(M, PanelComp->GetMaterial(M));
					}
					Look->SetupAttachment(HingeComp);
					Look->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					RoomDoor->AddInstanceComponent(Look);
					Look->RegisterComponent();
					Look->SetWorldTransform(PanelComp->GetComponentTransform());
					// The model's panel stops short of the floor: stretch it down to the floor, keeping its top edge.
					{
						const FBox Before = Look->CalcBounds(Look->GetComponentTransform()).GetBox();
						const float H = FMath::Max(1.f, Before.Max.Z - Before.Min.Z);
						const float Want = (Before.Max.Z - (P.Z + 1.f));
						UE_LOG(LogFBMapGen, Log, TEXT("Room 7 door panel: bottom %.0f top %.0f floor %.0f (stretch %.2f)."), Before.Min.Z, Before.Max.Z, P.Z, Want / H);
						if (Want > H && Want < H * 1.6f)
						{
							FVector Sc = Look->GetComponentScale();
							Sc.Z *= Want / H;
							Look->SetWorldScale3D(Sc);
							const FBox After = Look->CalcBounds(Look->GetComponentTransform()).GetBox();
							Look->AddWorldOffset(FVector(0.f, 0.f, Before.Max.Z - After.Max.Z));
						}
					}
					Cube->SetVisibility(false);
					Cube->SetHiddenInGame(true);
				}
			}

			// Ceiling light (warm, motel-cheap) and a bedside glow, so the room isn't pitch dark.
			const FVector RoomMid = P + Rr.GetCenter();
			for (const FVector& L : { FVector(RoomMid.X, RoomMid.Y, P.Z + 225.f), FVector(RoomMid.X, RoomMid.Y, P.Z + 225.f) + Nw * -150.f })
			{
				Spawn<APointLight>(L, 0.f, TEXT("Town/Story"), [](APointLight* Light)
				{
					UPointLightComponent* C = Light->PointLightComponent;
					C->SetMobility(EComponentMobility::Movable);
					C->SetIntensityUnits(ELightUnits::Candelas);
					C->SetIntensity(12.f);
					C->SetAttenuationRadius(800.f);
					C->SetSourceRadius(8.f);
					C->SetLightColor(FLinearColor(1.f, 0.78f, 0.55f));
					C->SetCastShadows(true);
				});
			}

			// The brothers research the case on the desk in their room (desk, old computer, lamp, chair).
			{
				const FBox RoomWorld(P + Rr.Min, P + Rr.Max);
				float DeskYaw = 0.f;
				const FVector DeskAt = FreeSpotInBox(GetWorld(), RoomWorld.ExpandBy(FVector(-30.f, -30.f, 0.f)), P.Z, Yaw, FVector(55.f, 118.f, 80.f), &DeskYaw, nullptr);
				if (!DeskAt.IsZero())
				{
					Spawn<AFBResearchComputer>(DeskAt, DeskYaw, TEXT("Gameplay"), [](AFBResearchComputer* C)
					{
						C->TerminalName = NSLOCTEXT("FB", "Room7Terminal", "ROOM 7 - TOWN ARCHIVE (DIAL-UP)");
					});
					FBDefaults::bComputerPlaced = true;
					// The guest book the brothers borrowed from the office, open on the desk beside the computer.
					const FRotator DeskRot(0.f, DeskYaw, 0.f);
					FBDefaults::GuestBookAt = DeskAt + DeskRot.RotateVector(FVector(-4.f, 40.f, 78.f + 10.f));
					FBDefaults::GuestBookYaw = DeskYaw + 18.f;
				}
				UE_LOG(LogFBMapGen, Log, TEXT("Room 7 research desk: %s"), DeskAt.IsZero() ? TEXT("no free spot") : *DeskAt.ToString());
			}
		}
		else
		{
			UE_LOG(LogFBMapGen, Warning, TEXT("Room 7 not built (cabin %d, doors %d, room parts %d)."), MotelCabin != nullptr, DoorPanels.Num(), RoomParts.Num());
		}
	}
	// The diner model has its own glass entrance; a grey FBDoor would stand inside it as a checker-board slab.
	if (Diner.IsValid() && !Diner->bShellOnly)
	{
		SpawnDoor(Diner.Get());
	}
	if (!bSheriffReal)
	{
		SpawnDoor(Sheriff.Get());
	}
	if (!bLibraryReal)
	{
		SpawnDoor(Library.Get());
	}
	if (!bChurchReal)
	{
		SpawnDoor(Church.Get());
	}

	// The cemetery across North Avenue from the church.
	{
		const FBDefaults::FStreet& North = FBDefaults::Streets[1];
		const FVector At(5100.f, North.Pos + North.Edge() + 300.f + 1500.f, 0.f);
		CemeteryActor = Spawn<AFBCemetery>(Base + At, -90.f, TEXT("Town/Story"), [&](AFBCemetery* C)
		{
			C->Rows = 6;
			C->GravesPerRow = 9;
			C->GroundMaterial = Mats.Grass;
		});
		FBDefaults::Occupied.Add(FBox2D(FVector2D(At.X - 1700.f, At.Y - 1600.f), FVector2D(At.X + 1700.f, At.Y + 1600.f)));
	}

	// ---- Houses: walk along every town street, both sides, and fill the lots that are free.
	FBDefaults::Lots.Reset();
	float TotalWeight = 0.f;
	for (const FFBTownBuildingType& T : HouseModels)
	{
		if (T.Mesh && T.Weight > 0.f)
		{
			TotalWeight += T.Weight;
		}
	}
	auto PickModel = [&]() -> const FFBTownBuildingType*
	{
		float Roll = Random.FRandRange(0.f, TotalWeight);
		for (const FFBTownBuildingType& T : HouseModels)
		{
			if (T.Mesh && T.Weight > 0.f && (Roll -= T.Weight) <= 0.f)
			{
				return &T;
			}
		}
		return nullptr;
	};
	const TArray<FBDefaults::FCrossing> Cross = FBDefaults::Crossings();

	// Keep the outskirts' story spots free of town houses: the burned house (placed later), its yard.
	FBDefaults::Occupied.Add(FBox2D(FVector2D(11500.f - 1500.f, -2100.f - 1400.f), FVector2D(11500.f + 1500.f, -2100.f + 1400.f)));
	// ...and the lake with its shore (same centre/size as the terrain's basin in BuildGround).
	if (bLake)
	{
		FBDefaults::Occupied.Add(FBox2D(FVector2D(13500.f - 4800.f * 1.2f, 9200.f - 3200.f * 1.2f), FVector2D(13500.f + 4800.f * 1.2f, 9200.f + 3200.f * 1.2f)));
	}

	TArray<TObjectPtr<USkeletalMesh>> GardenBushes;
	for (const TCHAR* Letter : { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Megaplant_Library/Shrub_Beach_Rose/Shrub_Beach_Rose_01/SK_Beach_Rose_01_%s"), Letter);
		if (USkeletalMesh* Bush = LoadObject<USkeletalMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			GardenBushes.Add(Bush);
		}
	}

	int32 Houses = 0;
	for (int32 StreetIndex = 0; StreetIndex < FBDefaults::Streets.Num(); ++StreetIndex)
	{
		const FBDefaults::FStreet& S = FBDefaults::Streets[StreetIndex];
		if (!S.bTown)
		{
			continue;
		}
		for (const float Side : { 1.f, -1.f })
		{
			// Direction from a house on this side to the street.
			const FVector ToRoad = S.bAlongX ? FVector(0.f, -Side, 0.f) : FVector(-Side, 0.f, 0.f);
			float Cursor = S.From + 300.f;
			while (Cursor < S.To - 600.f)
			{
				// Keep clear of the intersections (corner lots face the other street).
				bool bNearCrossing = false;
				for (const FBDefaults::FCrossing& C : Cross)
				{
					const float Along = S.bAlongX ? C.At.X : C.At.Y;
					const float OnLine = S.bAlongX ? C.At.Y : C.At.X;
					const FBDefaults::FStreet& Other = FBDefaults::Streets[S.bAlongX ? C.StreetY : C.StreetX];
					if (FMath::Abs(OnLine - S.Pos) < 1.f && Cursor > Along - Other.Edge() - 300.f && Cursor < Along + Other.Edge() + 300.f)
					{
						Cursor = Along + Other.Edge() + 300.f;
						bNearCrossing = true;
						break;
					}
				}
				if (bNearCrossing || TotalWeight <= 0.f)
				{
					if (TotalWeight <= 0.f)
					{
						break;
					}
					continue;
				}

				bool bPlaced = false;
				for (int32 Try = 0; Try < 4 && !bPlaced; ++Try)
				{
					const FFBTownBuildingType* T = PickModel();
					if (!T)
					{
						break;
					}
					// Width of this model along the street.
					const float Yaw = FBGen::YawFacing(ToRoad) + T->FrontYaw;
					const FBox MB = FBDefaults::ModelBox(T->Mesh).TransformBy(FTransform(FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, FVector(T->Scale)));
					const float Width = S.bAlongX ? MB.GetSize().X : MB.GetSize().Y;
					const float Along = Cursor + Width * 0.5f;
					if (Along + Width * 0.5f > S.To - 300.f)
					{
						break;
					}
					const FVector2D P = S.At(Along, 0.f);
					const FVector RoadPoint(P.X, P.Y, 0.f);
					const float Front = S.Edge() + (T->Mesh && T->Mesh->GetName().Contains(TEXT("gas_station")) ? 160.f : Random.FRandRange(150.f, 450.f)); // front yard (none at the gas station)
					FBox2D HouseFoot;
					SpawnModelHouse(*T, RoadPoint, ToRoad, Front, true, &HouseFoot);
					if (!FBDefaults::IsFree(HouseFoot, 250.f))
					{
						continue;
					}
					if (Random.FRand() < 0.03f)
					{
						// An empty lot: overgrown, a gap between the houses (real towns have them).
						Cursor += Width + 600.f;
						bPlaced = true;
						break;
					}
					if (SpawnModelHouse(*T, RoadPoint, ToRoad, Front, false, &HouseFoot))
					{
						const bool bGasStation = T->Mesh && T->Mesh->GetName().Contains(TEXT("gas_station"));
						if (bGasStation)
						{
							// A concrete apron from the forecourt to the sidewalk, so cars can drive in (no lawn, no fence).
							static UStaticMesh* ApronCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
							const int32 NA = S.bAlongX ? 1 : 0;
							const int32 AA = 1 - NA;
							const float Sign = ToRoad[NA] > 0.f ? 1.f : -1.f;
							const float Near = Sign > 0.f ? HouseFoot.Max[NA] : HouseFoot.Min[NA];
							const float Walk = RoadPoint[NA] - Sign * S.Edge();
							const float Gap = (Walk - Near) * Sign;
							if (ApronCube && Gap > 10.f)
							{
								FVector Mid = RoadPoint;
								Mid[NA] = (Near + Walk) * 0.5f;
								const float Len = (AA == 0 ? HouseFoot.GetSize().X : HouseFoot.GetSize().Y) * 0.95f;
								const float Z0 = RoadPoint.Z - 40.f;
								const float Z1 = RoadPoint.Z + 35.f + 22.f; // flush with the forecourt (SpawnModelHouse: FoundationRaise 35)
								Mid.Z = (Z0 + Z1) * 0.5f;
								AStaticMeshActor* Apron = Spawn<AStaticMeshActor>(Base + Mid, 0.f, TEXT("Town/Houses/Foundations"), [&](AStaticMeshActor* M)
								{
									M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
									M->GetStaticMeshComponent()->SetStaticMesh(ApronCube);
									if (UMaterialInterface* AM = Mats.Paving ? Mats.Paving : Mats.Asphalt)
									{
										M->GetStaticMeshComponent()->SetMaterial(0, AM);
									}
								});
								if (Apron)
								{
									FVector Sc((AA == 0 ? Len : Gap + 20.f) / 100.f, (AA == 1 ? Len : Gap + 20.f) / 100.f, (Z1 - Z0) / 100.f);
									Apron->SetActorScale3D(Sc);
								}
							}
						}
						// Most houses get a front garden: a wooden picket fence with a gate, bushes along it.
						if (!bGasStation && Random.FRand() < 0.7f)
						{
							const FVector AlongDir = S.bAlongX ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 1.f, 0.f);
							const float FenceLen = Width + 240.f;
							const FVector FenceMid = RoadPoint - ToRoad * (S.Edge() + 90.f);
							Spawn<AFBFence>(Base + FenceMid - AlongDir * (FenceLen * 0.5f), S.bAlongX ? 0.f : 90.f, TEXT("Town/Gardens"), [&](AFBFence* F)
							{
								F->Length = FenceLen;
								F->Height = 95.f;
								F->PostSpacing = 180.f;
								F->GatePosition = 0.5f;
								F->GateWidth = 160.f;
								F->Material = Mats.Planks;
							});
							if (GardenBushes.Num() > 0)
							{
								const FVector BushMid = RoadPoint - ToRoad * (S.Edge() + 190.f);
								Spawn<AFBForest>(Base + BushMid, 0.f, TEXT("Town/Gardens"), [&](AFBForest* F)
								{
									F->Size = S.bAlongX ? FVector2D(Width * 0.8f, 90.f) : FVector2D(90.f, Width * 0.8f);
									F->TreeCount = Random.RandRange(3, 6);
									F->Seed = Seed + Houses * 13 + 5;
									F->MinDistance = 150.f;
									F->SkeletalTreeMeshes = GardenBushes;
									F->TreeScaleMin = 0.45f;
									F->TreeScaleMax = 0.8f;
									F->TrunkCollisionDiameter = 1.f;
								});
							}
						}
						FBDefaults::Occupied.Add(HouseFoot);
						FBDefaults::Lots.Add({ RoadPoint, ToRoad, StreetIndex, HouseFoot });
						++Houses;
						Cursor += Width + Random.FRandRange(480.f, 680.f); // side yards: room for a driveway on one side
						bPlaced = true;
					}
				}
				if (!bPlaced)
				{
					Cursor += 500.f;
				}
			}
		}
	}

	// ---- Backyards, the way the small towns of the northern US look: an alley down the middle of every block with the
	// garages and sheds on it, board fences between the yards, a concrete driveway beside each house (often with a car).
	{
		FRandomStream YR(Random.GetInitialSeed() * 13 + 5);
		FBDefaults::DrivewayCars.Reset();
		FBDefaults::BackyardTrees.Reset();
		static UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		TArray<UStaticMesh*> Sheds;
		for (const TCHAR* N : { TEXT("cabin_worn_shed") }) // (shed_interior stands on a floating slab of lawn)
		{
			if (UStaticMesh* M = FBDefaults::LoadBuilding(N))
			{
				Sheds.Add(M);
			}
		}
		UStaticMesh* Pole = FBDefaults::LoadModel(TEXT("Props"), TEXT("prop_utility_pole"));
		auto Slab = [&](const FVector2D& C, const FVector2D& Size, float Z0, float Z1, UMaterialInterface* Mat, const TCHAR* Folder)
		{
			if (!Cube)
			{
				return;
			}
			if (AStaticMeshActor* A = Spawn<AStaticMeshActor>(Base + FVector(C.X, C.Y, (Z0 + Z1) * 0.5f), 0.f, Folder, [&](AStaticMeshActor* M)
			{
				M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
				M->GetStaticMeshComponent()->SetStaticMesh(Cube);
				if (Mat)
				{
					M->GetStaticMeshComponent()->SetMaterial(0, Mat);
				}
			}))
			{
				A->SetActorScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, (Z1 - Z0) / 100.f));
			}
		};
		// A model standing on the ground at Where (Base-relative), its footprint returned (test only = nothing spawned).
		auto Prop = [&](UStaticMesh* Mesh, const FVector2D& Where, float Yaw, float Height, const TCHAR* Folder, bool bTestOnly) -> FBox2D
		{
			if (!Mesh)
			{
				return FBox2D(ForceInit);
			}
			const FBox MB = FBDefaults::ModelBox(Mesh);
			const float Sc = Height / FMath::Max(1.f, MB.GetSize().Z);
			const FBox WB = MB.TransformBy(FTransform(FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, FVector(Sc)));
			const FVector At(Where.X - WB.GetCenter().X, Where.Y - WB.GetCenter().Y, -WB.Min.Z - 4.f);
			const FBox2D Foot(FVector2D(At.X + WB.Min.X, At.Y + WB.Min.Y), FVector2D(At.X + WB.Max.X, At.Y + WB.Max.Y));
			if (bTestOnly)
			{
				return Foot;
			}
			AStaticMeshActor* First = nullptr;
			for (UStaticMesh* Part : FBDefaults::Parts(Mesh))
			{
				FBDefaults::MakeWalkable(Part);
				if (AStaticMeshActor* A = Spawn<AStaticMeshActor>(Base + At, Yaw, Folder, [&](AStaticMeshActor* M)
				{
					M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
					M->GetStaticMeshComponent()->SetStaticMesh(Part);
				}))
				{
					A->SetActorScale3D(FVector(Sc));
					if (!First)
					{
						First = A;
					}
					else
					{
						A->AttachToActor(First, FAttachmentTransformRules::KeepWorldTransform);
					}
				}
			}
			return Foot;
		};
		auto Fence = [&](const FVector2D& From, const FVector2D& To, float Height, float Gate)
		{
			const FVector2D D = To - From;
			const float Len = D.Size();
			if (Len < 150.f)
			{
				return;
			}
			Spawn<AFBFence>(Base + FVector(From.X, From.Y, 0.f), FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), TEXT("Town/Backyards"), [&](AFBFence* F)
			{
				F->Length = Len;
				F->Height = Height;
				F->PostSpacing = 200.f;
				F->GatePosition = 0.5f;
				F->GateWidth = Gate;
				F->Material = Mats.Planks;
			});
		};

		// Alleys: gravel lanes halfway between Main Street and each avenue, block by block (broken where something stands).
		constexpr float AlleyW = 420.f;
		TArray<float> CrossLines;
		for (const float X : FBDefaults::CrossX)
		{
			CrossLines.Add(X);
		}
		CrossLines.Sort();
		const float CrossEdge = FBDefaults::Streets.Num() > 3 ? FBDefaults::Streets[3].Edge() : 850.f;
		const float AlleyLines[] = { FBDefaults::AvenueY * 0.5f, -FBDefaults::AvenueY * 0.5f };
		int32 AlleyBits = 0, Garages = 0, Drives = 0, Fences = 0;
		for (const float AY : AlleyLines)
		{
			for (int32 I = 0; I + 1 < CrossLines.Num(); ++I)
			{
				const float A0 = CrossLines[I] + CrossEdge + 110.f, A1 = CrossLines[I + 1] - CrossEdge - 110.f;
				constexpr float Chunk = 500.f;
				int32 PoleCount = 0;
				for (float X = A0; X + 10.f < A1; X += Chunk)
				{
					const float X1 = FMath::Min(X + Chunk, A1);
					const FBox2D Bit(FVector2D(X, AY - AlleyW * 0.5f), FVector2D(X1, AY + AlleyW * 0.5f));
					if (!FBDefaults::IsFree(Bit, 0.f))
					{
						continue;
					}
					Slab(Bit.GetCenter(), Bit.GetSize(), -25.f, 3.f, Mats.Ground, TEXT("Town/Alleys"));
					FBDefaults::Occupied.Add(Bit);
					++AlleyBits;
					// Power lines run down the alleys, not the streets.
					if (Pole && (PoleCount++ % 6) == 3)
					{
						Prop(Pole, FVector2D(X + Chunk * 0.5f, AY + AlleyW * 0.5f + 60.f), 0.f, 1100.f, TEXT("Town/Alleys"), false);
					}
				}
			}
		}

		for (FBDefaults::FLot& Lot : FBDefaults::Lots)
		{
			if (!Lot.Foot.bIsValid || !FBDefaults::Streets.IsValidIndex(Lot.Street))
			{
				continue;
			}
			const FBDefaults::FStreet& S = FBDefaults::Streets[Lot.Street];
			const int32 NA = S.bAlongX ? 1 : 0; // across the street
			const int32 AA = 1 - NA;            // along the street
			const float B = -Lot.ToRoad[NA];    // from the house towards its backyard
			const FBox2D& F = Lot.Foot;
			const float BackEdge = B > 0.f ? F.Max[NA] : F.Min[NA];
			const float FrontEdge = B > 0.f ? F.Min[NA] : F.Max[NA];
			auto P2 = [&](float Along, float Across) { FVector2D V; V[AA] = Along; V[NA] = Across; return V; };

			// The alley behind this lot, if there is one (only the blocks between Main Street and the avenues have one).
			float AlleyAt = 0.f;
			bool bAlley = false;
			if (S.bAlongX)
			{
				for (const float AY : AlleyLines)
				{
					if ((AY - S.Pos) * B > 0.f && FMath::Abs(AY - S.Pos) < 4000.f)
					{
						AlleyAt = AY;
						bAlley = true;
					}
				}
			}
			const float RearLine = bAlley ? AlleyAt - B * (AlleyW * 0.5f + 40.f) : BackEdge + B * 1600.f;

			// Garage or shed at the back of the lot, facing the alley (or the back fence).
			if (Sheds.Num() > 0 && YR.FRand() < 0.8f)
			{
				UStaticMesh* M = Sheds[YR.RandRange(0, Sheds.Num() - 1)];
				const float Yaw = FBGen::YawFacing(Lot.ToRoad * -1.f) + 0.f;
				const float Height = YR.FRandRange(290.f, 360.f);
				FBox2D Test = Prop(M, P2(F.GetCenter()[AA] + YR.FRandRange(-200.f, 200.f), RearLine), Yaw, Height, TEXT("Town/Backyards"), true);
				const float Depth = Test.GetSize()[NA];
				const FVector2D At = P2(F.GetCenter()[AA] + YR.FRandRange(-200.f, 200.f), RearLine - B * (Depth * 0.5f + 40.f));
				Test = Prop(M, At, Yaw, Height, TEXT("Town/Backyards"), true);
				if (Test.bIsValid && Test.GetSize()[AA] < F.GetSize()[AA] + 400.f && (RearLine - BackEdge) * B > Depth + 250.f && FBDefaults::IsFree(Test, 60.f))
				{
					FBDefaults::Occupied.Add(Prop(M, At, Yaw, Height, TEXT("Town/Backyards"), false));
					++Garages;
				}
			}

			// The driveway: from the street along the other side of the house to its back corner.
			const float DriveA = F.Min[AA] - 270.f; // clear of the front garden fence's end
			const float Street0 = S.Pos - B * 0.f; // (street centre line)
			const float Walk = S.Pos + B * S.Edge();
			const FBox2D Drive(FVector2D::Min(P2(DriveA - 145.f, Walk), P2(DriveA + 145.f, BackEdge + B * 150.f)), FVector2D::Max(P2(DriveA - 145.f, Walk), P2(DriveA + 145.f, BackEdge + B * 150.f)));
			(void)Street0;
			// (tested from just past the street's strip, which is taken; the slab itself runs up to the sidewalk)
			const FBox2D DriveTest(FVector2D::Min(P2(DriveA - 145.f, Walk + B * 130.f), P2(DriveA + 145.f, BackEdge + B * 150.f)), FVector2D::Max(P2(DriveA - 145.f, Walk + B * 130.f), P2(DriveA + 145.f, BackEdge + B * 150.f)));
			if (FBDefaults::IsFree(DriveTest, 15.f))
			{
				Slab(Drive.GetCenter(), Drive.GetSize(), -25.f, 4.f, Mats.Paving ? Mats.Paving : Mats.Asphalt, TEXT("Town/Driveways"));
				FBDefaults::Occupied.Add(Drive);
				++Drives;
				if (YR.FRand() < 0.55f)
				{
					// Nose in, parked beside the house.
					const FVector2D C = P2(DriveA, (FrontEdge + BackEdge) * 0.5f);
					FVector Facing = FVector::ZeroVector;
					Facing[NA] = B;
					FBDefaults::DrivewayCars.Add({ FVector(C.X, C.Y, 5.f), Facing });
				}
			}
			if ((RearLine - BackEdge) * B > 700.f)
			{
				FBDefaults::BackyardTrees.Add(P2(F.GetCenter()[AA] + YR.FRandRange(-250.f, 250.f), BackEdge + B * YR.FRandRange(350.f, 550.f)));
			}
			// Board fence along the back of the lot and down one side, from the back corner of the house.
			const float SideA = F.Max[AA] + 60.f; // close to the wall, leaving the next lot's driveway free
			const float FenceH = YR.FRand() < 0.6f ? 165.f : 115.f; // privacy fence or a picket one
			Fence(P2(F.Min[AA] - 140.f, RearLine), P2(SideA, RearLine), FenceH, bAlley ? 120.f : 0.f);
			const FBox2D SideBox(FVector2D::Min(P2(SideA - 10.f, BackEdge), P2(SideA + 10.f, RearLine)), FVector2D::Max(P2(SideA - 10.f, BackEdge), P2(SideA + 10.f, RearLine)));
			if (FBDefaults::IsFree(SideBox, 5.f))
			{
				Fence(P2(SideA, BackEdge + B * 30.f), P2(SideA, RearLine), FenceH, 0.f);
				FBDefaults::Occupied.Add(SideBox);
				++Fences;
			}

		}
		UE_LOG(LogFBMapGen, Log, TEXT("Backyards: %d alley pieces, %d garages/sheds, %d driveways (%d cars), %d side fences."), AlleyBits, Garages, Drives, FBDefaults::DrivewayCars.Num(), Fences);
	}

	// Farmhouses along the country road to the mill (north side; the burned house and the barn are on the south side).
	for (const float X : { 9800.f, 12000.f, 14400.f })
	{
		if (const FFBTownBuildingType* T = PickModel())
		{
			FBox2D HouseFoot;
			const FVector RoadPoint(X, 0.f, 0.f);
			SpawnModelHouse(*T, RoadPoint, FVector(0.f, -1.f, 0.f), 900.f, true, &HouseFoot);
			if (FBDefaults::IsFree(HouseFoot, 300.f) && SpawnModelHouse(*T, RoadPoint, FVector(0.f, -1.f, 0.f), 900.f, false, &HouseFoot))
			{
				FBDefaults::Occupied.Add(HouseFoot);
				FBDefaults::Lots.Add({ RoadPoint, FVector(0.f, -1.f, 0.f), FBDefaults::Streets.Num() - 2 });
				++Houses;
			}
		}
	}
	UE_LOG(LogFBMapGen, Log, TEXT("Town: %d houses on %d streets."), Houses, FBDefaults::Streets.Num());
}

// ------------------------------------------------------------------ Outskirts: mill, barn, cabin

void AFBMapGenerator::BuildOutskirts()
{
	// The mill at the end of the country road. Locked: the key is at the sheriff's office.
	Mill = Spawn<AFBBuilding_Mill>(Base + FVector(19500.f, -2400.f, 0.f), 90.f, TEXT("Outskirts"), [&](AFBBuilding_Mill* B)
	{
		B->bBackDoor = false; // one way in
		B->WallMaterial = Mats.Planks;
		B->FloorMaterial = Mats.Planks;
		B->RoofMaterial = Mats.Roofing;
	});

	Barn = Spawn<AFBBuilding>(Base + FVector(16200.f, -2300.f, 0.f), 90.f, TEXT("Outskirts"), [&](AFBBuilding* B)
	{
		B->Width = 1000.f;
		B->Depth = 1400.f;
		B->FloorHeight = 600.f;
		B->Roof = EFBRoofType::Pitched;
		B->RoofRise = 350.f;
		B->DoorWidth = 110.f;
		B->WindowSpacing = 500.f;
		B->WallMaterial = Mats.Planks;
		B->FloorMaterial = Mats.Ground;
		B->RoofMaterial = Mats.Roofing;
		B->WallColor = FLinearColor(0.35f, 0.12f, 0.08f);
		B->bInteriorLight = false;
	});

	// Hunter's cabin at the end of the forest trail - the Gaunt's hunting ground.
	Cabin = Spawn<AFBBuilding>(Base + FVector(FBDefaults::CrossX[1] + 1100.f, 15900.f, 0.f), 180.f, TEXT("Outskirts"), [&](AFBBuilding* B)
	{
		B->Width = 800.f;
		B->Depth = 700.f;
		B->FloorHeight = 300.f;
		B->Roof = EFBRoofType::Pitched;
		B->RoofRise = 250.f;
		B->bBackDoor = false;
		B->WindowSpacing = 350.f;
		B->WallMaterial = Mats.Planks;
		B->FloorMaterial = Mats.Planks;
		B->RoofMaterial = Mats.Roofing;
		B->bInteriorLight = false;
	});
	// Real models for the mill, the barn and the cabin (if imported). The mill and cabin get grey-box doors otherwise.
	auto SpawnOutPart = [this](const FVector& At, float Yaw, UStaticMesh* Part) -> AStaticMeshActor*
	{
		return Spawn<AStaticMeshActor>(At, Yaw, TEXT("Outskirts"), [&](AStaticMeshActor* M)
		{
			M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
			M->GetStaticMeshComponent()->SetStaticMesh(Part);
		});
	};
	if (!FBReplaceWithModel(Mill.Get(), FBDefaults::MillModel, SpawnOutPart))
	{
		SpawnDoor(Mill.Get(), true, TEXT("MillKey"));
	}
	FBReplaceWithModel(Barn.Get(), FBDefaults::BarnModel, SpawnOutPart);
	if (!FBReplaceWithModel(Cabin.Get(), FBDefaults::CabinModel, SpawnOutPart))
	{
		SpawnDoor(Cabin.Get());
	}

	// Fences along the country road in front of the mill yard, with gates to the barn and the mill.
	auto Fence = [this](const FVector& Start, float Length, float Gate)
	{
		Spawn<AFBFence>(Base + Start, 0.f, TEXT("Outskirts"), [&](AFBFence* F)
		{
			F->Length = Length;
			F->GatePosition = Gate;
			F->GateWidth = 300.f;
			F->Material = Mats.Planks;
		});
	};
	Fence(FVector(15000.f, -900.f, 0.f), 2800.f, (16200.f - 15000.f) / 2800.f);
	Fence(FVector(17900.f, -900.f, 0.f), 3600.f, (19500.f - 17900.f) / 3600.f);
}

// ------------------------------------------------------------------ Forests

void AFBMapGenerator::SpawnForest(const FVector& Center, const FVector2D& Size, int32 SeedOffset)
{
	// Real woods are dense: at least ~1 tree per 65 m². The woods north of town (1, 2, 6) are the thickest.
	const bool bNorth = SeedOffset == 1 || SeedOffset == 2 || SeedOffset == 6;
	const float Density = FMath::Max(TreesPerKm2, 15000.f) * (bNorth ? 1.6f : 1.f);
	const float AreaKm2 = (Size.X * Size.Y) / 1.0e10f; // cm² -> km²
	const int32 Count = FMath::Clamp(FMath::RoundToInt(AreaKm2 * Density), 50, 2000);

	Spawn<AFBForest>(Base + Center, 0.f, TEXT("Forests"), [&](AFBForest* F)
	{
		F->Size = Size;
		F->TreeCount = Count;
		F->Seed = Seed + SeedOffset;
		F->MinDistance = bNorth ? 200.f : 250.f;
		F->TrunkMaterial = Mats.Planks;
		F->TreeMeshes = TreeModels;
		F->SkeletalTreeMeshes = SkeletalTreeModels;
	});

	// Undergrowth: wild bushes between the trees (you can walk through them).
	TArray<TObjectPtr<USkeletalMesh>> Bushes;
	{
		for (const TCHAR* Letter : { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") })
		{
			const FString Path = FString::Printf(TEXT("/Game/Megaplant_Library/Shrub_Beach_Rose/Shrub_Beach_Rose_01/SK_Beach_Rose_01_%s"), Letter);
			if (USkeletalMesh* Bush = LoadObject<USkeletalMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Bushes.Add(Bush);
			}
		}
	}
	if (Bushes.Num() > 0)
	{
		const int32 BushCount = FMath::Clamp(FMath::RoundToInt(AreaKm2 * 9000.f * (bNorth ? 1.4f : 1.f)), 30, 1500);
		Spawn<AFBForest>(Base + Center, 0.f, TEXT("Forests/Undergrowth"), [&](AFBForest* F)
		{
			F->Size = Size * 1.04f; // spills a little past the tree line
			F->TreeCount = BushCount;
			F->Seed = Seed + SeedOffset * 31 + 7;
			F->MinDistance = 160.f;
			F->SkeletalTreeMeshes = Bushes;
			F->TreeScaleMin = 0.6f;
			F->TreeScaleMax = 1.3f;
			F->TrunkCollisionDiameter = 1.f; // bushes don't block
		});
	}
}

void AFBMapGenerator::BuildForests()
{
	// A ring of woods around the town (the town with its back yards reaches X -14800..11800, Y -11000..11000).
	// The trail corridor north (X -4800..-3600) stays open. Trees climb the hills and stop at the lake shore.
	SpawnForest(FVector(-12650.f, 16000.f, 0.f), FVector2D(15700.f, 10000.f), 1); // north-west
	SpawnForest(FVector(4200.f, 16000.f, 0.f),   FVector2D(15600.f, 10000.f), 2); // north
	SpawnForest(FVector(-17650.f, 5500.f, 0.f),  FVector2D(5700.f, 11000.f), 3);  // west, north of the highway
	SpawnForest(FVector(-17650.f, -5500.f, 0.f), FVector2D(5700.f, 11000.f), 7);  // west, south of the highway
	SpawnForest(FVector(-4000.f, -16250.f, 0.f), FVector2D(33000.f, 9500.f), 4);  // south
	SpawnForest(FVector(19000.f, -13600.f, 0.f), FVector2D(15000.f, 15000.f), 5); // behind the mill
	SpawnForest(FVector(19000.f, 13300.f, 0.f),  FVector2D(15000.f, 15400.f), 6); // north-east, around the lake
}

// ------------------------------------------------------------------ Lamps

void AFBMapGenerator::BuildLamps()
{
	// Street lights on every town street, staggered left / right, on the kerb, leaning over the road.
	const TArray<FBDefaults::FCrossing> Cross = FBDefaults::Crossings();
	int32 Index = 0;
	for (const FBDefaults::FStreet& S : FBDefaults::Streets)
	{
		if (!S.bTown)
		{
			continue;
		}
		const float Spacing = S.Lanes > 1 ? 2400.f : 3000.f;
		for (float Along = S.From + 900.f; Along < S.To - 600.f; Along += Spacing, ++Index)
		{
			bool bAtCrossing = false;
			for (const FBDefaults::FCrossing& C : Cross)
			{
				const float CA = S.bAlongX ? C.At.X : C.At.Y;
				const float CL = S.bAlongX ? C.At.Y : C.At.X;
				if (FMath::Abs(CL - S.Pos) < 1.f && FMath::Abs(CA - Along) < 1100.f)
				{
					bAtCrossing = true;
				}
			}
			if (bAtCrossing)
			{
				continue;
			}
			const float Side = (Index % 2 == 0) ? 1.f : -1.f;
			const FVector2D P = S.At(Along, Side * (S.Half() + 55.f));
			const FVector ToStreet = S.bAlongX ? FVector(0.f, -Side, 0.f) : FVector(-Side, 0.f, 0.f);
			Spawn<AFBStreetLamp>(Base + FVector(P.X, P.Y, 15.f), FBGen::YawFacing(ToStreet), TEXT("Lamps"), [&](AFBStreetLamp* L)
			{
				L->bFaulty = (Index % 7 == 3);
				L->LightHeight = 720.f;      // real US street lights: 7-9 m
				L->ArmReach = 220.f;
				L->Intensity = S.Lanes > 1 ? 2600.f : 2000.f;
				L->LightColor = (Index % 5 == 1) ? FLinearColor(0.9f, 0.92f, 1.f) : FLinearColor(1.f, 0.66f, 0.33f); // a few newer LED heads
			});
		}
	}

	// Sparse, failing lamps on the road out to the mill.
	for (float X = 9000.f; X <= 19000.f; X += 3500.f)
	{
		Spawn<AFBStreetLamp>(Base + FVector(X, -520.f, 0.f), 90.f, TEXT("Lamps"), [&](AFBStreetLamp* L)
		{
			L->bFaulty = true;
			L->Intensity = 900.f;
		});
	}
}

// ------------------------------------------------------------------ Gameplay

void AFBMapGenerator::BuildGameplay()
{
	// Atmosphere (only one per level).
	bool bHasAtmosphere = false;
	for (TActorIterator<AFBAtmosphere> It(GetWorld()); It; ++It)
	{
		bHasAtmosphere = true;
	}
	if (!bHasAtmosphere)
	{
		Spawn<AFBAtmosphere>(Base, 0.f, TEXT("Gameplay"), [](AFBAtmosphere* A)
		{
			A->Mood = EFBMood::FoggyNight; // the look of the night, when it comes
			// The demo starts on a grey, dry morning; a full day takes two hours of play.
			A->TimeOfDay = 10.f;
			A->DayLengthMinutes = 120.f;
		});
	}

	// Weather: rain showers, and the streets get wet (decals over asphalt, sidewalks and the foot of the facades).
	Spawn<AFBWeather>(Base + FVector(0.f, 0.f, 500.f), 0.f, TEXT("Gameplay"), [](AFBWeather* W)
	{
		// Dry at the start (streets dry too); the first shower rolls in after 8-12 minutes.
		W->Rain = 0.f;
		W->Wetness = 0.f;
	});
	{
		UMaterialInterface* WetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FallenBrothers/Weather/M_FB_Wet.M_FB_Wet"));
		int32 WetCount = 0;
		for (const FBDefaults::FStreet& S : FBDefaults::Streets)
		{
			if (S.Surface == 2)
			{
				continue; // forest trails stay forest floor
			}
			const float HalfWidth = S.Edge() + 15.f; // asphalt and sidewalks only (not the grass, not inside the shops)
			constexpr float Seg = 2500.f;
			for (float A = S.From - HalfWidth; A < S.To + HalfWidth; A += Seg)
			{
				const float Len = FMath::Min(Seg, S.To + HalfWidth - A);
				const FVector2D C = S.At(A + Len * 0.5f, 0.f);
				const float Yaw = S.bAlongX ? 0.f : 90.f;
				ADecalActor* D = Spawn<ADecalActor>(Base + FVector(C.X, C.Y, 20.f), Yaw, TEXT("Roads/Wet"), [&](ADecalActor* Dec)
				{
					Dec->Tags.AddUnique(TEXT("FBWet"));
					Dec->SetActorRotation(FRotator(-90.f, Yaw, 0.f)); // project straight down
					if (UDecalComponent* DC = Dec->GetDecal())
					{
						// Local X = down (depth), Y = across, Z = along the street.
						DC->DecalSize = FVector(110.f, HalfWidth, Len * 0.5f + 5.f);
						DC->SortOrder = -10; // under painted markings and dirt decals
						if (WetMat)
						{
							DC->SetDecalMaterial(WetMat);
						}
					}
				});
				WetCount += D ? 1 : 0;
			}
		}
		if (FBDefaults::MotelParking.bIsValid)
		{
			const FBox2D& PL = FBDefaults::MotelParking;
			const FVector2D C = PL.GetCenter();
			if (ADecalActor* D = Spawn<ADecalActor>(Base + FVector(C.X, C.Y, 20.f), 0.f, TEXT("Roads/Wet"), [&](ADecalActor* Dec)
			{
				Dec->Tags.AddUnique(TEXT("FBWet"));
				Dec->SetActorRotation(FRotator(-90.f, 0.f, 0.f));
				if (UDecalComponent* DC = Dec->GetDecal())
				{
					DC->DecalSize = FVector(110.f, PL.GetSize().Y * 0.5f, PL.GetSize().X * 0.5f);
					DC->SortOrder = -10;
					if (WetMat)
					{
						DC->SetDecalMaterial(WetMat);
					}
				}
			}))
			{
				++WetCount;
			}
		}
		UE_LOG(LogFBMapGen, Log, TEXT("Weather: %d wet street decals."), WetCount);
	}

	// Arrival: in front of the motel, looking down Main Street.
	const FBDefaults::FStreet& MainSt = FBDefaults::Streets[0];
	const float MotelX = Motel.IsValid() ? Motel->GetActorLocation().X - Base.X : -7600.f;
	const float Walkway = MainSt.Half() + MainSt.Sidewalk * 0.5f;
	if (!FBDefaults::BrothersCarAt.IsZero())
	{
		// Arrival: the brothers just got out of the car in the motel lot. Chris by the driver's door, Zack by the trunk.
		const FVector F = FBDefaults::BrothersCarFacing;
		const FVector Left(-F.Y, F.X, 0.f);
		const FVector Car = FBDefaults::BrothersCarAt;
		const float HalfLen = FBDefaults::BrothersCarHalfLength;
		// Chris stands in the open lane behind the trunk (the stalls beside the car can hold other cars, so the
		// driver's door side is not a safe spawn spot), looking at the trunk: the first thing to do is check the gear.
		const FVector Chris = FVector(Car.X, Car.Y, Car.Z) - F * (HalfLen + 110.f) - Left * 70.f;
		Spawn<APlayerStart>(FVector(Chris.X, Chris.Y, Base.Z + 100.f), FBGen::YawFacing(F), TEXT("Gameplay"), [](APlayerStart*) {});
		const FVector Back = Car - F * (HalfLen + 70.f);
		Spawn<AFBCompanion>(FVector(Back.X, Back.Y, Base.Z + 100.f) + Left * 140.f, FBGen::YawFacing(F), TEXT("Gameplay"), [](AFBCompanion*) {});
		// The trunk: a look-box just over the rear end of the car.
		Spawn<AFBCarTrunk>(FVector(Car.X, Car.Y, Base.Z) - F * (HalfLen - 25.f) + FVector(0.f, 0.f, 95.f), FBGen::YawFacing(F), TEXT("Gameplay"), [](AFBCarTrunk*) {});
	}
	else
	{
		Spawn<APlayerStart>(Base + FVector(MotelX + 600.f, Walkway, 100.f), 0.f, TEXT("Gameplay"), [](APlayerStart*) {});
		Spawn<AFBCompanion>(Base + FVector(MotelX + 750.f, Walkway + 80.f, 100.f), 0.f, TEXT("Gameplay"), [](AFBCompanion*) {});
	}
	Spawn<AFBMissionManager>(Base + FVector(0.f, 0.f, 300.f), 0.f, TEXT("Gameplay"), [](AFBMissionManager*) {});
	Spawn<AFBCaseFile>(Base + FVector(150.f, 0.f, 300.f), 0.f, TEXT("Gameplay"), [](AFBCaseFile*) {});

	// ---- Motel office: the night clerk behind his counter (asks questions, the fake FBI badge gets the room 7 key card).
	{
		AStaticMeshActor* Office = nullptr;
		AStaticMeshActor* OfficeDoor = nullptr;
		TArray<AStaticMeshActor*> Fronts; // cabins and the office: they get a concrete walk along their door side
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			UStaticMesh* M = It->ActorHasTag(FBGen::GeneratedTag) ? It->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
			if (!M || !M->GetPathName().Contains(TEXT("motel_modular")))
			{
				continue;
			}
			const FString N = M->GetName();
#if WITH_EDITOR
			const bool bWing = It->GetFolderPath().ToString().Contains(TEXT("MotelWings"));
#else
			const bool bWing = false;
#endif
			if (N.StartsWith(TEXT("motelCabin_bareMotel")) || (!bWing && N.StartsWith(TEXT("ReceptionCabin_"))))
			{
				Fronts.Add(*It);
			}
			if (bWing)
			{
				// The extra wing is the same model: its second office and second road sign go (one motel, one office).
				if (N.StartsWith(TEXT("Reception")) || N.StartsWith(TEXT("motelSign")))
				{
					It->SetActorHiddenInGame(true);
					It->SetActorEnableCollision(false);
#if WITH_EDITOR
					It->SetIsTemporarilyHiddenInEditor(true);
#endif
				}
				continue;
			}
			if (N.StartsWith(TEXT("ReceptionCabin_")))
			{
				Office = *It;
			}
			else if (N.StartsWith(TEXT("ReceptionCabinDoor")))
			{
				OfficeDoor = *It;
			}
		}
		if (Office && OfficeDoor)
		{
			const FBox Ob = Office->GetComponentsBoundingBox();
			const FBox Db = OfficeDoor->GetComponentsBoundingBox();
			const FVector Oc = Ob.GetCenter();
			const FVector Dc = Db.GetCenter();
			// Front of the office = the wall with the door in it.
			FVector Nf = (Dc - Oc);
			Nf.Z = 0.f;
			Nf = FMath::Abs(Nf.X / Ob.GetExtent().X) > FMath::Abs(Nf.Y / Ob.GetExtent().Y) ? FVector(FMath::Sign(Nf.X), 0.f, 0.f) : FVector(0.f, FMath::Sign(Nf.Y), 0.f);
			const FVector Along(-Nf.Y, Nf.X, 0.f);
			const float Floor = Office->GetActorLocation().Z;
			const float FacingYaw = FBGen::YawFacing(Nf);

			// The office door becomes a real door (opens; the model's glass panel goes).
			const float DoorYaw = FMath::RadiansToDegrees(FMath::Atan2(Nf.Y, Nf.X));
			const FVector PanelDir = FRotator(0.f, DoorYaw, 0.f).RotateVector(FVector(0.f, 1.f, 0.f));
			OfficeDoor->SetActorHiddenInGame(true);
			OfficeDoor->SetActorEnableCollision(false);
#if WITH_EDITOR
			OfficeDoor->SetIsTemporarilyHiddenInEditor(true);
#endif
			if (AFBDoor* D = Spawn<AFBDoor>(FVector(Dc.X, Dc.Y, Floor + 1.f) - PanelDir * 50.f, DoorYaw, TEXT("Doors"), [](AFBDoor*) {}))
			{
				TInlineComponentArray<USceneComponent*> Comps(D);
				USceneComponent* HingeComp = nullptr;
				UStaticMeshComponent* Cube = nullptr;
				for (USceneComponent* C : Comps)
				{
					if (C->GetFName() == TEXT("Hinge")) HingeComp = C;
					else if (C->GetFName() == TEXT("DoorMesh")) Cube = Cast<UStaticMeshComponent>(C);
				}
				UStaticMeshComponent* PanelComp = OfficeDoor->GetStaticMeshComponent();
				if (HingeComp && Cube && PanelComp && PanelComp->GetStaticMesh())
				{
					UStaticMeshComponent* Look = NewObject<UStaticMeshComponent>(D, TEXT("DoorLook"), RF_Transactional);
					Look->CreationMethod = EComponentCreationMethod::Instance;
					Look->SetStaticMesh(PanelComp->GetStaticMesh());
					for (int32 M = 0; M < PanelComp->GetNumMaterials(); ++M)
					{
						Look->SetMaterial(M, PanelComp->GetMaterial(M));
					}
					Look->SetupAttachment(HingeComp);
					Look->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					D->AddInstanceComponent(Look);
					Look->RegisterComponent();
					Look->SetWorldTransform(PanelComp->GetComponentTransform());
					Cube->SetVisibility(false);
					Cube->SetHiddenInGame(true);
				}
			}

			// A concrete walk along the door side of every cabin and the office (motels have one; no lawn at the doors),
			// and the office gets a floor (the model is open underneath).
			static UStaticMesh* WalkCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			auto Slab = [&](const FVector& C, const FVector& Size, UMaterialInterface* Mat, const TCHAR* Label)
			{
				if (AStaticMeshActor* A = WalkCube ? Spawn<AStaticMeshActor>(C, 0.f, TEXT("Town/Story/MotelWalk"), [&](AStaticMeshActor* M)
				{
					M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
					M->GetStaticMeshComponent()->SetStaticMesh(WalkCube);
					if (Mat)
					{
						M->GetStaticMeshComponent()->SetMaterial(0, Mat);
					}
				}) : nullptr)
				{
					A->SetActorScale3D(Size / 100.f);
#if WITH_EDITOR
					A->SetActorLabel(Label);
#endif
				}
			};
			const FVector LotC = FBDefaults::MotelParking.bIsValid ? Base + FVector(FBDefaults::MotelParking.GetCenter().X, FBDefaults::MotelParking.GetCenter().Y, 0.f) : Oc + Nf * 1000.f;
			for (AStaticMeshActor* B : Fronts)
			{
				const FBox Fb = B->GetComponentsBoundingBox();
				const float Fz = B->GetActorLocation().Z;
				// Doors face the lot (the main row and the row across from it are turned towards each other).
				const FVector Axis = FMath::Abs(Nf.Y) > 0.5f ? FVector(0.f, 1.f, 0.f) : FVector(1.f, 0.f, 0.f);
				const FVector NfB = Axis * (FVector::DotProduct(LotC - Fb.GetCenter(), Axis) >= 0.f ? 1.f : -1.f);
				const bool bAlongX = FMath::Abs(NfB.Y) > 0.5f; // the door wall runs along X
				const float FrontEdge = bAlongX ? (NfB.Y > 0.f ? Fb.Max.Y : Fb.Min.Y) : (NfB.X > 0.f ? Fb.Max.X : Fb.Min.X);
				const float Depth = 260.f;
				FVector C = Fb.GetCenter();
				FVector Size;
				if (bAlongX)
				{
					C.Y = FrontEdge + NfB.Y * (Depth * 0.5f - 10.f);
					Size = FVector(Fb.GetSize().X + 60.f, Depth, 0.f);
				}
				else
				{
					C.X = FrontEdge + NfB.X * (Depth * 0.5f - 10.f);
					Size = FVector(Depth, Fb.GetSize().Y + 60.f, 0.f);
				}
				// 12 cm curb-high walk, sunk 30 cm into the ground so it never floats.
				Size.Z = 42.f;
				C.Z = Fz + 12.f - Size.Z * 0.5f;
				Slab(C, Size, Mats.Paving ? Mats.Paving : Mats.Asphalt, TEXT("Motel_Walk"));
			}
			Slab(FVector(Oc.X, Oc.Y, Floor + 1.f - 15.f), FVector(Ob.GetSize().X - 30.f, Ob.GetSize().Y - 30.f, 30.f), Mats.Planks, TEXT("Motel_OfficeFloor"));

			// Counter across the middle of the office, the clerk behind it, a light over it.
			UStaticMesh* Counter = FBDefaults::LoadModel(TEXT("Props"), TEXT("diner_counter"));
			const FVector CounterAt = FVector(Oc.X, Oc.Y, Floor) - Nf * 30.f + Along * 40.f;
			if (Counter)
			{
				// Long side along the office front, 1.05 m tall, standing on the floor.
				const FBox MB = FBDefaults::ModelBox(Counter);
				const float Sc = 105.f / FMath::Max(1.f, MB.GetSize().Z);
				const float CYaw = FBGen::YawFacing(Along) + (MB.GetSize().X >= MB.GetSize().Y ? 0.f : 90.f);
				const FBox WB = MB.TransformBy(FTransform(FRotator(0.f, CYaw, 0.f), FVector::ZeroVector, FVector(Sc)));
				const FVector At(CounterAt.X - WB.GetCenter().X, CounterAt.Y - WB.GetCenter().Y, Floor - WB.Min.Z);
				for (UStaticMesh* Part : FBDefaults::Parts(Counter))
				{
					if (AStaticMeshActor* A = Spawn<AStaticMeshActor>(At, CYaw, TEXT("Town/Story/MotelOffice"), [&](AStaticMeshActor* M)
					{
						M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
						M->GetStaticMeshComponent()->SetStaticMesh(Part);
					}))
					{
						A->SetActorScale3D(FVector(Sc));
					}
				}
			}
			const FVector ClerkAt = FVector(CounterAt.X, CounterAt.Y, Floor + 92.f) - Nf * 115.f;
			AFBNPC* Clerk = Spawn<AFBNPC>(ClerkAt, FacingYaw, TEXT("Gameplay"), [](AFBNPC* N)
			{
				FBGen::SetProp(N, TEXT("Preset"), TEXT("MotelClerk"));
			});
			if (Clerk)
			{
				// His look: a townsperson body with its idle (the grey placeholder shapes go).
				if (FBDefaults::People.Num() == 0)
				{
					FBDefaults::LoadPeople();
				}
				const FBDefaults::FPerson* Who = nullptr;
				for (const FBDefaults::FPerson& Pp : FBDefaults::People)
				{
					if (Pp.Mesh.IsValid() && Pp.Mesh->GetName().Contains(TEXT("eric"), ESearchCase::IgnoreCase))
					{
						Who = &Pp;
					}
				}
				if (Who)
				{
					USkeletalMeshComponent* Body = NewObject<USkeletalMeshComponent>(Clerk, TEXT("ClerkBody"), RF_Transactional);
					Body->CreationMethod = EComponentCreationMethod::Instance;
					Body->SetSkeletalMesh(Who->Mesh.Get());
					Body->SetupAttachment(Clerk->GetRootComponent());
					Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Body->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
					Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // the rigs face +Y
					{
						// Imported rigs come in at any size (this one is ~100x): about 1.78 m already in the editor; the NPC refits it in play.
						const float H = Who->Mesh->GetBounds().BoxExtent.Z * 2.f;
						if (H > 1.f)
						{
							Body->SetRelativeScale3D(FVector(178.f / H));
						}
					}
					if (Who->Anim.IsValid())
					{
						Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						Body->AnimationData.AnimToPlay = Who->Anim.Get();
						Body->AnimationData.bSavedLooping = true;
						Body->AnimationData.bSavedPlaying = true;
					}
					Clerk->AddInstanceComponent(Body);
					Body->RegisterComponent();
					FBRealModels::HideGreyShapes(Clerk);
				}
				Spawn<APointLight>(FVector(Oc.X, Oc.Y, Floor + 235.f), 0.f, TEXT("Town/Story/MotelOffice"), [](APointLight* L)
				{
					UPointLightComponent* C = L->PointLightComponent;
					C->SetMobility(EComponentMobility::Movable);
					C->SetIntensityUnits(ELightUnits::Candelas);
					C->SetIntensity(35.f);
					C->SetLightColor(FLinearColor(1.f, 0.85f, 0.62f));
					C->SetAttenuationRadius(600.f);
				});
				UE_LOG(LogFBMapGen, Log, TEXT("Motel office: clerk at %s (look %s)."), *ClerkAt.ToString(), Who ? *Who->Mesh->GetName() : TEXT("placeholder"));
			}
		}
		else
		{
			UE_LOG(LogFBMapGen, Warning, TEXT("Motel office not found (office %d, door %d)."), Office != nullptr, OfficeDoor != nullptr);
		}
	}

	// Martha behind the diner counter, the archive computer in the library.
	if (Diner.IsValid())
	{
		AFBNPC* Martha = Spawn<AFBNPC>(Diner->GetActorTransform().TransformPosition(FBDefaults::DinerLocal(FVector(-150.f, 300.f, 92.f))), Diner->GetActorRotation().Yaw, TEXT("Gameplay"), [](AFBNPC*) {});

		// Martha's real look (if imported): the waitress with her notepad, standing behind the counter.
		if (Martha && FBRealModels::AttachModel(Martha, Martha->GetRootComponent(), TEXT("Characters"), TEXT("char_martha"), 166.f,
			FRotator(0.f, -90.f, 0.f), FVector(0.f, 0.f, -88.f), true, true)) // the scan faces +Y; turn it to face forward
		{
			FBRealModels::HideGreyShapes(Martha);
		}
	}
	if (Library.IsValid() && !FBDefaults::bComputerPlaced)
	{
		// Desk, chair and room to sit: about 1.9 m x 1.4 m x 1.6 m, somewhere free inside the library.
		float DeskYaw = 0.f;
		const FVector DeskAt = FreeSpotInside(Library.Get(), FVector(-350.f, 400.f, 0.f), FVector(45.f, 80.f, 80.f), &DeskYaw);
		Spawn<AFBResearchComputer>(DeskAt, DeskYaw, TEXT("Gameplay"), [](AFBResearchComputer*) {});
	}

	// ---- Evidence
	auto Evidence = [this](const AActor* Where, const FVector& Local, const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Clues,
		const TCHAR* Model = nullptr, float Size = 25.f, const FVector& WorldAt = FVector::ZeroVector, float WorldYaw = 0.f)
	{
		if (!Where && WorldAt.IsZero())
		{
			return;
		}
		const FVector At = WorldAt.IsZero() ? InBuilding(Where, Local) : WorldAt;
		const float AtYaw = WorldAt.IsZero() ? 0.f : WorldYaw;
		Spawn<AFBEvidenceItem>(At, AtYaw, TEXT("Gameplay/Evidence"), [&](AFBEvidenceItem* E)
		{
			FBGen::SetProp(E, TEXT("EvidenceId"), Id);
			FBGen::SetProp(E, TEXT("DisplayName"), FBGen::Text(Name));
			FBGen::SetProp(E, TEXT("Description"), FBGen::Text(Desc));
			if (Clues && *Clues)
			{
				FBGen::SetProp(E, TEXT("GrantsClues"), Clues);
			}
			if (Model)
			{
				FBGen::SetProp(E, TEXT("ModelName"), Model);
				FBGen::SetProp(E, TEXT("ModelSize"), *FString::SanitizeFloat(Size));
			}
		});
	};
	Evidence(Sheriff.Get(), FVector(-300.f, -300.f, 5.f), TEXT("Ev_MissingFlyers"), TEXT("Missing persons flyers"),
		TEXT("Three faces. All of them last seen on the old mill road."), TEXT("(\"Clue_OldMill\")"), TEXT("postcard_set_01"), 40.f);
	Evidence(Motel.Get(), FVector(-200.f, -1100.f, 5.f), TEXT("Ev_GuestBook"), TEXT("Motel guest book"),
		TEXT("Room 4 left in the middle of the night. Someone wrote COLD on the mirror."), TEXT("(\"Clue_ColdSpots\")"), TEXT("binder_notebook"), 30.f,
		FBDefaults::GuestBookAt, FBDefaults::GuestBookYaw);
	Evidence(Mill.Get(), FVector(-300.f, -400.f, 5.f), TEXT("Ev_CraneBadge"), TEXT("Charred pocket watch"),
		TEXT("Engraved: E. CRANE - FOREMAN. Scorched black, stopped at 11:52."), TEXT("(\"Clue_Crane\")"), TEXT("vintage_pocket_watch"), 13.f);
	Evidence(Cabin.Get(), FVector(0.f, 0.f, 5.f), TEXT("Ev_Jacket"), TEXT("Abandoned suitcase"),
		TEXT("Dale Whitmore's. Clothes still folded inside, the lid clawed open. Whatever did this wasn't a bear."), TEXT(""), TEXT("vintage_suitcase"), 150.f);

	// ---- Pickups
	auto Pickup = [this](const AActor* Where, const FVector& Local, const FString& ItemText)
	{
		if (!Where)
		{
			return;
		}
		Spawn<AFBItemPickup>(InBuilding(Where, Local), 0.f, TEXT("Gameplay/Items"), [&](AFBItemPickup* P)
		{
			if (!ItemText.IsEmpty())
			{
				FBGen::SetProp(P, TEXT("Item"), ItemText);
			}
		});
	};
	Pickup(Sheriff.Get(), FVector(-350.f, 400.f, 5.f),
		TEXT("(ItemId=\"MillKey\",DisplayName=INVTEXT(\"Old mill key\"),Description=INVTEXT(\"Rusty iron key. Tag: HARLOW MILL - NORTH DOOR.\"),Category=Key,Quantity=1,bStackable=False)"));
	// (The rock salt shells come from the car trunk now.)
	Pickup(Church.Get(), Church.IsValid() && Church->ActorHasTag(TEXT("FBReplaced")) ? FVector(-300.f, 250.f, 5.f) : FVector(-500.f, 0.f, 5.f),
		TEXT("(ItemId=\"Salt\",DisplayName=INVTEXT(\"Salt\"),Category=Ritual,Quantity=2)"));
	// A spare battery on the room 7 desk, next to the guest book (the motel's grey box is under the real floor).
	if (!FBDefaults::GuestBookAt.IsZero())
	{
		Spawn<AFBItemPickup>(FBDefaults::GuestBookAt + FVector(0.f, 0.f, 2.f) + FRotator(0.f, FBDefaults::GuestBookYaw, 0.f).RotateVector(FVector(0.f, -35.f, 0.f)), 0.f, TEXT("Gameplay/Items"), [](AFBItemPickup*) {});
	}
	Pickup(Barn.Get(), FVector(-400.f, 300.f, 5.f), FString());

	// ---- The haunted mill
	AFBGhost* Ghost = nullptr;
	if (Mill.IsValid())
	{
		AActor* M = Mill.Get();
		Ghost = Spawn<AFBGhost>(InBuilding(M, FVector(-400.f, 0.f, 0.f)), 0.f, TEXT("Gameplay/Mill"), [](AFBGhost*) {});
		Spawn<AFBParanormalHotspot>(InBuilding(M, FVector(-500.f, 300.f, 0.f)), 0.f, TEXT("Gameplay/Mill"), [](AFBParanormalHotspot*) {});
		Spawn<AFBUVTrace>(InBuilding(M, FVector(300.f, -300.f, 2.f)), 0.f, TEXT("Gameplay/Mill"), [](AFBUVTrace* U)
		{
			FBGen::SetProp(U, TEXT("Description"), FBGen::Text(TEXT("Small handprints, leading to the back wall")));
		});
		Spawn<AFBUVTrace>(InBuilding(M, FVector(-150.f, 450.f, 2.f)), 0.f, TEXT("Gameplay/Mill"), [](AFBUVTrace* U)
		{
			FBGen::SetProp(U, TEXT("Description"), FBGen::Text(TEXT("Scorch marks shaped like fingers")));
		});
		Spawn<AFBChandelier>(InBuilding(M, FVector(100.f, 0.f, 380.f)), 0.f, TEXT("Gameplay/Mill"), [](AFBChandelier*) {});

		for (int32 i = 0; i < 3; ++i)
		{
			Spawn<AFBIronPipe>(InBuilding(M, FVector(650.f, -450.f + i * 60.f, 30.f)), 90.f, TEXT("Gameplay/Physics"), [](AFBIronPipe*) {});
			Spawn<AFBPhysicsProp>(InBuilding(M, FVector(450.f, 350.f + i * 40.f, 20.f + i * 6.f)), 15.f * i, TEXT("Gameplay/Physics"), [](AFBPhysicsProp* P)
			{
				FBGen::SetProp(P, TEXT("Shape"), TEXT("Plank"));
				FBGen::SetProp(P, TEXT("DisplayName"), FBGen::Text(TEXT("Loose plank")));
			});
		}
	}

	// Crates and an iron pipe in the barn.
	if (Barn.IsValid())
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector Local(-300.f + (i % 3) * 80.f, -300.f + (i / 3) * 80.f, 30.f);
			Spawn<AFBPhysicsProp>(InBuilding(Barn.Get(), Local), 10.f * i, TEXT("Gameplay/Physics"), [](AFBPhysicsProp*) {});
		}
		Spawn<AFBIronPipe>(InBuilding(Barn.Get(), FVector(300.f, 200.f, 30.f)), 0.f, TEXT("Gameplay/Physics"), [](AFBIronPipe*) {});
	}

	// Elias Crane's grave, in the middle of the cemetery path.
	if (CemeteryActor.IsValid())
	{
		Spawn<AFBGraveSite>(CemeteryActor->GetActorLocation(), CemeteryActor->GetActorRotation().Yaw, TEXT("Gameplay"), [&](AFBGraveSite* G)
		{
			FBGen::SetObjectProp(G, TEXT("LinkedGhost"), Ghost);
		});
	}

	// The Gaunt hunts the woods around the hunter's cabin.
	Spawn<AFBCreature>(Base + FVector(-3200.f, 13400.f, 100.f), -90.f, TEXT("Gameplay"), [](AFBCreature*) {});
}
