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
}

FWaterFieldSceneExtension::~FWaterFieldSceneExtension()
{
}

void FWaterFieldSceneExtension::InitExtension(FScene& InScene)
{	
	check(IsInGameThread());

	Scene = &InScene;
	UWaterSubsystem* SubSystem = UWaterSubsystem::GetSubsystem(Scene->GetWorld());
	SubSystem->Reset();
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

void FWaterFieldSceneExtension::SetConfig_RenderThread(const FWaterFluidConfig& NewConfig)
{
	check(IsInRenderingThread());
	CurrentConfig = NewConfig;
}

void FWaterFieldSceneExtension::AddInteraction_RenderThread(const FWaterInteractionData& Interaction)
{
	check(IsInRenderingThread());
	CurrentInteractions.Add(Interaction);
}

void FWaterFieldSceneExtension::ResetState_RenderThread(FRHICommandListImmediate& RHICmdList, bool bNewEnable)
{
	check(IsInRenderingThread());

	CurrentInteractions.Empty();
	CurrentConfig.SolverType = EFluidSolverType::ShallowWater;
	CurrentConfig.GridSize = 256;
	CurrentConfig.WorldSize = 10000.0f;
	CurrentConfig.Density = 1.0f;

	HeightFieldRT.SafeRelease();
	VelocityFieldRT.SafeRelease();
	TempHeightFieldRT.SafeRelease();
	TempVelocityFieldRT.SafeRelease();
	PressureFieldRT.SafeRelease();
	DivergenceFieldRT.SafeRelease();
}


// ============================================================================
// FUpdater — fetch game-thread data for GPU upload
// ============================================================================

void FWaterFieldSceneExtension::FUpdater::ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	// Requires shader conversion to texture-based operations
	UE_LOG(LogWaterFieldSceneExtension, VeryVerbose, TEXT("Shallow Water solver (stub)"));
	
}
void FWaterFieldSceneExtension::FUpdater::ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	// TODO: Implement Navier-Stokes solver dispatch
	// Requires shader conversion to texture-based operations
	UE_LOG(LogWaterFieldSceneExtension, VeryVerbose, TEXT("Navier-Stokes solver (stub)"));
}

FORCEINLINE FPooledRenderTargetDesc CreateWindRenderTargetDesc(int32 RenderTargetSize, EPixelFormat Format = EPixelFormat::PF_R16F)
{
	return FPooledRenderTargetDesc::Create2DDesc(
		FIntPoint(RenderTargetSize),
		Format,
		FClearValueBinding::None,
		TexCreate_None,
		TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV,
		false
	);
}

void FWaterFieldSceneExtension::FUpdater::PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms)
{
	if (Extension.CurrentConfig.SolverType == EFluidSolverType::ShallowWater)
	{
		if (!SceneData->HeightFieldRT.IsValid())
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(SceneData->RenderTargetSize), SceneData->HeightFieldRT, "HeightFieldRT");
		}
		if (SceneData->VelocityFieldRT.IsValid())
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(SceneData->RenderTargetSize, EPixelFormat::PF_G16R16F), SceneData->VelocityFieldRT, "VelocityFieldRT");
		}
		ExecuteShallowWaterSolver_RenderThread(GraphBuilder);
	}
	else
	{
		ExecuteNavierStokesSolver_RenderThread(GraphBuilder);
	}
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

void FWaterFieldSceneExtension::FRenderer::ApplyInteractions_RenderThread(FRDGBuilder& GraphBuilder)
{
	// TODO: Interaction application
}
