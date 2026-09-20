// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneExtensions.h"
#include "RenderGraphResources.h"
#include "Fluid2DFluidTypes.h"

class FFluid2DFieldProxy;
class FRDGBuilder;
class FSceneUniformBuffer;

/**
 * Scene Extension that manages GPU water field resources and dispatches
 * compute shaders to simulate 2D fluid dynamics each frame.
 *
 * Architecture:
 *  - FFluid2DFieldSceneExtension: persistent per-FScene data (GPU resources)
 *  - FUpdater: fetches game-thread data and uploads to GPU buffers
 *  - FRenderer: dispatches compute shader for fluid simulation
 */
class FFluid2DFieldSceneExtension : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(FLUID2DSYSTEMRUNTIME_API, FFluid2DFieldSceneExtension);

public:

	// ------------------------------------------------------------------
	// Updater: runs during scene update phase (render thread)
	// ------------------------------------------------------------------
	class FUpdater : public ISceneExtensionUpdater
	{
		DECLARE_SCENE_EXTENSION_UPDATER(FUpdater, FFluid2DFieldSceneExtension);
	public:
		FUpdater(FFluid2DFieldSceneExtension* InExtension) : SceneData(InExtension) {}

		//~ Begin ISceneExtensionUpdater Interface.
		virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet) override;
		virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) override;
		//~ End ISceneExtensionUpdater Interface.
 
		void ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder);
		void ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder);
		void ExecuteMaskAccumulateSolver_RenderThread(FRDGBuilder& GraphBuilder);
		void ApplyScroll_RenderThread(FRDGBuilder& GraphBuilder);

	private:
		FFluid2DFieldSceneExtension* SceneData;
	};

	// ------------------------------------------------------------------
	// Renderer: dispatches compute for fluid simulation
	// ------------------------------------------------------------------
	class FRenderer : public ISceneExtensionRenderer
	{
		DECLARE_SCENE_EXTENSION_RENDERER(FRenderer, FFluid2DFieldSceneExtension);
	public:
		FRenderer(FSceneRendererBase& InSceneRenderer, FFluid2DFieldSceneExtension* InExtension)
			: ISceneExtensionRenderer(InSceneRenderer), SceneData(InExtension) {}

		virtual void PreRender(FRDGBuilder& GraphBuilder) override;
		virtual void UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& Buffer) override;

	private:
		FFluid2DFieldSceneExtension* SceneData;
	};
	
	static bool ShouldCreateExtension(FScene& Scene);

	explicit FFluid2DFieldSceneExtension(FScene& InScene);
	virtual ~FFluid2DFieldSceneExtension();

	// ISceneExtension interface
	virtual void InitExtension(FScene& InScene) override;
	virtual ISceneExtensionUpdater* CreateUpdater() override;
	virtual ISceneExtensionRenderer* CreateRenderer(FSceneRendererBase& InSceneRenderer, const FEngineShowFlags& EngineShowFlags) override;

	void SetConfig_RenderThread(const FFluid2DFluidConfig& NewConfig);
	void SetScrollOffset_RenderThread(const FIntVector2& NewGridScrollOffset);
	void SetInteractionsToApply_RenderThread(const TArray<FFluid2DInteractionData>& InteractionsToApply);
	void SetGlobalFlowData_RenderThread(const FFluid2DGlobalFlowData& InFlowData);
	void ResetState_RenderThread(FRHICommandListImmediate& RHICmdList);

private:

	// --- GPU Resources (render thread owned) ---
	TRefCountPtr<IPooledRenderTarget> HeightFieldRT[3];
	TRefCountPtr<IPooledRenderTarget> VelocityFieldRT[3];
	TRefCountPtr<IPooledRenderTarget> NormalFieldRT;

	TRefCountPtr<IPooledRenderTarget> TempHeightFieldRT;
	TRefCountPtr<IPooledRenderTarget> TempVelocityFieldRT;
    
	TRefCountPtr<IPooledRenderTarget> PressureFieldRT;
	TRefCountPtr<IPooledRenderTarget> TempPressureFieldRT;
	TRefCountPtr<IPooledRenderTarget> DivergenceFieldRT;
	TRefCountPtr<IPooledRenderTarget> VorticityFieldRT;
	TRefCountPtr<IPooledRenderTarget> OutputVelocityFieldRT;

	// --- Current frame data (render thread) ---
	TArray<FFluid2DInteractionData> CurrentInteractions;
	FFluid2DFluidConfig CurrentConfig;
	FFluid2DGlobalFlowData CurrentGlobalFlowData;
	FVector2f WorldGridCenter;
	FIntVector2 GridScrollOffset;

	uint32 CurrentHeightIndex = 0;
	uint32 CurrentVelocityIndex = 0;
};
