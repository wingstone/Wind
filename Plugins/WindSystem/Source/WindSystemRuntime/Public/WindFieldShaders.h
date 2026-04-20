// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphDefinitions.h"

// ============================================================================
// Pass 1: Advection — Semi-Lagrangian backtrace
// ============================================================================

class FWindFieldAdvectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindFieldAdvectionCS);
	SHADER_USE_PARAMETER_STRUCT(FWindFieldAdvectionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, FieldOrigin)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER(FVector3f, FieldExtent)
		SHADER_PARAMETER(float, Dissipation)
		SHADER_PARAMETER(uint32, ResolutionX)
		SHADER_PARAMETER(uint32, ResolutionY)
		SHADER_PARAMETER(uint32, ResolutionZ)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(FVector3f, PrevFieldOrigin)
		SHADER_PARAMETER(float, Padding0)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, PrevField)
		SHADER_PARAMETER_SAMPLER(SamplerState, PrevFieldSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutputField)
		// Directional wind (additive advection velocity)
		SHADER_PARAMETER(FVector3f, DirectionalWindDirection)
		SHADER_PARAMETER(float, DirectionalWindStrength)
		SHADER_PARAMETER(float, DirectionalNoiseStrength)
		SHADER_PARAMETER(uint32, bHasDirectionalWind)
		SHADER_PARAMETER(float, Padding1)
		SHADER_PARAMETER_TEXTURE(Texture2D, DirectionalNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DirectionalNoiseSampler)
		SHADER_PARAMETER(float, DirNoiseTiling)
		SHADER_PARAMETER(float, DirNoiseIntensityMin)
		SHADER_PARAMETER(float, DirNoiseIntensityMax)
		SHADER_PARAMETER(float, DirNoiseScrollSpeed)
		SHADER_PARAMETER(uint32, bHasDirectionalNoise)
		SHADER_PARAMETER(float, Padding2)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 8);
	}
};

// ============================================================================
// Pass 2: External Force Injection — wind sources + noise texture
// ============================================================================

class FWindFieldForceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindFieldForceCS);
	SHADER_USE_PARAMETER_STRUCT(FWindFieldForceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, FieldOrigin)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER(FVector3f, FieldExtent)
		SHADER_PARAMETER(uint32, SourceCount)
		SHADER_PARAMETER(uint32, ResolutionX)
		SHADER_PARAMETER(uint32, ResolutionY)
		SHADER_PARAMETER(uint32, ResolutionZ)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, Dissipation)
		SHADER_PARAMETER(float, Padding0)
		SHADER_PARAMETER(float, Padding1)
		SHADER_PARAMETER(float, Padding2)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUWindSource>, WindSources)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, AdvectedField)
		SHADER_PARAMETER_SAMPLER(SamplerState, AdvectedFieldSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutputField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 8);
	}
};

// ============================================================================
// Pass 3: Diffusion — Jacobi Laplacian smoothing
// ============================================================================

class FWindFieldDiffusionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindFieldDiffusionCS);
	SHADER_USE_PARAMETER_STRUCT(FWindFieldDiffusionCS, FGlobalShader);

	/** Diffusion method: 0 = Explicit finite difference, 1 = Jacobi iteration */
	class FDiffusionMethod : SHADER_PERMUTATION_INT("DIFFUSION_METHOD", 2);
	using FPermutationDomain = TShaderPermutationDomain<FDiffusionMethod>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, FieldOrigin)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(FVector3f, FieldExtent)
		SHADER_PARAMETER(float, Viscosity)
		SHADER_PARAMETER(uint32, ResolutionX)
		SHADER_PARAMETER(uint32, ResolutionY)
		SHADER_PARAMETER(uint32, ResolutionZ)
		SHADER_PARAMETER(float, Padding0)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, InputField)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputFieldSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutputField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 8);
	}
};

// ============================================================================
// Pass 4: Composition — add directional wind to final output
// ============================================================================

class FWindFieldComposeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindFieldComposeCS);
	SHADER_USE_PARAMETER_STRUCT(FWindFieldComposeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, FieldOrigin)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER(FVector3f, FieldExtent)
		SHADER_PARAMETER(float, Padding0)
		SHADER_PARAMETER(uint32, ResolutionX)
		SHADER_PARAMETER(uint32, ResolutionY)
		SHADER_PARAMETER(uint32, ResolutionZ)
		SHADER_PARAMETER(float, Padding1)
		SHADER_PARAMETER(FVector3f, DirectionalWindDirection)
		SHADER_PARAMETER(float, DirectionalWindStrength)
		SHADER_PARAMETER(float, DirectionalNoiseStrength)
		SHADER_PARAMETER(uint32, bHasDirectionalNoise)
		SHADER_PARAMETER(float, DirNoiseTiling)
		SHADER_PARAMETER(float, DirNoiseIntensityMin)
		SHADER_PARAMETER(float, DirNoiseIntensityMax)
		SHADER_PARAMETER(float, DirNoiseScrollSpeed)
		SHADER_PARAMETER(float, Padding2)
		SHADER_PARAMETER_TEXTURE(Texture2D, DirectionalNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DirectionalNoiseSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, SimulationField)
		SHADER_PARAMETER_SAMPLER(SamplerState, SimulationFieldSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutputField)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 8);
	}
};

// ============================================================================
// Debug: 3D Wind Field → 2D Slice Atlas
// ============================================================================

class FWindFieldDebugSliceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindFieldDebugSliceCS);
	SHADER_USE_PARAMETER_STRUCT(FWindFieldDebugSliceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VolumeResX)
		SHADER_PARAMETER(uint32, VolumeResY)
		SHADER_PARAMETER(uint32, VolumeResZ)
		SHADER_PARAMETER(uint32, SliceCols)
		SHADER_PARAMETER(uint32, Padding0)
		SHADER_PARAMETER(uint32, Padding1)
		SHADER_PARAMETER(uint32, Padding2)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, WindFieldVolume)
		SHADER_PARAMETER_SAMPLER(SamplerState, WindFieldVolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputAtlas)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), 8);
	}
};

// ============================================================================
// Scroll: shift 3D wind field volume by integer texel offset
// ============================================================================

class FWindFieldScrollCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindFieldScrollCS);
	SHADER_USE_PARAMETER_STRUCT(FWindFieldScrollCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, ResolutionX)
		SHADER_PARAMETER(uint32, ResolutionY)
		SHADER_PARAMETER(uint32, ResolutionZ)
		SHADER_PARAMETER(int32, ScrollOffsetX)
		SHADER_PARAMETER(int32, ScrollOffsetY)
		SHADER_PARAMETER(int32, ScrollOffsetZ)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, SourceVolume)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, DestVolume)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 8);
	}
};
