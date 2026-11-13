// Fallen Brothers - Atmosphere (one per level).
//
// Replaces the default sky / sun / fog actors with a horror-ready setup:
// moonlight, physically based night sky, ground fog with volumetric light shafts
// (flashlights and street lamps get visible beams), cold colour grading and lightning.
//
// Place ONE in the level and delete the default "Lighting" actors
// (DirectionalLight, SkyAtmosphere, SkyLight, ExponentialHeightFog, VolumetricCloud, SM_SkySphere).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FBAtmosphere.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

UENUM(BlueprintType)
enum class EFBMood : uint8
{
	ClearNight     UMETA(DisplayName = "Clear Night (moonlit)"),
	FoggyNight     UMETA(DisplayName = "Foggy Night"),
	StormNight     UMETA(DisplayName = "Storm Night (lightning)"),
	Dusk           UMETA(DisplayName = "Dusk (arrival)"),
	OvercastDay    UMETA(DisplayName = "Overcast Day")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLightningStrike);

UCLASS()
class FALLENBROTHERS_API AFBAtmosphere : public AActor
{
	GENERATED_BODY()

public:
	AFBAtmosphere();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	void SetMood(EFBMood NewMood);

	/** Flash now (also happens by itself during a storm). */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	void TriggerLightning();

	/** For thunder sounds later. */
	UPROPERTY(BlueprintAssignable, Category = "Atmosphere")
	FOnLightningStrike OnLightning;

	// ---------------------------------------------------------------- Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
	EFBMood Mood = EFBMood::FoggyNight;

	/** 1 = preset. Lower = darker nights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere", meta = (ClampMin = "0", ClampMax = "5"))
	float MoonBrightness = 1.f;

	/** 1 = preset. 0 = no fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere", meta = (ClampMin = "0", ClampMax = "5"))
	float FogAmount = 1.f;

	/** Moon height above the horizon (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere", meta = (ClampMin = "2", ClampMax = "89"))
	float MoonElevation = 35.f;

	/** Moon compass direction (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
	float MoonDirection = 210.f;

	/** Volumetric fog = visible light beams. Costs some GPU; turn off on weak PCs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
	bool bVolumetricFog = true;

	/** Lightning even when the mood isn't Storm. */
	// ---------------------------------------------------------------- Day / night
	/** The sun moves and the hours pass: dusk, night, dawn, day. Off = always the chosen mood. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Day Night")
	bool bDayNightCycle = true;

	/** Hour of the day (0-24). 21.5 = half past nine in the evening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Day Night", meta = (ClampMin = "0", ClampMax = "24"))
	float TimeOfDay = 21.f;

	/** Real minutes for a whole 24 h day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Day Night", meta = (ClampMin = "1"))
	float DayLengthMinutes = 48.f;

	/** Nights pass faster than days (1 = same speed). Keeps the dark long enough to be scary, but not endless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Day Night", meta = (ClampMin = "0.2", ClampMax = "5"))
	float NightSpeed = 1.f;

	/** Sunlight at noon (lux). Real direct sun: ~100000. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Day Night")
	float SunLux = 80000.f;

	/** How dark it is now: 0 = full day, 1 = night. Lamps and lit windows follow this. */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere|Day Night")
	float GetDarkness() const { return Darkness; }

	UFUNCTION(BlueprintCallable, Category = "Atmosphere|Day Night")
	void SetTimeOfDay(float Hours);

	/** Darkness of the atmosphere in this world (1 if there is none). */
	static float DarknessIn(const UWorld* World);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Lightning")
	bool bForceLightning = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Lightning")
	float LightningMinInterval = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Lightning")
	float LightningMaxInterval = 35.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USkyAtmosphereComponent> Sky;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPostProcessComponent> Grading;

private:
	struct FMoodValues
	{
		float MoonLux = 0.3f;
		FLinearColor MoonColor = FLinearColor(0.6f, 0.7f, 1.f);
		float SkyLightIntensity = 0.15f;
		float FogDensity = 0.03f;
		float FogFalloff = 0.25f;
		FLinearColor FogColor = FLinearColor(0.02f, 0.025f, 0.04f);
		float Saturation = 0.8f;
		FLinearColor Tint = FLinearColor(0.92f, 0.97f, 1.08f);
		bool bLightning = false;
	};

	FMoodValues GetMoodValues() const;
	void Apply();

	FMoodValues Current;
	float Darkness = 1.f;
	float CurrentMoonLux = 0.3f;
	float CurrentSkyLight = 0.15f;
	float ApplyTimer = 0.f;
	float NextLightningTime = 0.f;
	float LightningStartTime = -100.f;
	bool bWasFlashing = false;
};
