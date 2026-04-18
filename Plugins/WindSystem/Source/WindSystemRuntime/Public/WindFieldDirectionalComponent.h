// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WindFieldTypes.h"
#include "WindFieldDirectionalComponent.generated.h"

class UTexture2D;
class UWindSubsystem;

/**
 * Global directional wind component (static wind).
 * Provides a uniform wind direction with optional noise texture modulation.
 * Unlike Point/Vortex sources, directional wind does NOT participate in
 * fluid iteration (advection/diffusion) — it is injected as a constant
 * contribution each frame.
 *
 * Design follows UFluid2DGlobalFlowComponent pattern:
 * standalone USceneComponent with separate subsystem registration.
 */
UCLASS(ClassGroup = (Wind), meta = (BlueprintSpawnableComponent, DisplayName = "Wind Field Directional Source"))
class WINDSYSTEMRUNTIME_API UWindFieldDirectionalComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWindFieldDirectionalComponent();

	// --- Wind parameters ---

	/** Enable global directional wind */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Wind")
	bool bEnableDirectionalWind = true;

	/** Base wind strength (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Wind")
	float Strength = 200.0f;

	/** Curl noise turbulence strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Wind|Turbulence", meta = (ClampMin = "0"))
	float NoiseStrength = 0.2f;

	// --- Noise texture modulation ---

	/** 2D noise texture used to spatially vary the directional wind strength.
	 *  The texture is tiled in the XY plane (horizontal). R channel is sampled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Wind|Noise Texture")
	TObjectPtr<UTexture2D> WindNoiseTexture = nullptr;

	/** Tiling scale for the noise texture in world space (smaller = more repetition) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Wind|Noise Texture", meta = (ClampMin = "0.01", UIMax = "100"))
	float WindNoiseTiling = 1.0f;

	/** Minimum wind intensity multiplier (when noise = 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Wind|Noise Texture", meta = (ClampMin = "0", UIMax = "2"))
	float WindNoiseIntensityMin = 0.2f;

	/** Maximum wind intensity multiplier (when noise = 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Wind|Noise Texture", meta = (ClampMin = "0", UIMax = "2"))
	float WindNoiseIntensityMax = 1.0f;

	/** Scroll speed for the noise texture in the wind direction (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Wind|Noise Texture")
	float WindNoiseScrollSpeed = 50.0f;

	// --- Interface ---

	/** Build a render-thread-safe snapshot of directional wind data */
	FWindDirectionalData GetDirectionalData() const;

	/** Returns true if directional wind is enabled and component is active */
	bool IsWindEnabled() const;

	/** Returns true if a valid noise texture is assigned */
	bool HasNoiseTexture() const;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
	UWindSubsystem* GetWindSubsystem() const;
};
