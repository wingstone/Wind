// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneExtensions.h"
#include "RenderGraphResources.h"
#include "WaterFluidTypes.h"

class FWaterFieldProxy;
class FRDGBuilder;
class FSceneUniformBuffer;

/**
 * Scene Extension that manages GPU water field resources and dispatches
 * compute shaders to simulate 2D fluid dynamics each frame.
 *
 * Architecture:
 *  - FWaterFieldSceneExtension: persistent per-FScene data (GPU resources)
 *  - FUpdater: fetches game-thread data and uploads to GPU buffers
 *  - FRenderer: dispatches compute shader for fluid simulation
 */
class FWaterFieldSceneExtension : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(WATERSYSTEMRENDERER_API, FWaterFieldSceneExtension);

public:

	// ------------------------------------------------------------------
	// Updater: runs during scene update phase (render thread)
	// ------------------------------------------------------------------
	class FUpdater : public ISceneExtensionUpdater
	{
		DECLARE_SCENE_EXTENSION_UPDATER(FUpdater, FWaterFieldSceneExtension);
	public:
		FUpdater(FWaterFieldSceneExtension& InExtension) : Extension(InExtension) {}

		//~ Begin ISceneExtensionUpdater Interface.
		virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms) override;
		virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) override;
		//~ End ISceneExtensionUpdater Interface.

		void ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder);
		void ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder);

	private:
		FWaterFieldSceneExtension& Extension;
	};

	// ------------------------------------------------------------------
	// Renderer: dispatches compute for fluid simulation
	// ------------------------------------------------------------------
	class FRenderer : public ISceneExtensionRenderer
	{
		DECLARE_SCENE_EXTENSION_RENDERER(FRenderer, FWaterFieldSceneExtension);
	public:
		FRenderer(FSceneRendererBase& InSceneRenderer, FWaterFieldSceneExtension& InExtension)
			: ISceneExtensionRenderer(InSceneRenderer), Extension(InExtension) {}

		virtual void PreRender(FRDGBuilder& GraphBuilder) override;

	private:
		FWaterFieldSceneExtension& Extension;

		void InitializeResources_RenderThread(FRDGBuilder& GraphBuilder);
		void DispatchWaterFieldCompute_RenderThread(FRDGBuilder& GraphBuilder);
		void ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder, float DeltaTime);
		void ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder, float DeltaTime);
		void ApplyInteractions_RenderThread(FRDGBuilder& GraphBuilder);
	};
	
	static bool ShouldCreateExtension(FScene& Scene);

	explicit FWaterFieldSceneExtension(FScene& InScene);
	virtual ~FWaterFieldSceneExtension();

	// ISceneExtension interface
	virtual void InitExtension(FScene& InScene) override;
	virtual ISceneExtensionUpdater* CreateUpdater() override;
	virtual ISceneExtensionRenderer* CreateRenderer(FSceneRendererBase& InSceneRenderer, const FEngineShowFlags& EngineShowFlags) override;

	void SetConfig_RenderThread(const FWaterFluidConfig& NewConfig);
	void AddInteraction_RenderThread(const FWaterInteractionData& Interaction);

private:

	// --- GPU Resources (render thread owned) ---
	TRefCountPtr<IPooledRenderTarget> HeightFieldRT;
	TRefCountPtr<IPooledRenderTarget> VelocityFieldRT;

	TRefCountPtr<IPooledRenderTarget> TempHeightFieldRT;
	TRefCountPtr<IPooledRenderTarget> TempVelocityFieldRT;
	TRefCountPtr<IPooledRenderTarget> PressureFieldRT;
	TRefCountPtr<IPooledRenderTarget> DivergenceFieldRT;

	// --- Current frame data (render thread) ---
	TArray<FWaterInteractionData> CurrentInteractions;
	FWaterFluidConfig CurrentConfig;
};
