// Fallen Brothers - Footsteps.

#include "Town/FBFootsteps.h"
#include "Town/FBWeather.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	enum class ESurf : uint8 { Boots, Grass, Gravel, Metal, Tile, Water, Wood, Num };
	const TCHAR* SurfName[] = { TEXT("boots"), TEXT("grass"), TEXT("gravel"), TEXT("metal"), TEXT("tile"), TEXT("water"), TEXT("wood") };

	TArray<TWeakObjectPtr<USoundBase>>& Sounds(ESurf S)
	{
		static TArray<TWeakObjectPtr<USoundBase>> Cache[int32(ESurf::Num)];
		static bool bLoaded[int32(ESurf::Num)] = {};
		const int32 I = int32(S);
		if (!bLoaded[I])
		{
			bLoaded[I] = true;
			for (int32 K = 0; K < 10; ++K)
			{
				const FString P = FString::Printf(TEXT("/Game/FallenBrothers/Audio/Steps/%s/S_%s_%d.S_%s_%d"), SurfName[I], SurfName[I], K, SurfName[I], K);
				if (USoundBase* Snd = LoadObject<USoundBase>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					Snd->AddToRoot();
					Cache[I].Add(Snd);
				}
			}
		}
		return Cache[I];
	}

	USoundAttenuation* Attenuation()
	{
		static USoundAttenuation* A = nullptr;
		if (!A)
		{
			A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("FBFootstepAttenuation"));
			A->AddToRoot();
			A->Attenuation.bAttenuate = true;
			A->Attenuation.bSpatialize = true;
			A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			A->Attenuation.AttenuationShapeExtents = FVector(150.f, 0.f, 0.f);
			A->Attenuation.FalloffDistance = 2200.f;
			A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		}
		return A;
	}

	bool Has(const FString& S, std::initializer_list<const TCHAR*> Words)
	{
		for (const TCHAR* W : Words)
		{
			if (S.Contains(W))
			{
				return true;
			}
		}
		return false;
	}

	ESurf Classify(const FHitResult& Hit)
	{
		const AActor* A = Hit.GetActor();
		const UPrimitiveComponent* C = Hit.GetComponent();
		FString Names = A ? A->GetClass()->GetName() + TEXT(" ") + A->GetName() : FString();
		if (C)
		{
			Names += TEXT(" ") + C->GetName();
			if (const UMaterialInterface* M = C->GetMaterial(int32(Hit.ElementIndex)))
			{
				Names += TEXT(" ") + M->GetName();
			}
			if (C->GetNumMaterials() > 0 && C->GetMaterial(0))
			{
				Names += TEXT(" ") + C->GetMaterial(0)->GetName();
			}
		}
		Names.ToLowerInline();
		if (Has(Names, { TEXT("fbwater"), TEXT("lake"), TEXT("puddle") })) return ESurf::Water;
		if (Has(Names, { TEXT("metal"), TEXT("iron"), TEXT("steel"), TEXT("grate"), TEXT("tin") })) return ESurf::Metal;
		if (Has(Names, { TEXT("tile"), TEXT("marble"), TEXT("ceramic"), TEXT("linoleum") })) return ESurf::Tile;
		if (Has(Names, { TEXT("wood"), TEXT("plank"), TEXT("parquet"), TEXT("board"), TEXT("deck"), TEXT("porch"), TEXT("floor"), TEXT("stair") })) return ESurf::Wood;
		if (Has(Names, { TEXT("gravel"), TEXT("rubble"), TEXT("pebble"), TEXT("dirt_road"), TEXT("rock") })) return ESurf::Gravel;
		if (Has(Names, { TEXT("fbroad"), TEXT("asphalt"), TEXT("concrete"), TEXT("paving"), TEXT("sidewalk"), TEXT("road"), TEXT("brick"), TEXT("cement") })) return ESurf::Boots;
		if (Has(Names, { TEXT("terrain"), TEXT("landscape"), TEXT("grass"), TEXT("forest"), TEXT("soil"), TEXT("dirt"), TEXT("mud"), TEXT("moss"), TEXT("leaf"), TEXT("ground") })) return ESurf::Grass;
		return ESurf::Boots;
	}

	void PlayOne(const AActor* Walker, ESurf S, const FVector& At, float Volume)
	{
		TArray<TWeakObjectPtr<USoundBase>>& List = Sounds(S);
		if (List.Num() == 0 || Volume < 0.01f)
		{
			return;
		}
		if (USoundBase* Snd = List[FMath::RandRange(0, List.Num() - 1)].Get())
		{
			UGameplayStatics::PlaySoundAtLocation(Walker, Snd, At, FRotator::ZeroRotator, Volume, FMath::FRandRange(0.92f, 1.08f), 0.f, Attenuation());
		}
	}
}

void FBFootsteps::Play(const AActor* Walker, const FVector& Foot, float Volume)
{
	UWorld* World = Walker ? Walker->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FBFootstep), true, Walker);
	if (!World->LineTraceSingleByChannel(Hit, Foot + FVector(0.f, 0.f, 40.f), Foot - FVector(0.f, 0.f, 80.f), ECC_Visibility, Q))
	{
		return; // in the air
	}
	ESurf S = Classify(Hit);

	// Wet hard ground: the step splashes (more the wetter it is); grass gets soggy, a little louder.
	const float Wet = (S == ESurf::Water) ? 0.f : AFBWeather::WetnessAt(World, Hit.ImpactPoint, Walker);
	if (Wet > 0.2f && (S == ESurf::Boots || S == ESurf::Gravel || S == ESurf::Tile || S == ESurf::Metal))
	{
		PlayOne(Walker, ESurf::Water, Hit.ImpactPoint, Volume * FMath::Lerp(0.35f, 0.9f, Wet));
		PlayOne(Walker, S, Hit.ImpactPoint, Volume * FMath::Lerp(0.8f, 0.45f, Wet));
		return;
	}
	PlayOne(Walker, S, Hit.ImpactPoint, Volume * (S == ESurf::Grass && Wet > 0.2f ? 1.15f : 1.f));
}
