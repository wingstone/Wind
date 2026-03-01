// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphDefinitions.h"

/**
 * Compute shader that generates a 3D wind velocity field texture.
 *
 * Reads from a StructuredBuffer of wind sources and writes wind velocity + turbulence
 * into a RWTexture3D. Each thread computes one texel of the 3D volume.
 *
 * Thread group: [8, 8, 8]
 */
class FWindFieldComputeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindFieldComputeCS);
	SHADER_USE_PARAMETER_STRUCT(FWindFieldComputeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Wind field volume parameters
		SHADER_PARAMETER(FVector3f, FieldOrigin)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER(FVector3f, FieldExtent)
		SHADER_PARAMETER(uint32, SourceCount)
		SHADER_PARAMETER(uint32, ResolutionX)
		SHADER_PARAMETER(uint32, ResolutionY)
		SHADER_PARAMETER(uint32, ResolutionZ)
		SHADER_PARAMETER(float, Padding0)
		// Wind source data
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUWindSource>, WindSources)
		// Output 3D texture
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, WindFieldOutput)
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
