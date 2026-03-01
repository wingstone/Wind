// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneExtensions.h"
#include "WindFieldTypes.h"
#include "RenderGraphResources.h"
#include "ScreenRendering.h"

class FWindFieldProxy;
class FRDGBuilder;
class FSceneUniformBuffer;

/**
 * Scene Extension that manages GPU wind field resources and dispatches
 * compute shaders to generate a 3D wind velocity texture each frame.
 *
 * Architecture:
 *  - FWindFieldSceneExtension: persistent per-FScene data (GPU resources, proxy ref)
 *  - FUpdater: fetches game-thread data and uploads to GPU buffers
 *  - FRenderer: dispatches compute shader and injects results into scene
 */
class FWindFieldSceneExtension : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(WINDSYSTEMRENDERER_API, FWindFieldSceneExtension);

public:
	static bool ShouldCreateExtension(FScene& Scene);

	explicit FWindFieldSceneExtension(FScene& InScene);
	virtual ~FWindFieldSceneExtension();

	// ISceneExtension interface
	virtual void InitExtension(FScene& InScene) override;
	virtual ISceneExtensionUpdater* CreateUpdater() override;
	virtual ISceneExtensionRenderer* CreateRenderer(FSceneRendererBase& InSceneRenderer, const FEngineShowFlags& EngineShowFlags) override;

	// --- GPU Resources (render thread owned) ---
	TRefCountPtr<IPooledRenderTarget> WindFieldRT;

	// --- Current frame data (render thread) ---
	TArray<FGPUWindSourceData> CurrentSources;
	FVector3f FieldCenter = FVector3f::ZeroVector;
	float CurrentTime = 0.0f;
	FWindFieldConfig CurrentConfig;
	bool bNeedsUpdate = false;
	bool bHasValidData = false;

	// --- Bridge to game thread ---
	TSharedPtr<FWindFieldProxy> FieldProxy;
	UWorld* CachedWorld = nullptr;

	// ------------------------------------------------------------------
	// Updater: runs during scene update phase (render thread)
	// ------------------------------------------------------------------
	class FUpdater : public ISceneExtensionUpdater
	{
		DECLARE_SCENE_EXTENSION_UPDATER(FUpdater, FWindFieldSceneExtension);
	public:
		FUpdater(FWindFieldSceneExtension& InExtension) : Extension(InExtension) {}

		virtual void PostGPUSceneUpdate(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniforms) override;

	private:
		FWindFieldSceneExtension& Extension;
	};

	// ------------------------------------------------------------------
	// Renderer: dispatches compute and writes scene uniform buffer
	// ------------------------------------------------------------------
	class FRenderer : public ISceneExtensionRenderer
	{
		DECLARE_SCENE_EXTENSION_RENDERER(FRenderer, FWindFieldSceneExtension);
	public:
		FRenderer(FSceneRendererBase& InSceneRenderer, FWindFieldSceneExtension& InExtension)
			: ISceneExtensionRenderer(InSceneRenderer), Extension(InExtension) {}

		virtual void PreRender(FRDGBuilder& GraphBuilder) override;

	private:
		FWindFieldSceneExtension& Extension;

		void DispatchWindFieldCompute(FRDGBuilder& GraphBuilder);
	};
};
