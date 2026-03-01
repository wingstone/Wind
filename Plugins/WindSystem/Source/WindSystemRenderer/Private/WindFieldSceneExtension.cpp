// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldSceneExtension.h"
#include "WindFieldComputeShader.h"
#include "WindFieldGPUData.h"
#include "ScenePrivate.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "PixelShaderUtils.h"

IMPLEMENT_SCENE_EXTENSION(FWindFieldSceneExtension);

// ============================================================================
// FWindFieldSceneExtension
// ============================================================================

bool FWindFieldSceneExtension::ShouldCreateExtension(FScene& Scene)
{
	return Scene.GetWorld() != nullptr;
}

FWindFieldSceneExtension::FWindFieldSceneExtension(FScene& InScene)
	: ISceneExtension(InScene)
{
	CachedWorld = InScene.GetWorld();
}

FWindFieldSceneExtension::~FWindFieldSceneExtension()
{
	WindFieldRT.SafeRelease();
}

void FWindFieldSceneExtension::InitExtension(FScene& InScene)
{
	if (CachedWorld)
	{
		FieldProxy = FWindFieldProxyRegistry::Get().Find(CachedWorld);
	}
}

ISceneExtensionUpdater* FWindFieldSceneExtension::CreateUpdater()
{
	return new FUpdater(*this);
}

ISceneExtensionRenderer* FWindFieldSceneExtension::CreateRenderer(
	FSceneRendererBase& InSceneRenderer,
	const FEngineShowFlags& EngineShowFlags)
{
	return new FRenderer(InSceneRenderer, *this);
}

// ============================================================================
// FUpdater — fetch game-thread data for GPU upload
// ============================================================================

void FWindFieldSceneExtension::FUpdater::PostGPUSceneUpdate(
	FRDGBuilder& GraphBuilder,
	FSceneUniformBuffer& SceneUniforms)
{
	// Lazily resolve the proxy if not yet connected
	if (!Extension.FieldProxy.IsValid() && Extension.CachedWorld)
	{
		Extension.FieldProxy = FWindFieldProxyRegistry::Get().Find(Extension.CachedWorld);
	}

	if (!Extension.FieldProxy.IsValid())
	{
		return;
	}

	// Pull latest data from the game thread
	Extension.bNeedsUpdate = Extension.FieldProxy->FetchUpdate_RenderThread(
		Extension.CurrentSources,
		Extension.FieldCenter,
		Extension.CurrentTime,
		Extension.CurrentConfig);
}

// ============================================================================
// FRenderer — dispatch compute shader and produce wind field texture
// ============================================================================

void FWindFieldSceneExtension::FRenderer::PreRender(FRDGBuilder& GraphBuilder)
{
	// Only dispatch if we have new data or need initial generation
	if (!Extension.bNeedsUpdate && Extension.bHasValidData)
	{
		return;
	}

	if (Extension.CurrentSources.Num() == 0)
	{
		return;
	}

	DispatchWindFieldCompute(GraphBuilder);
}

void FWindFieldSceneExtension::FRenderer::DispatchWindFieldCompute(FRDGBuilder& GraphBuilder)
{
	const FWindFieldConfig& Config = Extension.CurrentConfig;
	const FIntVector Resolution = Config.Resolution;

	if (Resolution.X <= 0 || Resolution.Y <= 0 || Resolution.Z <= 0)
	{
		return;
	}

	// Verify the compute shader is available
	TShaderMapRef<FWindFieldComputeCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	if (!ComputeShader.IsValid())
	{
		return;
	}

	// ----- Create 3D wind field texture -----
	FRDGTextureDesc TexDesc = FRDGTextureDesc::Create3D(
		FIntVector(Resolution.X, Resolution.Y, Resolution.Z),
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);

	FRDGTextureRef WindFieldTexture = GraphBuilder.CreateTexture(TexDesc, TEXT("WindFieldTexture"));
	FRDGTextureUAVRef WindFieldUAV = GraphBuilder.CreateUAV(WindFieldTexture);

	// ----- Upload wind source data to structured buffer -----
	const uint32 SourceCount = Extension.CurrentSources.Num();
	const uint32 BufferSize = FMath::Max(SourceCount, 1u);

	FRDGBufferDesc BufDesc = FRDGBufferDesc::CreateStructuredDesc(
		sizeof(FGPUWindSourceData), BufferSize);
	FRDGBufferRef SourceBuffer = GraphBuilder.CreateBuffer(BufDesc, TEXT("WindSourceBuffer"));

	// Upload source data
	if (SourceCount > 0)
	{
		void* UploadData = GraphBuilder.Alloc(SourceCount * sizeof(FGPUWindSourceData), 16);
		FMemory::Memcpy(UploadData, Extension.CurrentSources.GetData(), SourceCount * sizeof(FGPUWindSourceData));
		GraphBuilder.QueueBufferUpload(SourceBuffer, UploadData, SourceCount * sizeof(FGPUWindSourceData), ERDGInitialDataFlags::NoCopy);
	}

	FRDGBufferSRVRef SourceSRV = GraphBuilder.CreateSRV(SourceBuffer);

	// ----- Set up compute shader parameters -----
	FWindFieldComputeCS::FParameters* PassParams = GraphBuilder.AllocParameters<FWindFieldComputeCS::FParameters>();
	PassParams->FieldOrigin = Extension.FieldCenter - FVector3f(Config.WorldExtent) * 0.5f;
	PassParams->Time = Extension.CurrentTime;
	PassParams->FieldExtent = FVector3f(Config.WorldExtent);
	PassParams->SourceCount = SourceCount;
	PassParams->ResolutionX = Resolution.X;
	PassParams->ResolutionY = Resolution.Y;
	PassParams->ResolutionZ = Resolution.Z;
	PassParams->Padding0 = 0.0f;
	PassParams->WindSources = SourceSRV;
	PassParams->WindFieldOutput = WindFieldUAV;

	// ----- Dispatch -----
	const FIntVector GroupCount(
		FMath::DivideAndRoundUp(Resolution.X, 8),
		FMath::DivideAndRoundUp(Resolution.Y, 8),
		FMath::DivideAndRoundUp(Resolution.Z, 8));

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("WindFieldCompute %dx%dx%d (%d sources)",
			Resolution.X, Resolution.Y, Resolution.Z, SourceCount),
		ComputeShader,
		PassParams,
		GroupCount);

	// ----- Extract for persistence across frames -----
	GraphBuilder.QueueTextureExtraction(WindFieldTexture, &Extension.WindFieldRT);

	Extension.bNeedsUpdate = false;
	Extension.bHasValidData = true;
}
