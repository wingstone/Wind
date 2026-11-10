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
 * Simulation fields owned by a single solver type. Several solver types can be enabled at the same time,
 * so the scene extension keeps one layer per EFluidSolverType and they never share render targets,
 * grid mapping or flow settings.
 */
struct FLUID2DSYSTEMRUNTIME_API FFluid2DSolverLayer
{
	TRefCountPtr<IPooledRenderTarget> HeightFieldRT[3];
	TRefCountPtr<IPooledRenderTarget> VelocityFieldRT[3];
	TRefCountPtr<IPooledRenderTarget> TempHeightFieldRT;
	TRefCountPtr<IPooledRenderTarget> TempVelocityFieldRT;
	TRefCountPtr<IPooledRenderTarget> NormalFieldRT;
	TRefCountPtr<IPooledRenderTarget> PressureFieldRT;
	TRefCountPtr<IPooledRenderTarget> TempPressureFieldRT;
	TRefCountPtr<IPooledRenderTarget> DivergenceFieldRT;
	TRefCountPtr<IPooledRenderTarget> VorticityFieldRT;
	TRefCountPtr<IPooledRenderTarget> OutputVelocityFieldRT;

	uint32 CurrentHeightIndex = 0;
	uint32 CurrentVelocityIndex = 0;

	/** World-space center of this layer's grid (each layer owns its own domain, so this differs per layer). */
	FVector2f WorldGridCenter = FVector2f::ZeroVector;

	/** Pending integer texel scroll of this layer's grid, consumed by the next simulation dispatch. */
	FIntVector2 GridScrollOffset = FIntVector2::ZeroValue;

	/** Flow volume / noise state applied to this layer. */
	FFluid2DGlobalFlowData GlobalFlowData;

	/** Releases every render target owned by the layer and resets the ping-pong indices. */
	void ReleaseResources();

	TRefCountPtr<IPooledRenderTarget>& GetCurrentHeightField() { return HeightFieldRT[CurrentHeightIndex]; }
	TRefCountPtr<IPooledRenderTarget>& GetCurrentVelocityField() { return OutputVelocityFieldRT.IsValid() ? OutputVelocityFieldRT : VelocityFieldRT[CurrentVelocityIndex]; }
	TRefCountPtr<IPooledRenderTarget>& GetNormalField() { return NormalFieldRT; }
};

/**
 * Scene Extension that manages GPU water field resources and dispatches
 * compute shaders to simulate 2D fluid dynamics each frame.
 *
 * Architecture:
 *  - FFluid2DFieldSceneExtension: persistent per-FScene data (GPU resources)
 *  - FFluid2DSolverLayer: one independent set of fields per enabled solver type
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
 
		void ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, const FSWFluidConfig& Config);
		void ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, const FNSFluidConfig& Config);
		void ExecuteMaskAccumulateSolver_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, const FMAFluidConfig& Config);
		void ApplyScroll_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, const FFluid2DSolverConfigBase& Config, const FIntPoint& TexelOffset);

	private:
		/** Creates the render targets the given solver type needs (if missing) and clears the new ones. */
		void EnsureLayerResources_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, EFluidSolverType SolverType, const FFluid2DSolverConfigBase& Config);

		/** Dispatches the solver matching SolverType on the given layer. */
		void ExecuteSolver_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, EFluidSolverType SolverType);

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
	void SetScrollOffset_RenderThread(EFluidSolverType SolverType, const FIntVector2& NewGridScrollOffset);
	void SetInteractionsToApply_RenderThread(const TArray<FFluid2DInteractionData>& InteractionsToApply);
	void SetGlobalFlowData_RenderThread(EFluidSolverType SolverType, const FFluid2DGlobalFlowData& InFlowData);
	void ResetState_RenderThread(FRHICommandListImmediate& RHICmdList);

private:

	// --- GPU Resources (render thread owned) ---
	/** One layer per solver type; a layer is only simulated while its solver config is enabled. */
	FFluid2DSolverLayer SolverLayers[Fluid2DSolverType::Count];

	// --- Current frame data (render thread) ---
	TArray<FFluid2DInteractionData> CurrentInteractions;
	FFluid2DFluidConfig CurrentConfig;
};
