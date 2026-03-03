// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WaterFluidTypes.h"

class FRHICommandListImmediate;

/**
 * Render thread proxy for water fluid simulation
 * Manages GPU resources and compute shader dispatch
 */
class WATERSYSTEMRENDERER_API FWaterFieldProxy
{
public:
	FWaterFieldProxy();
	~FWaterFieldProxy();

	/** Initialize GPU resources */
	void Initialize(const FWaterFluidConfig& Config);

	/** Release GPU resources */
	void Release();

	/** Update simulation on GPU */
	void Update(FRHICommandListImmediate& RHICmdList, float DeltaTime);

	/** Apply disturbances (splashes) */
	void ApplyDisturbances(FRHICommandListImmediate& RHICmdList, const TArray<FGPUWaterDisturbance>& Disturbances);

	/** Apply continuous interactions */
	void ApplyInteractions(FRHICommandListImmediate& RHICmdList, const TArray<FWaterInteractionData>& Interactions);

	/** Readback height field to CPU (for debugging) */
	void ReadbackHeightField(TArray<float>& OutHeightField);

	/** Get configuration */
	const FWaterFluidConfig& GetConfig() const { return Config; }

	/** Check if initialized */
	bool IsInitialized() const { return bIsInitialized; }

private:
	FWaterFluidConfig Config;
	bool bIsInitialized;

	// GPU buffers
	FBufferRHIRef HeightFieldBuffer;
	FUnorderedAccessViewRHIRef HeightFieldUAV;
	FShaderResourceViewRHIRef HeightFieldSRV;

	FBufferRHIRef VelocityFieldBuffer;
	FUnorderedAccessViewRHIRef VelocityFieldUAV;
	FShaderResourceViewRHIRef VelocityFieldSRV;

	FBufferRHIRef TempHeightBuffer;
	FUnorderedAccessViewRHIRef TempHeightUAV;
	FShaderResourceViewRHIRef TempHeightSRV;

	FBufferRHIRef TempVelocityBuffer;
	FUnorderedAccessViewRHIRef TempVelocityUAV;
	FShaderResourceViewRHIRef TempVelocitySRV;

	// Navier-Stokes specific buffers
	FBufferRHIRef PressureFieldBuffer;
	FUnorderedAccessViewRHIRef PressureFieldUAV;
	FShaderResourceViewRHIRef PressureFieldSRV;

	FBufferRHIRef DivergenceFieldBuffer;
	FUnorderedAccessViewRHIRef DivergenceFieldUAV;
	FShaderResourceViewRHIRef DivergenceFieldSRV;

	void CreateBuffers();
	void ReleaseBuffers();
	void SolveShallowWater(FRHICommandListImmediate& RHICmdList, float DeltaTime);
	void SolveNavierStokes(FRHICommandListImmediate& RHICmdList, float DeltaTime);
};
