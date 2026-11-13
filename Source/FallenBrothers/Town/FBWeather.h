// Fallen Brothers - Weather: rain showers that come and go, wet streets that dry slowly.
//
// Rain: thousands of thin lit streaks around the camera (they fall in the vertex shader, so they cost no CPU),
// lit by street lamps and flashlights, hidden under a roof. The rain loop plays (muffled indoors).
// Wet ground: decals along the streets (placed by the map generator, tag "FBWet") get darker and glossy,
// with mirror-like puddles; they follow Wetness (wets fast in rain, dries over minutes).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBWeather.generated.h"

class UInstancedStaticMeshComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;

UCLASS()
class FALLENBROTHERS_API AFBWeather : public AActor
{
	GENERATED_BODY()

public:
	AFBWeather();

	virtual void Tick(float DeltaSeconds) override;

	/** Rain now (0 = dry, 1 = downpour). With Auto Weather on this drifts by itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1"))
	float Rain = 0.6f;

	/** How wet the ground is (0..1). Rises in rain, dries slowly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1"))
	float Wetness = 1.f;

	/** Showers come and go on their own. Off = Rain stays where it is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	bool bAutoWeather = true;

	/** Number of rain streaks at full rain. */
	UPROPERTY(EditAnywhere, Category = "Weather")
	int32 MaxDrops = 7000;

	/** Radius around the camera that has rain (cm). */
	UPROPERTY(EditAnywhere, Category = "Weather")
	float Radius = 2200.f;

	/** Minutes for a soaked street to dry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float DryMinutes = 6.f;

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetRain(float NewRain, bool bStopAuto = true);

	/** Drops hitting the ground (little crowns) and the lake (rings) per second at full rain. */
	UPROPERTY(EditAnywhere, Category = "Weather")
	float SplashesPerSecond = 1400.f;

	/** The weather of this world (nullptr if there is none). */
	static AFBWeather* Find(const UWorld* World);

	/** Inside a town building's footprint (its walls), whether or not the model has collision. */
	static bool InsideBuilding(const UWorld* World, const FVector& Location);

	/** Under a roof / inside a building (roof models without collision count too). */
	static bool IsIndoors(const UWorld* World, const FVector& Location, const AActor* Ignore = nullptr);

	/** How wet the ground is at a spot (0 under a roof / indoors). */
	static float WetnessAt(const UWorld* World, const FVector& Location, const AActor* Ignore = nullptr);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Drops;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Splashes;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UAudioComponent> Sound;

private:
	void BuildDrops();
	void UpdateSplashes(float DeltaSeconds, const FVector& Eye, float Visible);
	void UpdateWeatherTarget(float Now);

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RainMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> WetMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SplashMID;

	struct FLake { TWeakObjectPtr<AActor> Water; FBox Box; float SurfaceZ = 0.f; };
	TArray<FLake> Lakes;
	int32 SplashCount = 900;
	int32 NextSplash = 0;
	float SplashBudget = 0.f;

	float TargetRain = 0.6f;
	float NextChange = 0.f;
	float Indoors = 0.f;     // 0 outside .. 1 under a roof (smoothed)
	float RoofTimer = 0.f;
	bool bRoof = false;
	float WetPushTimer = 0.f;
	float LastWetPushed = -1.f;
	float LastRainPushed = -1.f;
};
