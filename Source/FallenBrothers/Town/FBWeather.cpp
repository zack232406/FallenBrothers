// Fallen Brothers - Weather.

#include "Town/FBWeather.h"
#include "Town/FBAtmosphere.h"
#include "Environment/FBWeatherZones.h"

#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/DecalActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	const TCHAR* RainMatPath = TEXT("/Game/FallenBrothers/Weather/M_FB_Rain.M_FB_Rain");
	const TCHAR* WetMatPath = TEXT("/Game/FallenBrothers/Weather/M_FB_Wet.M_FB_Wet");
	const TCHAR* SplashMatPath = TEXT("/Game/FallenBrothers/Weather/M_FB_Splash.M_FB_Splash");
	const TCHAR* RainSoundPath = TEXT("/Game/FallenBrothers/Weather/SW_Rain.SW_Rain");
	constexpr float FallHeight = 1600.f;
}

AFBWeather::AFBWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the camera moved

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Drops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Drops"));
	Drops->SetupAttachment(Root);
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->SetMobility(EComponentMobility::Movable);
	Drops->bReceivesDecals = false;
	Drops->SetCanEverAffectNavigation(false);
	Drops->bAffectDistanceFieldLighting = false;
	Drops->bVisibleInRayTracing = false;
	Drops->SetGenerateOverlapEvents(false);
	// Drops fall up to FallHeight below their spawn point: keep them from being culled early.
	Drops->SetBoundsScale(3.f);

	// Splashes live in world space (the actor itself follows the camera).
	Splashes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Splashes"));
	Splashes->SetupAttachment(Root);
	Splashes->SetUsingAbsoluteLocation(true);
	Splashes->SetUsingAbsoluteRotation(true);
	Splashes->SetUsingAbsoluteScale(true);
	Splashes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Splashes->SetCastShadow(false);
	Splashes->SetMobility(EComponentMobility::Movable);
	Splashes->bReceivesDecals = false;
	Splashes->SetCanEverAffectNavigation(false);
	Splashes->bAffectDistanceFieldLighting = false;
	Splashes->bVisibleInRayTracing = false;
	Splashes->SetGenerateOverlapEvents(false);
	Splashes->NumCustomDataFloats = 2; // start time, duration

	Sound = CreateDefaultSubobject<UAudioComponent>(TEXT("Sound"));
	Sound->SetupAttachment(Root);
	Sound->bAutoActivate = false;
	Sound->bAllowSpatialization = false; // the rain is all around
}

void AFBWeather::BuildDrops()
{
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, RainMatPath);
	if (!Plane || !Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("FBWeather: rain assets missing (%s)"), RainMatPath);
		return;
	}
	Drops->SetStaticMesh(Plane);
	RainMID = UMaterialInstanceDynamic::Create(Mat, this);
	RainMID->SetScalarParameterValue(TEXT("FallHeight"), FallHeight);
	RainMID->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
	Drops->SetMaterial(0, RainMID);

	// A vertical streak 1.2 cm x 90 cm (motion-blurred drop) (plane is 100 x 100 in XY: stand it up), random yaw, random spot in a disc,
	// spawn heights spread over the fall so the column is evenly full. The material makes them fall.
	FRandomStream R(1234);
	TArray<FTransform> T;
	T.Reserve(MaxDrops);
	for (int32 I = 0; I < MaxDrops; ++I)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float D = Radius * FMath::Sqrt(R.FRand());
		const FVector P(D * FMath::Cos(A), D * FMath::Sin(A), FallHeight * 0.5f);
		const FRotator Rot(0.f, R.FRandRange(0.f, 360.f), 90.f);
		T.Add(FTransform(Rot, P, FVector(0.012f, 0.9f, 1.f)));
	}
	Drops->AddInstances(T, false, false, false);
}

void AFBWeather::BeginPlay()
{
	Super::BeginPlay();
	BuildDrops();

	if (UMaterialInterface* Wet = LoadObject<UMaterialInterface>(nullptr, WetMatPath))
	{
		WetMID = UMaterialInstanceDynamic::Create(Wet, this);
		int32 N = 0;
		for (TActorIterator<ADecalActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("FBWet")) && It->GetDecal())
			{
				It->GetDecal()->SetDecalMaterial(WetMID);
				++N;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("FBWeather: %d wet decals"), N);
	}
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, RainSoundPath))
	{
		Sound->SetSound(S);
		Sound->SetVolumeMultiplier(0.01f);
		Sound->Play();
	}
	// Splashes: a pool of little planes, re-used round-robin.
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (UMaterialInterface* SM = LoadObject<UMaterialInterface>(nullptr, SplashMatPath); SM && Plane)
	{
		Splashes->SetStaticMesh(Plane);
		Splashes->SetWorldLocation(FVector::ZeroVector);
		SplashMID = UMaterialInstanceDynamic::Create(SM, this);
		Splashes->SetMaterial(0, SplashMID);
		TArray<FTransform> T;
		T.Init(FTransform(FQuat::Identity, FVector(0.f, 0.f, -100000.f), FVector(0.001f)), SplashCount);
		Splashes->AddInstances(T, false, true, false);
		for (int32 I = 0; I < SplashCount; ++I)
		{
			Splashes->SetCustomDataValue(I, 0, -100.f, false);
			Splashes->SetCustomDataValue(I, 1, 0.1f, false);
		}
		Splashes->MarkRenderStateDirty();
	}
	for (TActorIterator<AFBWater> It(GetWorld()); It; ++It)
	{
		FLake L;
		L.Water = *It;
		L.Box = It->GetComponentsBoundingBox(true);
		L.SurfaceZ = It->GetSurfaceZ();
		Lakes.Add(L);
	}

	TargetRain = Rain;
	// Starting dry: a good long dry spell first (the player gets to see the town), then the weather turns.
	NextChange = GetWorld()->GetTimeSeconds() + (Rain < 0.05f ? FMath::FRandRange(480.f, 720.f) : FMath::FRandRange(120.f, 300.f));
}

void AFBWeather::SetRain(float NewRain, bool bStopAuto)
{
	TargetRain = Rain = FMath::Clamp(NewRain, 0.f, 1.f);
	if (bStopAuto)
	{
		bAutoWeather = false;
	}
}

void AFBWeather::UpdateWeatherTarget(float Now)
{
	if (!bAutoWeather || Now < NextChange)
	{
		return;
	}
	// Midwest autumn: grey, often raining. Dry spells 3-8 min, showers 2-6 min, sometimes a downpour.
	if (TargetRain > 0.05f)
	{
		TargetRain = 0.f;
		NextChange = Now + FMath::FRandRange(180.f, 480.f);
	}
	else
	{
		TargetRain = FMath::FRand() < 0.25f ? FMath::FRandRange(0.85f, 1.f) : FMath::FRandRange(0.3f, 0.7f);
		NextChange = Now + FMath::FRandRange(120.f, 360.f);
	}
}

AFBWeather* AFBWeather::Find(const UWorld* World)
{
	static TWeakObjectPtr<AFBWeather> Cached;
	if (!World)
	{
		return nullptr;
	}
	if (!Cached.IsValid() || Cached->GetWorld() != World)
	{
		Cached = nullptr;
		for (TActorIterator<AFBWeather> It(const_cast<UWorld*>(World)); It; ++It)
		{
			Cached = *It;
			break;
		}
	}
	return Cached.Get();
}

bool AFBWeather::InsideBuilding(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return false;
	}
	// The buildings' footprints (the town's building actors; their real models are often without collision).
	static TWeakObjectPtr<const UWorld> CachedWorld;
	static TArray<FBox> Boxes;
	if (CachedWorld.Get() != World)
	{
		CachedWorld = World;
		Boxes.Reset();
		for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (It->GetClass()->GetName().StartsWith(TEXT("FBBuilding")))
			{
				const FBox B = It->GetComponentsBoundingBox(true);
				if (B.IsValid && B.GetSize().X < 6000.f && B.GetSize().Y < 6000.f)
				{
					Boxes.Add(B.ExpandBy(FVector(-25.f, -25.f, 0.f)));
				}
			}
		}
	}
	for (const FBox& B : Boxes)
	{
		if (Location.X > B.Min.X && Location.X < B.Max.X && Location.Y > B.Min.Y && Location.Y < B.Max.Y && Location.Z > B.Min.Z - 50.f && Location.Z < B.Max.Z)
		{
			return true;
		}
	}
	return false;
}

bool AFBWeather::IsIndoors(const UWorld* World, const FVector& Location, const AActor* Ignore)
{
	if (!World)
	{
		return false;
	}
	if (InsideBuilding(World, Location))
	{
		return true;
	}
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FBRoof), false, Ignore);
	return World->LineTraceSingleByChannel(Hit, Location, Location + FVector(0.f, 0.f, 3000.f), ECC_Visibility, Q);
}

float AFBWeather::WetnessAt(const UWorld* World, const FVector& Location, const AActor* Ignore)
{
	const AFBWeather* W = Find(World);
	if (!W || W->Wetness < 0.02f)
	{
		return 0.f;
	}
	return IsIndoors(World, Location + FVector(0.f, 0.f, 120.f), Ignore) ? 0.f : W->Wetness;
}

void AFBWeather::UpdateSplashes(float DeltaSeconds, const FVector& Eye, float Visible)
{
	if (!SplashMID || Splashes->GetInstanceCount() == 0)
	{
		return;
	}
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	SplashBudget += DeltaSeconds * SplashesPerSecond * Visible;
	int32 N = FMath::Min(FMath::FloorToInt(SplashBudget), 90);
	SplashBudget -= N;
	if (N <= 0)
	{
		return;
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FBSplash), false);
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Q.AddIgnoredActor(Pawn);
	}
	for (int32 K = 0; K < N; ++K)
	{
		const int32 Index = NextSplash;
		NextSplash = (NextSplash + 1) % Splashes->GetInstanceCount();
		// Mostly near the camera (that's where you see single drops), a few further out.
		const float A = FMath::FRandRange(0.f, 2.f * PI);
		const float D = 1300.f * FMath::Square(FMath::FRand()) + 60.f;
		const FVector XY(Eye.X + D * FMath::Cos(A), Eye.Y + D * FMath::Sin(A), 0.f);

		// On the lake: a ring on the water, it lives longer.
		bool bOnLake = false;
		float WaterZ = 0.f;
		for (const FLake& L : Lakes)
		{
			const FVector C = L.Box.GetCenter(), E = L.Box.GetExtent();
			if (E.X > 1.f && E.Y > 1.f && FMath::Square((XY.X - C.X) / E.X) + FMath::Square((XY.Y - C.Y) / E.Y) < 0.55f)
			{
				bOnLake = true;
				WaterZ = L.SurfaceZ;
				break;
			}
		}
		FTransform T;
		float Duration;
		if (bOnLake)
		{
			const float Size = FMath::FRandRange(0.2f, 0.45f);
			T = FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), FVector(XY.X, XY.Y, WaterZ + 1.f), FVector(Size, Size, 1.f));
			Duration = FMath::FRandRange(0.7f, 1.1f);
		}
		else
		{
			FHitResult Hit;
			const FVector From(XY.X, XY.Y, Eye.Z + 600.f);
			if (!W->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, 2600.f), ECC_Visibility, Q)
				|| IsIndoors(W, Hit.ImpactPoint + FVector(0.f, 0.f, 30.f), nullptr))
			{
				continue;
			}
			// A small crown of water thrown up where the drop lands (standing plane, half of it below the ground).
			const float Size = FMath::FRandRange(0.08f, 0.14f);
			T = FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 90.f), Hit.ImpactPoint, FVector(Size, Size, 1.f));
			Duration = FMath::FRandRange(0.16f, 0.28f);
		}
		Splashes->UpdateInstanceTransform(Index, T, true, false, true);
		Splashes->SetCustomDataValue(Index, 0, Now, false);
		Splashes->SetCustomDataValue(Index, 1, Duration, false);
	}
	Splashes->MarkRenderStateDirty();
}

void AFBWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();

	UpdateWeatherTarget(Now);
	if (bAutoWeather)
	{
		Rain = FMath::FInterpConstantTo(Rain, TargetRain, DeltaSeconds, 1.f / 25.f); // showers build up over ~25 s
	}

	// Ground: soaks in ~40 s of real rain, dries over DryMinutes.
	if (Rain > 0.05f)
	{
		Wetness = FMath::Min(1.f, Wetness + DeltaSeconds * Rain / 40.f);
	}
	else
	{
		Wetness = FMath::Max(0.f, Wetness - DeltaSeconds / (FMath::Max(0.5f, DryMinutes) * 60.f));
	}
	WetPushTimer -= DeltaSeconds;
	if (WetMID && WetPushTimer <= 0.f && !FMath::IsNearlyEqual(Wetness, LastWetPushed, 0.002f))
	{
		WetPushTimer = 0.5f;
		LastWetPushed = Wetness;
		WetMID->SetScalarParameterValue(TEXT("Wetness"), Wetness);
	}

	// Follow the camera.
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return;
	}
	const FVector Eye = Cam->GetCameraLocation();
	SetActorLocation(Eye - FVector(0.f, 0.f, FallHeight * 0.35f));

	// Under a roof? (a few times a second)
	RoofTimer -= DeltaSeconds;
	if (RoofTimer <= 0.f)
	{
		RoofTimer = 0.25f;
		bRoof = IsIndoors(W, Eye, UGameplayStatics::GetPlayerPawn(this, 0));
	}
	Indoors = FMath::FInterpConstantTo(Indoors, bRoof ? 1.f : 0.f, DeltaSeconds, 4.f);

	// Rain streaks and wind.
	const float Visible = Rain * (1.f - Indoors);
	Drops->SetVisibility(Visible > 0.02f);
	if (RainMID)
	{
		RainMID->SetScalarParameterValue(TEXT("Amount"), FMath::Clamp(Visible * 1.2f, 0.f, 1.f));
		RainMID->SetScalarParameterValue(TEXT("Speed"), FMath::Lerp(750.f, 1050.f, Rain));
		// A faint glow so the streaks read against the sky (lit translucency alone is too dim); scaled to the light.
		const float Dark = AFBAtmosphere::DarknessIn(W);
		RainMID->SetScalarParameterValue(TEXT("Glow"), FMath::Lerp(60.f, 0.06f, Dark));
		if (SplashMID)
		{
			SplashMID->SetScalarParameterValue(TEXT("Glow"), FMath::Lerp(12.f, 0.05f, Dark));
			SplashMID->SetScalarParameterValue(TEXT("Amount"), FMath::Clamp(Rain * 1.3f, 0.f, 1.f));
		}
		const float Gust = 0.6f + 0.4f * FMath::Sin(Now * 0.37f) * FMath::Sin(Now * 0.11f);
		RainMID->SetScalarParameterValue(TEXT("WindX"), 160.f * Gust * Rain);
		RainMID->SetScalarParameterValue(TEXT("WindY"), 60.f * Gust * Rain);
	}
	UpdateSplashes(DeltaSeconds, Eye, Rain);
	if (WetMID && !FMath::IsNearlyEqual(Rain, LastRainPushed, 0.01f)) // puddle ripples follow the rain
	{
		LastRainPushed = Rain;
		WetMID->SetScalarParameterValue(TEXT("Rain"), Rain);
	}

	// Fewer streaks in light rain (draw only the first N instances' worth by scaling them away is costly; opacity does it).

	// Sound: louder with the rain, muffled indoors (on the roof).
	if (Sound->IsPlaying())
	{
		Sound->SetVolumeMultiplier(FMath::Max(0.001f, FMath::Pow(Rain, 0.7f) * FMath::Lerp(0.9f, 0.45f, Indoors)));
		Sound->SetLowPassFilterEnabled(true);
		Sound->SetLowPassFilterFrequency(FMath::Lerp(20000.f, 1200.f, Indoors));
	}

	// The sky follows: thicker mist in rain, lightning in a downpour.
	static TWeakObjectPtr<AFBAtmosphere> Atmo;
	if (!Atmo.IsValid() || Atmo->GetWorld() != W)
	{
		for (TActorIterator<AFBAtmosphere> It(W); It; ++It)
		{
			Atmo = *It;
			break;
		}
	}
	if (Atmo.IsValid())
	{
		Atmo->FogAmount = 1.f + 0.6f * Rain;
		Atmo->bForceLightning = Rain > 0.85f;
	}
}
