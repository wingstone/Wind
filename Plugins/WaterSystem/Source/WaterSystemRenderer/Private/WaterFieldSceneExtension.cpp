// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterFieldSceneExtension.h"
//#include "WaterFieldShaders.h"
//#include "WaterFieldProxy.h"
#include "ScenePrivate.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "PixelShaderUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaterFieldSceneExtension, Log, All);

IMPLEMENT_SCENE_EXTENSION(FWaterFieldSceneExtension);

// ============================================================================
// FWaterFieldSceneExtension
// ============================================================================

bool FWaterFieldSceneExtension::ShouldCreateExtension(FScene& Scene)
{
	return Scene.GetWorld() != nullptr;
}

FWaterFieldSceneExtension::FWaterFieldSceneExtension(FScene& InScene)
	: ISceneExtension(InScene)
{
	CachedWorld = InScene.GetWorld();
	
	// Default configuration
	CurrentConfig.SolverType = EFluidSolverType::ShallowWater;
	CurrentConfig.GridSize = 256;
	CurrentConfig.WorldSize = 10000.0f;
	CurrentConfig.Density = 1.0f;
	CurrentConfig.Viscosity = 0.01f;
	CurrentConfig.Damping = 0.02f;
	CurrentConfig.Gravity = 980.0f;
	CurrentConfig.SurfaceTension = 0.05f;
	CurrentConfig.TimeStep = 0.016f;
}

FWaterFieldSceneExtension::~FWaterFieldSceneExtension()
{
	HeightFieldRT.SafeRelease();
	VelocityFieldRT.SafeRelease();
	TempHeightFieldRT.SafeRelease();
	TempVelocityFieldRT.SafeRelease();
	PressureFieldRT.SafeRelease();
	DivergenceFieldRT.SafeRelease();
}

void FWaterFieldSceneExtension::InitExtension(FScene& InScene)
{
	// Initialization if needed
	UE_LOG(LogWaterFieldSceneExtension, Log, TEXT("WaterFieldSceneExtension initialized"));
}

ISceneExtensionUpdater* FWaterFieldSceneExtension::CreateUpdater()
{
	return new FUpdater(*this);
}

ISceneExtensionRenderer* FWaterFieldSceneExtension::CreateRenderer(
	FSceneRendererBase& InSceneRenderer,
	const FEngineShowFlags& EngineShowFlags)
{
	return new FRenderer(InSceneRenderer, *this);
}

// ============================================================================
// FUpdater — fetch game-thread data for GPU upload
// ============================================================================

void FWaterFieldSceneExtension::FUpdater::PostGPUSceneUpdate(
	FRDGBuilder& GraphBuilder,
	FSceneUniformBuffer& SceneUniforms)
{
	// Fetch data from game thread proxy if needed
	// TODO: Implement proxy communication
}

// ============================================================================
// FRenderer — dispatch compute shader
// ============================================================================

void FWaterFieldSceneExtension::FRenderer::PreRender(FRDGBuilder& GraphBuilder)
{
	if (!Extension.bHasValidData)
	{
		// Initialize resources on first render
		InitializeResources_RenderThread(GraphBuilder);
		Extension.bHasValidData = true;
	}

	if (Extension.bNeedsUpdate)
	{
		DispatchWaterFieldCompute_RenderThread(GraphBuilder);
		Extension.bNeedsUpdate = false;
	}
}

void FWaterFieldSceneExtension::FRenderer::InitializeResources_RenderThread(FRDGBuilder& GraphBuilder)
{
	const FWaterFluidConfig& Config = Extension.CurrentConfig;
	FIntPoint GridExtent(Config.GridSize, Config.GridSize);

	// Create pooled render targets for persistent storage
	// Height field (R32F)
	{
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			GridExtent,
			PF_R32_FLOAT,
			FClearValueBinding::Black,
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_UAV,
			false);
		Desc.DebugName = TEXT("WaterHeightField");
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, Extension.HeightFieldRT, TEXT("WaterHeightField"));
	}

	// Velocity field (R32G32F)
	{
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			GridExtent,
			PF_G32R32F,
			FClearValueBinding::Black,
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_UAV,
			false);
		Desc.DebugName = TEXT("WaterVelocityField");
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, Extension.VelocityFieldRT, TEXT("WaterVelocityField"));
	}

	// Temp buffers for double buffering
	{
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			GridExtent,
			PF_R32_FLOAT,
			FClearValueBinding::Black,
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_UAV,
			false);
		Desc.DebugName = TEXT("WaterTempHeight");
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, Extension.TempHeightFieldRT, TEXT("WaterTempHeight"));
	}

	{
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			GridExtent,
			PF_G32R32F,
			FClearValueBinding::Black,
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_UAV,
			false);
		Desc.DebugName = TEXT("WaterTempVelocity");
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, Extension.TempVelocityFieldRT, TEXT("WaterTempVelocity"));
	}

	// Navier-Stokes specific: Pressure and Divergence
	if (Config.SolverType == EFluidSolverType::NavierStokes)
	{
		{
			FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
				GridExtent,
				PF_R32_FLOAT,
				FClearValueBinding::Black,
				TexCreate_None,
				TexCreate_ShaderResource | TexCreate_UAV,
				false);
			Desc.DebugName = TEXT("WaterPressureField");
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, Extension.PressureFieldRT, TEXT("WaterPressureField"));
		}

		{
			FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
				GridExtent,
				PF_R32_FLOAT,
				FClearValueBinding::Black,
				TexCreate_None,
				TexCreate_ShaderResource | TexCreate_UAV,
				false);
			Desc.DebugName = TEXT("WaterDivergenceField");
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, Extension.DivergenceFieldRT, TEXT("WaterDivergenceField"));
		}
	}

	UE_LOG(LogWaterFieldSceneExtension, Log, TEXT("Initialized WaterField resources: %dx%d"), Config.GridSize, Config.GridSize);
}

void FWaterFieldSceneExtension::FRenderer::DispatchWaterFieldCompute_RenderThread(FRDGBuilder& GraphBuilder)
{
	if (!Extension.HeightFieldRT.IsValid() || !Extension.VelocityFieldRT.IsValid())
		return;

	const FWaterFluidConfig& Config = Extension.CurrentConfig;

	// Register external textures in RenderGraph
	FRDGTextureRef HeightTexture = GraphBuilder.RegisterExternalTexture(Extension.HeightFieldRT, TEXT("WaterHeightField"));
	FRDGTextureRef VelocityTexture = GraphBuilder.RegisterExternalTexture(Extension.VelocityFieldRT, TEXT("WaterVelocityField"));

	// Dispatch appropriate solver
	if (Config.SolverType == EFluidSolverType::ShallowWater)
	{
		ExecuteShallowWaterSolver_RenderThread(GraphBuilder, Config.TimeStep);
	}
	else
	{
		ExecuteNavierStokesSolver_RenderThread(GraphBuilder, Config.TimeStep);
	}

	Extension.CurrentTime += Config.TimeStep;
}

void FWaterFieldSceneExtension::FRenderer::ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder, float DeltaTime)
{
	// TODO: Implement shallow water solver dispatch
	// Requires shader conversion to texture-based operations
	UE_LOG(LogWaterFieldSceneExtension, VeryVerbose, TEXT("Shallow Water solver (stub), dt=%.4f"), DeltaTime);
}

void FWaterFieldSceneExtension::FRenderer::ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder, float DeltaTime)
{
	// TODO: Implement Navier-Stokes solver dispatch
	UE_LOG(LogWaterFieldSceneExtension, VeryVerbose, TEXT("Navier-Stokes solver (stub), dt=%.4f"), DeltaTime);
}

void FWaterFieldSceneExtension::FRenderer::ApplyDisturbances_RenderThread(FRDGBuilder& GraphBuilder)
{
	// TODO: Disturbance application
}

void FWaterFieldSceneExtension::FRenderer::ApplyInteractions_RenderThread(FRDGBuilder& GraphBuilder)
{
	// TODO: Interaction application
}
