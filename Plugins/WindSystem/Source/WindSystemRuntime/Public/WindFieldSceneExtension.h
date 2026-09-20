// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneExtensions.h"
#include "RenderGraphResources.h"
#include "WindFieldTypes.h"

class FRDGBuilder;
class FSceneUniformBuffer;

/**
 * Scene Extension that manages GPU wind field resources and dispatches
 * a compute shader to generate a 3D wind velocity texture each frame.
 *
 * Architecture (mirrors Fluid2DSystem pattern):
 *  - FWindFieldSceneExtension: persistent per-FScene data (GPU resources)
 *  - FUpdater: dispatches compute shader during scene update
 *  - FRenderer: fills Scene Uniform Buffer so materials can sample wind
 *
 * Data flow:
 *  Game Thread (UWindSubsystem::Tick)
 *    → ENQUEUE_RENDER_COMMAND → SetSourceData_RenderThread()
 *    → FUpdater::PreSceneUpdate dispatches compute
 *    → FRenderer::UpdateSceneUniformBuffer exposes to materials
 */
class FWindFieldSceneExtension : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(WINDSYSTEMRUNTIME_API, FWindFieldSceneExtension);

public:

	// ------------------------------------------------------------------
	// Updater: dispatches wind field compute shader
	// ------------------------------------------------------------------
	class FUpdater : public ISceneExtensionUpdater
	{
		DECLARE_SCENE_EXTENSION_UPDATER(FUpdater, FWindFieldSceneExtension);
	public:
		FUpdater(FWindFieldSceneExtension* InExtension) : SceneData(InExtension) {}

		virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet) override;
		virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) override;

	private:
		/**
		 * Shift wind field volume data by integer texel offset, zeroing newly exposed regions.
		 * Reads from InField and writes the scrolled result into a transient texture, which is returned.
		 * Returns InField unchanged when there is no pending scroll offset.
		 */
		FRDGTextureRef ApplyScroll_RenderThread(FRDGBuilder& GraphBuilder, FRDGTextureRef InField, const FRDGTextureDesc& TransientDesc);

		FWindFieldSceneExtension* SceneData;
	};

	// ------------------------------------------------------------------
	// Renderer: fills scene uniform buffer with wind field data
	// ------------------------------------------------------------------
	class FRenderer : public ISceneExtensionRenderer
	{
		DECLARE_SCENE_EXTENSION_RENDERER(FRenderer, FWindFieldSceneExtension);
	public:
		FRenderer(FSceneRendererBase& InSceneRenderer, FWindFieldSceneExtension* InExtension)
			: ISceneExtensionRenderer(InSceneRenderer), SceneData(InExtension) {}

		virtual void PreRender(FRDGBuilder& GraphBuilder) override;
		virtual void UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& Buffer) override;

	private:
		FWindFieldSceneExtension* SceneData;
	};

	static bool ShouldCreateExtension(FScene& Scene);

	explicit FWindFieldSceneExtension(FScene& InScene);
	virtual ~FWindFieldSceneExtension();

	// ISceneExtension interface
	virtual void InitExtension(FScene& InScene) override;
	virtual ISceneExtensionUpdater* CreateUpdater() override;
	virtual ISceneExtensionRenderer* CreateRenderer(FSceneRendererBase& InSceneRenderer, const FEngineShowFlags& EngineShowFlags) override;

	/** Called from render thread via ENQUEUE_RENDER_COMMAND */
	void SetSourceData_RenderThread(
		const TArray<FGPUWindSourceData>& InSources,
		const FVector3f& InCenter,
		float InTime,
		float InDeltaTime,
		const FWindFieldConfig& InConfig,
		const FWindDirectionalData& InDirectionalData);

	/** Set the pending scroll offset (in texels) from the game thread */
	void SetScrollOffset_RenderThread(const FIntVector& NewGridScrollOffset);

	/**
	 * Render-thread snapshot for external consumers (e.g. Niagara Data Interface).
	 * OutTexture is null if no frame has been produced yet.
	 * Origin / InvExtent are in world space; multiply (WorldPos - Origin) * InvExtent to get UVW.
	 */
	WINDSYSTEMRUNTIME_API void GetRenderState_RenderThread(
		FRHITexture*& OutTexture,
		FVector3f& OutOrigin,
		FVector3f& OutInvExtent,
		FVector3f& OutDirectionalDirection,
		float& OutDirectionalStrength) const;

private:

	// --- GPU Resources (render thread owned) ---
	/**
	 * Persistent simulation state — diffused result from the previous frame,
	 * BEFORE directional-wind composition. Serves as the "previous frame" input
	 * for temporal advection, keeping the compose overlay out of the feedback loop.
	 */
	TRefCountPtr<IPooledRenderTarget> PrevSimulationRT;

	/**
	 * Persistent final output — diffused result WITH directional-wind composition
	 * overlay. This is the texture materials sample via the Scene Uniform Buffer.
	 * Every other RT used by the simulation is allocated transiently per-frame.
	 */
	TRefCountPtr<IPooledRenderTarget> OutWindFieldRT;

	// --- Current frame data (render thread) ---
	TArray<FGPUWindSourceData> CurrentSources;
	FVector3f FieldCenter = FVector3f::ZeroVector;
	FVector3f PrevFieldCenter = FVector3f::ZeroVector;
	float CurrentTime = 0.0f;
	float DeltaTime = 0.0f;
	FWindFieldConfig CurrentConfig;
	FWindDirectionalData DirectionalData;
	bool bNeedsUpdate = false;

	// --- Scroll tracking (render thread) ---
	/** Quantized world-space center of the grid (updated by scroll) */
	FVector3f WorldGridCenter = FVector3f::ZeroVector;
	/** Whether WorldGridCenter has been seeded from the first incoming field center */
	bool bGridCenterInitialized = false;
	/** Pending scroll offset in texels, consumed by ApplyScroll_RenderThread */
	FIntVector GridScrollOffset = FIntVector::ZeroValue;

	// --- Debug Slice Atlas (enabled via r.WindField.DebugSlice CVar) ---
	TRefCountPtr<IPooledRenderTarget> DebugSliceRT;
};
