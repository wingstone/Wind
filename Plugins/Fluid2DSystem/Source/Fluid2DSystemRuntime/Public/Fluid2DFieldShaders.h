// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"

/**
 * SW Step 1: Advection (Semi-Lagrangian)
 */
class FSWHeightAdvectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSWHeightAdvectionCS);
	SHADER_USE_PARAMETER_STRUCT(FSWHeightAdvectionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(FVector2f, GridOrigin)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, AdvectionDamping)
		SHADER_PARAMETER(float, FlowNoiseIntensityMin)
		SHADER_PARAMETER(float, FlowNoiseIntensityMax)
		SHADER_PARAMETER(float, FlowNoiseTiling)
		SHADER_PARAMETER(FVector2f, FlowDirection)
		SHADER_PARAMETER(FVector4f, UVScaleOffset)
		SHADER_PARAMETER_SAMPLER(SamplerState, FlowNoiseSampler)
		SHADER_PARAMETER_TEXTURE(Texture2D, FlowNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CurrentVelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CurrentHeightField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, NextHeightField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

class FSWVelocityAdvectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSWVelocityAdvectionCS);
	SHADER_USE_PARAMETER_STRUCT(FSWVelocityAdvectionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(FVector2f, GridOrigin)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, AdvectionDamping)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CurrentVelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, NextVelocityField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * SW Step 2: Diffusion
 */
class FSWDiffusionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSWDiffusionCS);
	SHADER_USE_PARAMETER_STRUCT(FSWDiffusionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(FVector2f, GridOrigin)
		SHADER_PARAMETER(float, Alpha)
		SHADER_PARAMETER(float, Beta)
		SHADER_PARAMETER(float, DiffusionDamping)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, PrevHeightField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CurrentHeightField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, NextHeightField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * SW Step 4: Height to normal conversion
 */
class FFluid2DHeightToNormalCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluid2DHeightToNormalCS);
	SHADER_USE_PARAMETER_STRUCT(FFluid2DHeightToNormalCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, HeightField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutNormalField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * Scroll height field texture with integer texel offset
 */
class FFluid2DScrollHeightCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluid2DScrollHeightCS);
	SHADER_USE_PARAMETER_STRUCT(FFluid2DScrollHeightCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(int32, ScrollTexelOffsetX)
		SHADER_PARAMETER(int32, ScrollTexelOffsetY)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SourceTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DestHeightTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * Scroll velocity field texture with integer texel offset
 */
class FFluid2DScrollVelocityCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluid2DScrollVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FFluid2DScrollVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(int32, ScrollTexelOffsetX)
		SHADER_PARAMETER(int32, ScrollTexelOffsetY)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SourceTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DestVelocityTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * Apply player/object interaction forces compute shader
 */
class FFluid2DInteractionApplicationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluid2DInteractionApplicationCS);
	SHADER_USE_PARAMETER_STRUCT(FFluid2DInteractionApplicationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(FVector2f, GridOrigin)
		SHADER_PARAMETER(float, InteractionDirectionalStrength)
		SHADER_PARAMETER(float, InteractionOmniStrength)
		SHADER_PARAMETER(float, InteractionVortexStrength)
		SHADER_PARAMETER(float, InteractionRadius)
		SHADER_PARAMETER(float, InteractionRadiusWidth)
		SHADER_PARAMETER(float, InteractionPowerFalloff)
		SHADER_PARAMETER(float, InteractionHeightIntensity)
		SHADER_PARAMETER(uint32, InteractionShapeType)
		SHADER_PARAMETER(uint32, InteractionEmissionTypeMask)
		SHADER_PARAMETER(FVector2f, InteractionPosition)
		SHADER_PARAMETER(FVector2f, InteractionPrevPosition)
		SHADER_PARAMETER(FVector2f, InteractionDirection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HeightField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, VelocityField)
	END_SHADER_PARAMETER_STRUCT()
	
	class FApplyVelocity : SHADER_PERMUTATION_BOOL("APPLY_VELOCITY");

	using FPermutationDomain = TShaderPermutationDomain<FApplyVelocity>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

//=============================================================================
// Navier-Stokes Solver Shaders (5-step projection method, texture-based)
//=============================================================================

/**
 * NS Step 1: Advection (Semi-Lagrangian)
 */
class FNSAdvectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNSAdvectionCS);
	SHADER_USE_PARAMETER_STRUCT(FNSAdvectionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(float, Damping)
		SHADER_PARAMETER(float, FlowNoiseIntensityMin)
		SHADER_PARAMETER(float, FlowNoiseIntensityMax)
		SHADER_PARAMETER(float, FlowNoiseTiling)
		SHADER_PARAMETER(FVector2f, FlowDirection)
		SHADER_PARAMETER(FVector4f, UVScaleOffset)
		SHADER_PARAMETER_SAMPLER(SamplerState, FlowNoiseSampler)
		SHADER_PARAMETER_TEXTURE(Texture2D, FlowNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, DensityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutVelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutDensityField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * NS Step 2: Diffusion (Viscosity)
 */
class FNSDiffusionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNSDiffusionCS);
	SHADER_USE_PARAMETER_STRUCT(FNSDiffusionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(float, Viscosity)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, OriginalVelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutVelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, DensityField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, OriginalDensityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutDensityField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * NS Step 3: Compute Divergence
 */
class FNSComputeDivergenceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNSComputeDivergenceCS);
	SHADER_USE_PARAMETER_STRUCT(FNSComputeDivergenceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutDivergenceField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * NS Step 4: Pressure Solve (Jacobi iteration)
 */
class FNSPressureSolveCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNSPressureSolveCS);
	SHADER_USE_PARAMETER_STRUCT(FNSPressureSolveCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, PressureField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, DivergenceField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutPressureField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * NS Step 5: Projection (Enforce incompressibility)
 */
class FNSProjectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNSProjectionCS);
	SHADER_USE_PARAMETER_STRUCT(FNSProjectionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, PressureField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutVelocityField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * NS Vorticity Confinement Step A: Compute scalar vorticity
 */
class FNSComputeVorticityCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNSComputeVorticityCS);
	SHADER_USE_PARAMETER_STRUCT(FNSComputeVorticityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutVorticityField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * NS Vorticity Confinement Step B: Apply confinement force
 */
class FNSVorticityConfinementCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNSVorticityConfinementCS);
	SHADER_USE_PARAMETER_STRUCT(FNSVorticityConfinementCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(float, VorticityEpsilon)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VorticityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutVelocityField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * Apply global flow force from noise texture
 */
class FFluid2DGlobalFlowCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluid2DGlobalFlowCS);
	SHADER_USE_PARAMETER_STRUCT(FFluid2DGlobalFlowCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, FlowNoiseIntensityMin)
		SHADER_PARAMETER(float, FlowNoiseIntensityMax)
		SHADER_PARAMETER(float, FlowNoiseTiling)
		SHADER_PARAMETER(FVector2f, FlowDirection)
		SHADER_PARAMETER_TEXTURE(Texture2D, FlowNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, FlowNoiseSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, VelocityField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

/**
 * Compose final velocity output: simulation velocity + global flow noise
 */
class FFluid2DComposeVelocityCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluid2DComposeVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FFluid2DComposeVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, FlowNoiseIntensityMin)
		SHADER_PARAMETER(float, FlowNoiseIntensityMax)
		SHADER_PARAMETER(float, FlowNoiseTiling)
		SHADER_PARAMETER(FVector2f, FlowDirection)
		SHADER_PARAMETER(FVector4f, UVScaleOffset)
		SHADER_PARAMETER_TEXTURE(Texture2D, FlowNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, FlowNoiseSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SourceVelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutVelocityField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};

class FFluid2DMaskAccumulateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluid2DMaskAccumulateCS);
	SHADER_USE_PARAMETER_STRUCT(FFluid2DMaskAccumulateCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, FadeFactor)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CurrentHeightField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, NextHeightField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), 16);
	}
};