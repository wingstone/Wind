// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "WeatherSystemTypes.h"
#include "WeatherDirector.generated.h"

class UWeatherSystemConfig;
class UWeatherSubsystem;
class AWeatherActor;
class ASeasonActor;

/**
 * A single "conductor" actor placed in the level. It owns the initial config, drives
 * TimeOfDay / SeasonProgress, and offers designer-friendly knobs (auto-advance time,
 * auto-request weather every N seconds, initial weather / season, initial addons).
 *
 * Not required — external code can drive UWeatherSubsystem directly — but a level with
 * one of these placed is playable out of the box.
 */
UCLASS(BlueprintType, Blueprintable)
class WEATHERSYSTEM_API AWeatherDirector : public AActor
{
	GENERATED_BODY()

public:
	AWeatherDirector();

	//~ AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	/** Tick even when the editor isn't playing, so InitialXXX / auto-advance drive the preview. */
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual void PostRegisterAllComponents() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ------------------------------------------------------------------ Config

	/** Plugin config asset — points at transition table + weather / season / addon lists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Setup")
	TObjectPtr<UWeatherSystemConfig> Config;

	// Weather / Season host actors self-register with the subsystem on PostRegisterAllComponents.
	// Place an AWeatherActor and an ASeasonActor anywhere in the level — no wiring needed here.

	// ------------------------------------------------------------------ Initial state

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Initial", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float InitialTimeOfDay = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Initial", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float InitialSeasonProgress = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Initial")
	EWeatherSeason InitialSeason = EWeatherSeason::Spring;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Initial", meta = (Categories = "Weather"))
	FGameplayTag InitialWeather;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Initial", meta = (Categories = "Addon"))
	TArray<FGameplayTag> InitiallyEnabledAddons;

	// ------------------------------------------------------------------ Auto-advance

	/** If true, TimeOfDay advances every frame while the director ticks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Auto")
	bool bAutoAdvanceTime = false;

	/** Real seconds per in-game hour. 60 => a full 24h cycle takes 24 minutes real time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Auto", meta = (ClampMin = "0.01"))
	float RealSecondsPerHour = 60.0f;

	/** If true, calls RequestNextWeather every AutoWeatherInterval seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Auto")
	bool bAutoRequestNextWeather = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Auto", meta = (ClampMin = "0.0"))
	float AutoWeatherInterval = 120.0f;

	/**
	 * If true, TickActor pulls the local-player camera position and pushes it into the
	 * subsystem as the query location for local weather volumes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Auto")
	bool bDriveQueryLocationFromCamera = true;

	// ------------------------------------------------------------------ BP API (delegating)

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetTimeOfDay(float NewTOD);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void AddHours(float Hours);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetSeasonProgress(float Phase);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetWeather(FGameplayTag WeatherId, float BlendSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetSeason(EWeatherSeason Season, float BlendSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void RequestNextWeather(float BlendSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void EnableAddon(FGameplayTag AddonId, bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetTimeOfDay() const;

	UFUNCTION(BlueprintPure, Category = "Weather")
	FGameplayTag GetCurrentWeather() const;

	UFUNCTION(BlueprintPure, Category = "Weather")
	EWeatherSeason GetCurrentSeason() const;

	UFUNCTION(BlueprintPure, Category = "Weather")
	UWeatherSubsystem* GetSubsystem() const;

private:
	float AutoWeatherTimer = 0.0f;
};
