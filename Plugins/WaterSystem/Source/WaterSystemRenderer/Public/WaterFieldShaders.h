// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"

/**
 * Shallow water equation solver compute shader
 * Solves 2D height field and velocity field using finite difference
 */
class FShallowWaterSolverCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShallowWaterSolverCS);
	SHADER_USE_PARAMETER_STRUCT(FShallowWaterSolverCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(float, Gravity)
		SHADER_PARAMETER(float, Viscosity)
		SHADER_PARAMETER(float, Damping)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, HeightField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, VelocityField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutHeightField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, OutVelocityField)
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
		SHADER_PARAMETER(int32, GridSize)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(float, InteractionStrength)
		SHADER_PARAMETER(float, InteractionRadius)
		SHADER_PARAMETER(FVector2f, InteractionPosition)
		SHADER_PARAMETER(FVector2f, InteractionForce)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, VelocityField)
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
// Navier-Stokes Solver Shaders (5-step projection method)
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
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, VelocityField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, TempVelocityField)
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
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, VelocityField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, TempVelocityField)
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
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, VelocityField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, DivergenceField)
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
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, PressureField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, DivergenceField)
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
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, VelocityField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, PressureField)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float2>, OutVelocityField)
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
