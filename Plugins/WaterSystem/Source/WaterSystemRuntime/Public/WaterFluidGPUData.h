// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WaterFluidTypes.h"

/**
 * GPU-side water fluid data manager
 * Handles 2D fluid simulation data packaging and render thread communication
 */
class WATERSYSTEMRUNTIME_API FWaterFluidGPUData
{
public:
	FWaterFluidGPUData();
	~FWaterFluidGPUData();

	/** Update simulation configuration */
	void SetConfig(const FWaterFluidConfig& InConfig);

	/** Add a water disturbance (splash, wave, etc.) */
	void AddDisturbance(const FVector2D& Position, float Strength, float Radius, float Duration = 1.0f);

	/** Add player/object interaction */
	void AddInteraction(const FWaterInteractionData& Interaction);

	/** Update GPU data (called each frame) */
	void Update(float DeltaTime);

	/** Get current configuration */
	const FWaterFluidConfig& GetConfig() const { return Config; }

	/** Get active disturbances */
	const TArray<FGPUWaterDisturbance>& GetDisturbances() const { return ActiveDisturbances; }

	/** Get pending interactions */
	const TArray<FWaterInteractionData>& GetInteractions() const { return PendingInteractions; }

	/** Clear processed interactions */
	void ClearInteractions();

private:
	FWaterFluidConfig Config;
	TArray<FGPUWaterDisturbance> ActiveDisturbances;
	TArray<FWaterInteractionData> PendingInteractions;
	float SimulationTime;

	void UpdateDisturbances(float DeltaTime);
};

/**
 * Shallow water equations state for GPU simulation
 * Implements 2D fluid dynamics using height field approach
 */
struct FShallowWaterState
{
	/** Water column height */
	float Height;

	/** Velocity in X direction (cm/s) */
	float VelocityX;

	/** Velocity in Y direction (cm/s) */
	float VelocityY;

	/** Terrain/bathymetry height */
	float TerrainHeight;

	FShallowWaterState()
		: Height(0.0f)
		, VelocityX(0.0f)
		, VelocityY(0.0f)
		, TerrainHeight(0.0f)
	{}
};
