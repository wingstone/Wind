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
class FSWAdvectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSWAdvectionCS);
	SHADER_USE_PARAMETER_STRUCT(FSWAdvectionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(FVector2f, GridOrigin)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, AdvectionDamping)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CurrentHeightField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, NextHeightField)
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
		SHADER_PARAMETER(float, Damping)
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
class FWaterHeightToNormalCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWaterHeightToNormalCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterHeightToNormalCS, FGlobalShader);

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
class FWaterScrollHeightCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWaterScrollHeightCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterScrollHeightCS, FGlobalShader);

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
class FWaterScrollVelocityCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWaterScrollVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterScrollVelocityCS, FGlobalShader);

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
class FWaterInteractionApplicationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWaterInteractionApplicationCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterInteractionApplicationCS, FGlobalShader);

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
		SHADER_PARAMETER(FVector2f, InteractionDirection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HeightField)
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
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VelocityField)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, TempVelocityField)
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
		SHADER_PARAMETER(float, Density)
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
		SHADER_PARAMETER(float, Density)
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
