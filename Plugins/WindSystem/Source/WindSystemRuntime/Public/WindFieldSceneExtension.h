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

		virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms) override;
		virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) override;

	private:
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

private:

	// --- GPU Resources (render thread owned) ---
	/** Double-buffered wind field textures for temporal blending */
	TRefCountPtr<IPooledRenderTarget> WindFieldRT[2];
	int32 CurrentRTIndex = 0;

	// --- Current frame data (render thread) ---
	TArray<FGPUWindSourceData> CurrentSources;
	FVector3f FieldCenter = FVector3f::ZeroVector;
	FVector3f PrevFieldCenter = FVector3f::ZeroVector;
	float CurrentTime = 0.0f;
	float DeltaTime = 0.0f;
	FWindFieldConfig CurrentConfig;
	FWindDirectionalData DirectionalData;
	bool bNeedsUpdate = false;

	// --- Debug Slice Atlas (enabled via r.WindField.DebugSlice CVar) ---
	TRefCountPtr<IPooledRenderTarget> DebugSliceRT;
};
