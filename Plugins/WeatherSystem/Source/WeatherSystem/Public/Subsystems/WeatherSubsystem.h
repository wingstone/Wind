// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "WeatherSystemTypes.h"
#include "Reflection/ParameterApplier.h"
#include "WeatherSubsystem.generated.h"

class UWeatherDataAsset;
class USeasonDataAsset;
class UWeatherAddonDataAsset;
class UWeatherSystemConfig;
class UWeatherTransitionController;
class AWeatherActor;
class ASeasonActor;
class AWeatherAddonActor;
class ALocalWeatherVolume;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeatherChanged, FGameplayTag, PrevId, FGameplayTag, NewId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSeasonChanged, EWeatherSeason, PrevSeason, EWeatherSeason, NewSeason);

/**
 * The public API of the plugin. External systems only touch this.
 *
 * Internally maintains:
 *   - registries of weather / season / addon data assets, keyed by GameplayTag
 *   - the currently active weather + season, and the previous ones during a blend
 *   - a set of active addon actors, each with its own applier
 *   - the shared weather/season host actors and their appliers
 *   - a priority-sorted list of registered ALocalWeatherVolume
 *
 * Time and season phase are external state — the subsystem never advances them itself.
 * The subsystem does tick to progress in-flight blends and apply per-frame values.
 */
UCLASS(BlueprintType)
class WEATHERSYSTEM_API UWeatherSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWeatherSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	//~ End FTickableGameObject

	// ==================================================================================
	// Public API
	// ==================================================================================

	/** Point the subsystem at a config asset; loads and registers its weather/season/addon lists. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetConfig(UWeatherSystemConfig* InConfig);

	/**
	 * Bind (or replace) the host actors. Either can be null; the subsystem will spawn a
	 * default one on demand if a data asset carries a HostActorClass.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetWeatherHostActor(AWeatherActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetSeasonHostActor(ASeasonActor* InActor);

	/** Push a new time-of-day (0..24) into the system. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetTimeOfDay(float InTOD);

	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetTimeOfDay() const { return CurrentTOD; }

	/** Change the season phase (0..2). Orthogonal to SetSeason. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetSeasonProgress(float Phase);

	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetSeasonProgress() const { return CurrentSeasonPhase; }

	/**
	 * Switch weather. Negative BlendSeconds -> use config default.
	 * Snaps immediately if BlendSeconds == 0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetWeather(FGameplayTag WeatherId, float BlendSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetSeason(EWeatherSeason Season, float BlendSeconds = -1.0f);

	/** Ask the transition controller for the next weather and switch to it. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void RequestNextWeather(float BlendSeconds = -1.0f);

	/** Turn an addon on / off. Enabling spawns the actor if needed; disabling destroys it. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void EnableAddon(FGameplayTag AddonId, bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void RegisterLocalVolume(ALocalWeatherVolume* Volume);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void UnregisterLocalVolume(ALocalWeatherVolume* Volume);

	/**
	 * Addon actors register themselves in PostRegisterAllComponents. The subsystem does
	 * NOT spawn addon actors — designers place one per addon in the level and set its
	 * AddonId. EnableAddon then toggles that placed actor on / off.
	 *
	 * Registration binds the actor to its UWeatherAddonDataAsset (resolved from the addon
	 * registry by AddonId), caches its applier and applies one-shot constants.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void RegisterAddonActor(AWeatherAddonActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void UnregisterAddonActor(AWeatherAddonActor* Actor);

	// Reactive hooks --------------------------------------------------------------------
	UPROPERTY(BlueprintAssignable, Category = "Weather")
	FOnWeatherChanged OnWeatherChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weather")
	FOnSeasonChanged OnSeasonChanged;

	// Queries ---------------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Weather")
	FGameplayTag GetCurrentWeather() const { return CurrentWeatherId; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	EWeatherSeason GetCurrentSeason() const { return CurrentSeason; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	UWeatherDataAsset* FindWeather(FGameplayTag Id) const;

	UFUNCTION(BlueprintPure, Category = "Weather")
	USeasonDataAsset* FindSeason(EWeatherSeason Season) const;

	UFUNCTION(BlueprintPure, Category = "Weather")
	UWeatherAddonDataAsset* FindAddon(FGameplayTag Id) const;

	/** Sets the world-space position we should use to query local weather volumes. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetQueryLocation(FVector InWorldLocation) { QueryLocation = InWorldLocation; bHasQueryLocation = true; }

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ClearQueryLocation() { bHasQueryLocation = false; }

private:
	// Registry --------------------------------------------------------------------------
	UPROPERTY() TObjectPtr<UWeatherSystemConfig> Config;
	UPROPERTY() TMap<FGameplayTag, TObjectPtr<UWeatherDataAsset>> WeatherRegistry;
	UPROPERTY() TMap<EWeatherSeason, TObjectPtr<USeasonDataAsset>> SeasonRegistry;
	UPROPERTY() TMap<FGameplayTag, TObjectPtr<UWeatherAddonDataAsset>> AddonRegistry;

	// State -----------------------------------------------------------------------------
	UPROPERTY() FGameplayTag CurrentWeatherId;
	UPROPERTY() FGameplayTag PrevWeatherId;
	UPROPERTY() TObjectPtr<UWeatherDataAsset> CurrentWeather;
	UPROPERTY() TObjectPtr<UWeatherDataAsset> PrevWeather;
	float WeatherAlpha = 1.0f;   // 0 = just started, 1 = fully on Current
	float WeatherRate  = 0.0f;   // per-second

	UPROPERTY() EWeatherSeason CurrentSeason = EWeatherSeason::Spring;
	UPROPERTY() EWeatherSeason PrevSeason    = EWeatherSeason::Spring;
	UPROPERTY() TObjectPtr<USeasonDataAsset> CurrentSeasonAsset;
	UPROPERTY() TObjectPtr<USeasonDataAsset> PrevSeasonAsset;
	float SeasonAlpha = 1.0f;
	float SeasonRate  = 0.0f;

	float CurrentTOD          = 12.0f;
	float CurrentSeasonPhase  = 1.0f;

	FVector QueryLocation = FVector::ZeroVector;
	bool    bHasQueryLocation = false;

	// Hosts + appliers ------------------------------------------------------------------
	UPROPERTY() TObjectPtr<AWeatherActor> WeatherHost;
	UPROPERTY() TObjectPtr<ASeasonActor> SeasonHost;

	/** Applier + baked default values from Config->Schema. Written every tick BEFORE the
	 *  current weather's sampled row, so channels the active weather doesn't touch fall
	 *  back to schema defaults instead of leaking from the previous weather. */
	FParameterApplier SchemaDefaultsApplier;
	TArray<FWeatherParamValue> SchemaDefaultsValues;

	FParameterApplier CurrentWeatherApplier;
	FParameterApplier PrevWeatherApplier;
	FParameterApplier CurrentSeasonApplier;
	FParameterApplier PrevSeasonApplier;

	// Addons ----------------------------------------------------------------------------
	struct FActiveAddon
	{
		FGameplayTag Id;
		TWeakObjectPtr<UWeatherAddonDataAsset> Asset;
		TWeakObjectPtr<AWeatherAddonActor> Actor;
		FParameterApplier Applier;
		bool bEnabled = true;   // false while fading out
		float EnvelopeAlpha = 0.0f;   // 0..1, envelope on top of the frame weight
		float EnvelopeRate  = 0.0f;
	};

	/** Every addon actor placed in the level that has registered itself (independent of state). */
	UPROPERTY()
	TArray<TWeakObjectPtr<AWeatherAddonActor>> RegisteredAddonActors;

	/** Active slots, one per bound actor. The slot lives as long as its actor is registered. */
	TArray<FActiveAddon> ActiveAddons;

	/** Enable/disable requests issued before an actor for that AddonId registered; applied on bind. */
	TMap<FGameplayTag, bool> PendingAddonToggles;

	// Local volumes ---------------------------------------------------------------------
	UPROPERTY() TArray<TWeakObjectPtr<ALocalWeatherVolume>> LocalVolumes;

	// Controller ------------------------------------------------------------------------
	UPROPERTY() TObjectPtr<UWeatherTransitionController> Controller;
	FRandomStream Rng;

	// Scratch (avoid per-tick heap alloc) -----------------------------------------------
	TArray<FWeatherParamValue> ScratchA;
	TArray<FWeatherParamValue> ScratchB;
	TArray<FWeatherParamValue> ScratchC;
	TArray<FWeatherParamValue> ScratchLocal;

	// Internals -------------------------------------------------------------------------
	void ResolveWeatherAppliers();
	void ResolveSeasonAppliers();
	/** Rebuild SchemaDefaultsApplier + SchemaDefaultsValues from Config->Schema. */
	void ResolveSchemaDefaults();

	/**
	 * Create (or refresh) the ActiveAddons slot for a placed addon actor, resolving its
	 * asset from AddonRegistry by AddonId. Returns false when the asset isn't registered yet
	 * (config not set) — the actor stays in RegisteredAddonActors and is retried on SetConfig.
	 */
	bool TryBindAddonActor(AWeatherAddonActor* Actor);

	/** Sample weather at TOD into Out; blends PrevWeather/CurrentWeather sample per-channel. */
	void SampleWeather(const UWeatherDataAsset* Asset, float TOD, TArray<FWeatherParamValue>& Out) const;
	void SampleSeason(const USeasonDataAsset* Asset, float Phase, TArray<FWeatherParamValue>& Out) const;
	void SampleAddon(const UWeatherAddonDataAsset* Asset, float TOD, float& OutWeight, TArray<FWeatherParamValue>& Out) const;

	void TickWeather(float Dt);
	void TickSeason(float Dt);
	void TickAddons(float Dt);
	void TickLocalVolumes();

	float ResolveBlendSeconds(float Requested, float Default) const;
};
