// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherActor.generated.h"

class UStaticMeshComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UNiagaraComponent;

/**
 * Shared host actor for all weathers. The subsystem's applier writes into components
 * on this actor (mesh MIDs / Niagara / actor properties / functions). Place ONE in the
 * level — it auto-registers with UWeatherSubsystem.
 *
 * Carries the standard sky / atmosphere / lighting rig so a level with just this actor
 * plus a WeatherDirector is playable. Bindings on UWeatherDataAsset can drive any of
 * these components by ComponentTag; the tags match each component's name below.
 *
 * Component tags (usable as FWeatherParamBinding::ComponentTag):
 *   "SkyMesh"      -> UStaticMeshComponent
 *   "SkyAtmosphere"-> USkyAtmosphereComponent
 *   "HeightFog"    -> UExponentialHeightFogComponent
 *   "VolumetricCloud"-> UVolumetricCloudComponent
 *   "SunLight"     -> UDirectionalLightComponent
 *   "MoonLight"    -> UDirectionalLightComponent
 *   "SkyLight"     -> USkyLightComponent
 *   "RainFX"       -> UNiagaraComponent
 *   "SnowFX"       -> UNiagaraComponent
 */
UCLASS(BlueprintType, Blueprintable, HideCategories = ("Rendering", "Collision", "Physics", "Networking", "Cooking", "Actor", "Input", "HLOD", "Replication", "Default", "Weather"))
class WEATHERSYSTEM_API AWeatherActor : public AActor
{
	GENERATED_BODY()

public:
	AWeatherActor();

	//~ AActor
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;

#if WITH_EDITOR
	/**
	 * On save, walk this actor's components and replace any Material Instance Dynamic
	 * (MID) with the MID's parent MaterialInterface. Covers:
	 *   - Every material slot on every MeshComponent (sky mesh + any user-added meshes).
	 *   - LightFunctionMaterial on every LightComponent (sun / moon / etc).
	 *   - The material on every VolumetricCloudComponent.
	 * MIDs are transient runtime objects; letting them stick to component slots at save
	 * time bloats the map package and leaves stale bindings. This normalizes saved state
	 * back to the authored parent material.
	 */
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif

	/**
	 * Fired every time the subsystem retargets this actor at a new weather. Gives content
	 * a hook to (e.g.) swap a mesh's base material before the applier resolves against it.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weather")
	void OnWeatherAssetBound(const class UWeatherDataAsset* NewAsset);

	// ------------------------------------------------------------------ Celestial motion

	/**
	 * Drive sun / moon rotation from a time-of-day in [0,24). Called by the subsystem
	 * every tick when bAutoUpdateCelestials is true; can also be called manually or
	 * overridden in a Blueprint subclass.
	 *
	 * TOD convention:
	 *   6  = sunrise (sun at east horizon)
	 *   12 = noon    (sun overhead)
	 *   18 = sunset  (sun at west horizon)
	 *   0/24 = midnight (sun opposite side of earth; moon overhead if MoonHourOffset=12)
	 */
	UFUNCTION(BlueprintCallable, Category = "Celestial")
	virtual void UpdateCelestialBodies(float TimeOfDay);

	/** If true, subsystem drives UpdateCelestialBodies each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial")
	bool bAutoUpdateCelestials = true;

	/** Heading of "north" in world yaw degrees (compass). 0 = +X world axis, 90 = +Y, etc.
	 *  Sunrise is 90° clockwise from north (east). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float CompassYawDegrees = 0.0f;

	/** Observer latitude in degrees ([-90, 90]).
	 *  Controls how much the sun's daily arc tilts away from the zenith:
	 *  the sun's peak elevation at local noon is (90 - |Latitude|) degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float LatitudeDegrees = 12.0f;

	/** Moon leads/lags the sun by this many hours. 12 = opposite; 6 = quarter phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float MoonHourOffset = 12.0f;

	/** Moon peak elevation offset from sun's — a small tilt so it doesn't retrace the same arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float MoonTiltDegrees = 5.0f;

	/**
	 * Cross-fade sun ↔ moon intensity around the horizon.
	 * SunLight intensity is scaled by saturate((sun_pitch_down + Twilight) / (2*Twilight)),
	 * MoonLight uses the same curve mirrored.
	 * Twilight in degrees defines the horizon soft-fade band.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float TwilightDegrees = 6.0f;

	/** Baseline intensity for the sun light (multiplied by the horizon curve). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial", meta = (ClampMin = "0.0"))
	float SunPeakIntensity = 10.0f;

	/** Baseline intensity for the moon light (multiplied by the horizon curve). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial", meta = (ClampMin = "0.0"))
	float MoonPeakIntensity = 0.4f;

	// ------------------------------------------------------------------ Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** Sky dome / sphere mesh. Tag: "SkyMesh". Content sets the material; MID param bindings drive tints etc. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> SkyMesh;

	/** Physically-based sky atmosphere. Tag: "SkyAtmosphere". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	/** Exponential height fog. Tag: "HeightFog". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	/** Volumetric cloud layer. Tag: "VolumetricCloud". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVolumetricCloudComponent> VolumetricCloud;

	/** Primary directional light — the sun. Tag: "SunLight". Marked as the atmosphere sun light. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDirectionalLightComponent> SunLight;

	/** Secondary directional light — the moon (atmosphere light index 1). Tag: "MoonLight". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	/** Sky light for ambient / reflection. Tag: "SkyLight". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkyLightComponent> SkyLight;

	/** Niagara particle system for rain. Tag: "RainFX". Content assigns the system asset. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> RainFX;

	/** Niagara particle system for snow. Tag: "SnowFX". Content assigns the system asset. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> SnowFX;
};
