// Fallen Brothers - Atmosphere.

#include "Town/FBAtmosphere.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"

AFBAtmosphere::AFBAtmosphere()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// The sun (day) and the moon (night): two atmosphere lights, so the sky colours itself correctly.
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->bAtmosphereSunLight = true;
	Sun->AtmosphereSunLightIndex = 0;
	Sun->SetCastShadows(true);
	Sun->ForwardShadingPriority = 1; // at twilight the sun, not the moon, drives fog/translucency/water (no "competing lights" warning)
	Sun->SetLightSourceAngle(0.53f);
	Sun->SetLightColor(FLinearColor(1.f, 0.96f, 0.9f));

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->bAtmosphereSunLight = true;
	Moon->AtmosphereSunLightIndex = 1;
	Moon->SetCastShadows(true);
	Moon->ForwardShadingPriority = 0;

	Sky = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);

	// Real-time captured sky light: lights everything softly from the sky, follows the lightning.
	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	// Whole-level colour grading (the bodycam adds its own lens effects on top).
	Grading = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grading"));
	Grading->SetupAttachment(Root);
	Grading->bUnbound = true;
	Grading->Priority = -1.f; // Below any hand-placed post process volumes.
}

AFBAtmosphere::FMoodValues AFBAtmosphere::GetMoodValues() const
{
	FMoodValues V;
	switch (Mood)
	{
	case EFBMood::ClearNight:
		V.MoonLux = 0.9f;
		V.SkyLightIntensity = 0.4f;
		V.FogDensity = 0.01f;
		V.FogFalloff = 0.2f;
		break;

	case EFBMood::FoggyNight:
		V.MoonLux = 0.65f;
		V.SkyLightIntensity = 0.32f;
		V.FogDensity = 0.04f;
		V.FogFalloff = 0.35f;   // fog hugs the ground
		break;

	case EFBMood::StormNight:
		V.MoonLux = 0.3f;
		V.MoonColor = FLinearColor(0.55f, 0.6f, 0.8f);
		V.SkyLightIntensity = 0.22f;
		V.FogDensity = 0.03f;
		V.FogFalloff = 0.2f;
		V.Saturation = 0.7f;
		V.bLightning = true;
		break;

	case EFBMood::Dusk:
		V.MoonLux = 3.f;       // low orange sun
		V.MoonColor = FLinearColor(1.f, 0.55f, 0.3f);
		V.SkyLightIntensity = 0.6f;
		V.FogDensity = 0.02f;
		V.FogFalloff = 0.2f;
		V.FogColor = FLinearColor(0.12f, 0.08f, 0.07f);
		V.Saturation = 0.85f;
		V.Tint = FLinearColor(1.05f, 0.98f, 0.92f);
		break;

	case EFBMood::OvercastDay:
		V.MoonLux = 8.f;
		V.MoonColor = FLinearColor(0.9f, 0.93f, 1.f);
		V.SkyLightIntensity = 1.f;
		V.FogDensity = 0.02f;
		V.FogFalloff = 0.15f;
		V.FogColor = FLinearColor(0.35f, 0.37f, 0.4f);
		V.Saturation = 0.75f;
		V.Tint = FLinearColor(0.97f, 1.f, 1.02f);
		break;
	}
	return V;
}

void AFBAtmosphere::Apply()
{
	Current = GetMoodValues();

	// ---- Where the sun is. Sunrise ~6:00, noon high in the south, sunset ~18:30.
	const float Hours = FMath::Fmod(FMath::Max(0.f, TimeOfDay), 24.f);
	float SunElevation = -40.f; // without the cycle: the mood decides (night moods = sun far below)
	if (bDayNightCycle)
	{
		SunElevation = 58.f * FMath::Sin((Hours - 6.25f) / 12.5f * PI);
	}
	else if (Mood == EFBMood::OvercastDay)
	{
		SunElevation = 35.f;
	}
	else if (Mood == EFBMood::Dusk)
	{
		SunElevation = 3.f;
	}
	const float SunYaw = MoonDirection + 180.f + (Hours - 12.f) * 15.f;

	// 0 = full day, 1 = night. Twilight between the sun 10 degrees up and 7 degrees down.
	Darkness = 1.f - FMath::SmoothStep(-7.f, 10.f, SunElevation);
	const float Day = 1.f - Darkness;

	// ---- Sun: warm and soft near the horizon, white and strong at midday.
	const float SunUp = FMath::SmoothStep(-2.f, 25.f, SunElevation);
	Sun->SetWorldRotation(FRotator(-FMath::Max(SunElevation, -10.f), SunYaw, 0.f));
	// Overcast Midwest daylight (the look of Supernatural's first seasons): the sun is behind a grey sky,
	// weak, cool and soft-shadowed - never a bright postcard day.
	constexpr float OvercastSun = 0.28f;
	Sun->SetIntensity(SunElevation > -4.f ? SunLux * OvercastSun * FMath::Square(SunUp) + 400.f * FMath::SmoothStep(-4.f, 2.f, SunElevation) : 0.f);
	Sun->SetLightColor(FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.55f, 0.3f), FLinearColor(0.9f, 0.94f, 1.f), FMath::SmoothStep(0.f, 20.f, SunElevation)));
	Sun->SetLightSourceAngle(FMath::Lerp(0.53f, 6.f, FMath::SmoothStep(0.f, 15.f, SunElevation))); // soft overcast shadows

	// Grey, hazy sky instead of a clear blue one: less blue scattering, a lot of haze.
	Sky->SetRayleighScatteringScale(FMath::Lerp(0.0331f, 0.02f, FMath::SmoothStep(-5.f, 15.f, SunElevation)));
	Sky->SetMieScatteringScale(FMath::Lerp(0.004f, 0.075f, FMath::SmoothStep(-5.f, 15.f, SunElevation)));
	Sky->SetMieAnisotropy(0.6f);
	Sun->SetVisibility(SunElevation > -6.f);

	// ---- Moon: the night light (its own direction), fades out in daylight.
	CurrentMoonLux = Current.MoonLux * MoonBrightness * Darkness;
	Moon->SetWorldRotation(FRotator(-MoonElevation, MoonDirection, 0.f));
	Moon->SetIntensity(CurrentMoonLux);
	Moon->SetLightColor(Current.MoonColor);
	Moon->SetVisibility(Darkness > 0.02f);

	CurrentSkyLight = FMath::Lerp(Current.SkyLightIntensity * MoonBrightness, 0.8f, Day);
	SkyLight->SetIntensity(CurrentSkyLight);

	// ---- Fog: thick and dark at night, light haze by day (the mood's fog thins out with the sun).
	const float DayFog = Mood == EFBMood::FoggyNight ? 0.011f : 0.008f; // grey overcast haze all day
	Fog->SetFogDensity(FMath::Lerp(Current.FogDensity, DayFog, Day) * FogAmount);
	Fog->SetFogHeightFalloff(FMath::Lerp(Current.FogFalloff, 0.1f, Day));
	Fog->SetFogInscatteringColor(FMath::Lerp(Current.FogColor, FLinearColor(0.48f, 0.51f, 0.52f), Day));
	// A second, very tall fog layer by day: the whole sky dome turns into a flat grey-white overcast.
	Fog->SecondFogData.FogDensity = 0.0045f * Day * FogAmount;
	Fog->SecondFogData.FogHeightFalloff = 0.004f;
	Fog->SecondFogData.FogHeightOffset = 1500.f;
	Fog->MarkRenderStateDirty();
	Fog->SetVolumetricFog(bVolumetricFog && FogAmount > 0.f);
	Fog->SetVolumetricFogScatteringDistribution(0.6f); // light scatters forward -> beams
	Fog->SetVolumetricFogExtinctionScale(FMath::Lerp(1.5f, 0.6f, Day));
	Fog->SetVolumetricFogDistance(FMath::Lerp(4000.f, 9000.f, Day));

	FPostProcessSettings& PP = Grading->Settings;
	// Grade: washed-out colour, hard contrast, cold green-teal shadows, a faint dirty-amber in the highlights,
	// heavy vignette and film grain (Supernatural seasons 1-2). Night keeps its own cold moonlit grade.
	const float Saturation = FMath::Lerp(Current.Saturation, 0.55f, Day);
	const FLinearColor Tint = FMath::Lerp(Current.Tint, FLinearColor(0.95f, 1.f, 0.96f), Day);
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.f);
	PP.bOverride_ColorContrast = true;
	const float Contrast = FMath::Lerp(1.1f, 1.22f, Day);
	PP.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.f);
	PP.bOverride_ColorGain = true;
	PP.ColorGain = FVector4(Tint.R, Tint.G, Tint.B, 1.f);
	PP.bOverride_ColorGammaShadows = true;
	PP.ColorGammaShadows = FVector4(FMath::Lerp(1.f, 0.93f, Day), FMath::Lerp(1.f, 1.0f, Day), FMath::Lerp(1.f, 1.03f, Day), FMath::Lerp(1.f, 0.92f, Day));
	PP.bOverride_ColorSaturationShadows = true;
	PP.ColorSaturationShadows = FVector4(1.f, 1.f, 1.f, FMath::Lerp(1.f, 0.8f, Day));
	PP.bOverride_ColorGainHighlights = true;
	PP.ColorGainHighlights = FVector4(FMath::Lerp(1.f, 1.03f, Day), FMath::Lerp(1.f, 1.01f, Day), FMath::Lerp(1.f, 0.9f, Day), 1.f);
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.6f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.3f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = FMath::Lerp(0.6f, 0.25f, Day);

	// Highest-quality ray-traced lighting: Lumen GI and reflections at full quality, hit-lit (real) reflections
	// in windows, puddles, cars and the lake, ray-traced reflections on glass and water.
	PP.bOverride_LumenFinalGatherQuality = true;
	PP.LumenFinalGatherQuality = 2.f;
	PP.bOverride_LumenSceneLightingQuality = true;
	PP.LumenSceneLightingQuality = 2.f;
	PP.bOverride_LumenSceneDetail = true;
	PP.LumenSceneDetail = 2.f;
	PP.bOverride_LumenReflectionQuality = true;
	PP.LumenReflectionQuality = 2.f;
	PP.bOverride_LumenRayLightingMode = true;
	PP.LumenRayLightingMode = ELumenRayLightingModeOverride::HitLighting;
	PP.bOverride_LumenFrontLayerTranslucencyReflections = true;
	PP.LumenFrontLayerTranslucencyReflections = true;
	PP.bOverride_LumenMaxTraceDistance = true;
	PP.LumenMaxTraceDistance = 40000.f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.35f;
}

void AFBAtmosphere::SetTimeOfDay(float NewHours)
{
	TimeOfDay = FMath::Fmod(NewHours + 24.f, 24.f);
	Apply();
}

float AFBAtmosphere::DarknessIn(const UWorld* World)
{
	static TWeakObjectPtr<AFBAtmosphere> Cached;
	if (!World)
	{
		return 1.f;
	}
	if (!Cached.IsValid() || Cached->GetWorld() != World)
	{
		Cached = nullptr;
		for (TActorIterator<AFBAtmosphere> It(const_cast<UWorld*>(World)); It; ++It)
		{
			Cached = *It;
			break;
		}
	}
	return Cached.IsValid() ? Cached->GetDarkness() : 1.f;
}

void AFBAtmosphere::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Apply(); // Live preview in the editor.
}

void AFBAtmosphere::BeginPlay()
{
	Super::BeginPlay();
	// Play at Epic quality (shadows, GI, reflections, foliage, textures, effects).
	if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		if (Settings->GetOverallScalabilityLevel() < 3)
		{
			Settings->SetOverallScalabilityLevel(3);
			Settings->ApplySettings(false);
		}
	}
	Apply();
	NextLightningTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(5.f, 12.f);
}

void AFBAtmosphere::SetMood(EFBMood NewMood)
{
	Mood = NewMood;
	Apply();
}

void AFBAtmosphere::TriggerLightning()
{
	LightningStartTime = GetWorld()->GetTimeSeconds();
	OnLightning.Broadcast();
}

void AFBAtmosphere::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The clock: hours pass, the sun moves (lights updated 4x a second - smooth enough, cheap).
	if (bDayNightCycle && DayLengthMinutes > 0.f)
	{
		const float Speed = (Darkness > 0.5f) ? NightSpeed : 1.f;
		TimeOfDay = FMath::Fmod(TimeOfDay + DeltaSeconds * Speed * 24.f / (DayLengthMinutes * 60.f), 24.f);
		ApplyTimer -= DeltaSeconds;
		if (ApplyTimer <= 0.f)
		{
			ApplyTimer = 0.25f;
			Apply();
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if ((Current.bLightning || bForceLightning) && Darkness > 0.5f && Now >= NextLightningTime)
	{
		TriggerLightning();
		NextLightningTime = Now + FMath::FRandRange(LightningMinInterval, FMath::Max(LightningMinInterval, LightningMaxInterval));
	}

	// Real lightning flickers: 2-3 bright pulses in under a second.
	const float T = Now - LightningStartTime;
	float Flash = 0.f;
	if (T >= 0.f && T < 0.9f)
	{
		auto Pulse = [T](float Start, float Length, float Strength)
		{
			const float X = (T - Start) / Length;
			return (X >= 0.f && X <= 1.f) ? Strength * (1.f - X) : 0.f;
		};
		Flash = FMath::Max3(Pulse(0.f, 0.12f, 1.f), Pulse(0.18f, 0.1f, 0.6f), Pulse(0.35f, 0.45f, 0.9f));
	}

	// Only touch the lights while flashing (and once more to reset).
	if (Flash <= 0.f && !bWasFlashing)
	{
		return;
	}
	bWasFlashing = Flash > 0.f;

	Moon->SetIntensity(CurrentMoonLux + Flash * 6.f);
	Moon->SetLightColor(Flash > 0.f ? FLinearColor::LerpUsingHSV(Current.MoonColor, FLinearColor(0.85f, 0.9f, 1.f), FMath::Min(1.f, Flash)) : Current.MoonColor);
	SkyLight->SetIntensity(CurrentSkyLight + Flash * 1.5f);
}
