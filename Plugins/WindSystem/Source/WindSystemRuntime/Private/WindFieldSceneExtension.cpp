// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldSceneExtension.h"
#include "WindFieldShaders.h"
#include "ScenePrivate.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogWindFieldSceneExtension, Log, All);

static TAutoConsoleVariable<int32> CVarWindFieldDebugSlice(
	TEXT("r.WindField.DebugSlice"),
	1,
	TEXT("Enable wind field 3D slice debug visualization (0 = off, 1 = on)"),
	ECVF_RenderThreadSafe);

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
	WorldGridCenter = FVector3f::ZeroVector;
	GridScrollOffset = FIntVector::ZeroValue;
}

FWindFieldSceneExtension::~FWindFieldSceneExtension()
{
	WindFieldRT[0].SafeRelease();
	WindFieldRT[1].SafeRelease();
	TempWindFieldRT.SafeRelease();
	DebugSliceRT.SafeRelease();
}

void FWindFieldSceneExtension::InitExtension(FScene& InScene)
{
	UE_LOG(LogWindFieldSceneExtension, Log, TEXT("WindFieldSceneExtension initialized"));
}

ISceneExtensionUpdater* FWindFieldSceneExtension::CreateUpdater()
{
	return new FUpdater(this);
}

ISceneExtensionRenderer* FWindFieldSceneExtension::CreateRenderer(
	FSceneRendererBase& InSceneRenderer,
	const FEngineShowFlags& EngineShowFlags)
{
	return new FRenderer(InSceneRenderer, this);
}

void FWindFieldSceneExtension::SetSourceData_RenderThread(
	const TArray<FGPUWindSourceData>& InSources,
	const FVector3f& InCenter,
	float InTime,
	float InDeltaTime,
	const FWindFieldConfig& InConfig,
	const FWindDirectionalData& InDirectionalData)
{
	check(IsInRenderingThread());
	CurrentSources = InSources;
	PrevFieldCenter = FieldCenter;
	FieldCenter = InCenter;
	CurrentTime = InTime;
	DeltaTime = InDeltaTime;
	CurrentConfig = InConfig;
	DirectionalData = InDirectionalData;
	bNeedsUpdate = true;
}

void FWindFieldSceneExtension::SetScrollOffset_RenderThread(const FIntVector& NewGridScrollOffset)
{
	check(IsInRenderingThread());
	GridScrollOffset = NewGridScrollOffset;
}

// ============================================================================
// FUpdater — dispatch wind field compute shader
// ============================================================================

void FWindFieldSceneExtension::FUpdater::ApplyScroll_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	const FWindFieldConfig& Config = SceneData->CurrentConfig;
	const FIntVector Res(
		FMath::Max(Config.Resolution.X, 1),
		FMath::Max(Config.Resolution.Y, 1),
		FMath::Max(Config.Resolution.Z, 1));
	const FVector3f CellSize(
		static_cast<float>(Config.WorldExtent.X) / static_cast<float>(Res.X),
		static_cast<float>(Config.WorldExtent.Y) / static_cast<float>(Res.Y),
		static_cast<float>(Config.WorldExtent.Z) / static_cast<float>(Res.Z));

	FIntVector TexelOffset = SceneData->GridScrollOffset;

	if (TexelOffset.X == 0 && TexelOffset.Y == 0 && TexelOffset.Z == 0)
	{
		return;
	}

	// Update world grid origin by the quantized amount
	SceneData->WorldGridCenter += FVector3f(
		static_cast<float>(TexelOffset.X) * CellSize.X,
		static_cast<float>(TexelOffset.Y) * CellSize.Y,
		static_cast<float>(TexelOffset.Z) * CellSize.Z);
	SceneData->GridScrollOffset = FIntVector::ZeroValue;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());
	const FIntVector GroupCount(
		FMath::DivideAndRoundUp(Res.X, 8),
		FMath::DivideAndRoundUp(Res.Y, 8),
		FMath::DivideAndRoundUp(Res.Z, 8));

	const TShaderMapRef<FWindFieldScrollCS> ScrollCS(GlobalShaderMap);

	// Scroll both double-buffered wind field textures
	for (int32 i = 0; i < 2; i++)
	{
		if (!SceneData->WindFieldRT[i] || !SceneData->TempWindFieldRT) continue;

		FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(SceneData->WindFieldRT[i]);
		FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(SceneData->TempWindFieldRT);

		FWindFieldScrollCS::FParameters* Params = GraphBuilder.AllocParameters<FWindFieldScrollCS::FParameters>();
		Params->ResolutionX = Res.X;
		Params->ResolutionY = Res.Y;
		Params->ResolutionZ = Res.Z;
		Params->ScrollOffsetX = TexelOffset.X;
		Params->ScrollOffsetY = TexelOffset.Y;
		Params->ScrollOffsetZ = TexelOffset.Z;
		Params->SourceVolume = GraphBuilder.CreateSRV(Source);
		Params->DestVolume = GraphBuilder.CreateUAV(Dest);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("WindField.Scroll_%d", i),
			ERDGPassFlags::Compute,
			ScrollCS,
			Params,
			GroupCount);

		// Swap so WindFieldRT[i] now points to the scrolled result
		Swap(SceneData->WindFieldRT[i], SceneData->TempWindFieldRT);
	}
}

void FWindFieldSceneExtension::FUpdater::PreSceneUpdate(
	FRDGBuilder& GraphBuilder,
	const FScenePreUpdateChangeSet& ChangeSet,
	FSceneUniformBuffer& SceneUniforms)
{
	const FWindFieldConfig& Config = SceneData->CurrentConfig;
	const FIntVector Res(
		FMath::Max(Config.Resolution.X, 1),
		FMath::Max(Config.Resolution.Y, 1),
		FMath::Max(Config.Resolution.Z, 1));

	// Ensure double-buffered 3D wind field textures exist
	FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::CreateVolumeDesc(
		Res.X, Res.Y, Res.Z,
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_None,
		TexCreate_ShaderResource | TexCreate_UAV,
		false);

	for (int32 i = 0; i < 2; ++i)
	{
		if (!SceneData->WindFieldRT[i])
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc,
				SceneData->WindFieldRT[i],
				i == 0 ? TEXT("WindFieldRT_0") : TEXT("WindFieldRT_1"));
		}
	}

	// Ensure temporary volume for scroll copy exists
	if (!SceneData->TempWindFieldRT)
	{
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc,
			SceneData->TempWindFieldRT, TEXT("TempWindFieldRT"));
	}

	// Apply pending scroll before simulation
	if (SceneData->GridScrollOffset != FIntVector::ZeroValue)
	{
		ApplyScroll_RenderThread(GraphBuilder);
	}

	if (!SceneData->bNeedsUpdate || (SceneData->CurrentSources.Num() == 0 && !SceneData->DirectionalData.IsValid()))
	{
		return;
	}

	// Double-buffer: write to current, read previous for temporal blending
	const int32 WriteIdx = SceneData->CurrentRTIndex;
	const int32 ReadIdx = 1 - WriteIdx;

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	FRDGTextureRef WindFieldTex = GraphBuilder.RegisterExternalTexture(SceneData->WindFieldRT[WriteIdx]);
	FRDGTextureRef PrevWindFieldTex = GraphBuilder.RegisterExternalTexture(SceneData->WindFieldRT[ReadIdx]);

	// Create transient intermediate textures
	FRDGTextureDesc IntermediateDesc = FRDGTextureDesc::Create3D(
		FIntVector(Res.X, Res.Y, Res.Z),
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef AdvectedTex = GraphBuilder.CreateTexture(IntermediateDesc, TEXT("WindField.Advected"));
	FRDGTextureRef ForcedTex = GraphBuilder.CreateTexture(IntermediateDesc, TEXT("WindField.Forced"));

	const bool bHasDirectionalWind = SceneData->DirectionalData.IsValid();

	// Upload wind source data to structured buffer
	const uint32 SourceCount = SceneData->CurrentSources.Num();
	FRDGBufferRef SourceBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUWindSourceData), FMath::Max(SourceCount, 1u)),
		TEXT("WindSourceBuffer"));

	if (SourceCount > 0)
	{
		void* UploadData = GraphBuilder.Alloc(SourceCount * sizeof(FGPUWindSourceData), 16);
		FMemory::Memcpy(UploadData, SceneData->CurrentSources.GetData(), SourceCount * sizeof(FGPUWindSourceData));
		GraphBuilder.QueueBufferUpload(SourceBuffer, UploadData, SourceCount * sizeof(FGPUWindSourceData), ERDGInitialDataFlags::NoCopy);
	}

	const FIntVector GroupCount(
		FMath::DivideAndRoundUp(Res.X, 8),
		FMath::DivideAndRoundUp(Res.Y, 8),
		FMath::DivideAndRoundUp(Res.Z, 8));

	const float ClampedDissipation = FMath::Clamp(Config.Dissipation, 0.8f, 1.0f);

	// ====================================================================
	// Pass 1: Advection — semi-Lagrangian backtrace
	// Input:  PrevWindFieldTex (previous frame)
	// Output: AdvectedTex
	// ====================================================================
	{
		TShaderMapRef<FWindFieldAdvectionCS> AdvectionShader(ShaderMap);

		FWindFieldAdvectionCS::FParameters* Params = GraphBuilder.AllocParameters<FWindFieldAdvectionCS::FParameters>();
		const FVector3f FieldOrigin = SceneData->WorldGridCenter - FVector3f(Config.WorldExtent) * 0.5f;
		Params->FieldOrigin = FieldOrigin;
		Params->Time = SceneData->CurrentTime;
		Params->FieldExtent = FVector3f(Config.WorldExtent);
		Params->Dissipation = ClampedDissipation;
		Params->ResolutionX = Res.X;
		Params->ResolutionY = Res.Y;
		Params->ResolutionZ = Res.Z;
		Params->DeltaTime = SceneData->DeltaTime;
		Params->PrevFieldOrigin = FieldOrigin;
		Params->Padding0 = 0.0f;
		Params->PrevField = GraphBuilder.CreateSRV(PrevWindFieldTex);
		Params->PrevFieldSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Params->OutputField = GraphBuilder.CreateUAV(AdvectedTex);

		// Bind directional wind (additive advection velocity)
		const FWindDirectionalData& DirData = SceneData->DirectionalData;
		if (bHasDirectionalWind)
		{
			Params->DirectionalWindDirection = DirData.WindDirection;
			Params->DirectionalWindStrength = DirData.Strength;
			Params->DirectionalNoiseStrength = DirData.NoiseStrength;
			Params->bHasDirectionalWind = 1;
			Params->Padding1 = 0.0f;
			if (DirData.HasNoiseTexture())
			{
				Params->DirectionalNoiseTexture = DirData.NoiseTextureRHI;
				Params->DirectionalNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
				Params->DirNoiseTiling = DirData.Tiling;
				Params->DirNoiseIntensityMin = DirData.IntensityMin;
				Params->DirNoiseIntensityMax = DirData.IntensityMax;
				Params->DirNoiseScrollSpeed = DirData.ScrollSpeed;
				Params->bHasDirectionalNoise = 1;
			}
			else
			{
				Params->DirectionalNoiseTexture = GWhiteTexture->TextureRHI;
				Params->DirectionalNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
				Params->DirNoiseTiling = 1.0f;
				Params->DirNoiseIntensityMin = 0.0f;
				Params->DirNoiseIntensityMax = 1.0f;
				Params->DirNoiseScrollSpeed = 0.0f;
				Params->bHasDirectionalNoise = 0;
			}
		}
		else
		{
			Params->DirectionalWindDirection = FVector3f::ZeroVector;
			Params->DirectionalWindStrength = 0.0f;
			Params->DirectionalNoiseStrength = 0.0f;
			Params->bHasDirectionalWind = 0;
			Params->Padding1 = 0.0f;
			Params->DirectionalNoiseTexture = GWhiteTexture->TextureRHI;
			Params->DirectionalNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
			Params->DirNoiseTiling = 1.0f;
			Params->DirNoiseIntensityMin = 0.0f;
			Params->DirNoiseIntensityMax = 1.0f;
			Params->DirNoiseScrollSpeed = 0.0f;
			Params->bHasDirectionalNoise = 0;
		}
		Params->Padding2 = 0.0f;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("WindField.Advection %dx%dx%d", Res.X, Res.Y, Res.Z),
			ERDGPassFlags::Compute,
			AdvectionShader,
			Params,
			GroupCount);
	}

	// ====================================================================
	// Pass 2: Force Injection — point/vortex wind sources
	// Input:  AdvectedTex (output of advection)
	// Output: ForcedTex
	// ====================================================================
	{
		TShaderMapRef<FWindFieldForceCS> ForceShader(ShaderMap);

		FWindFieldForceCS::FParameters* Params = GraphBuilder.AllocParameters<FWindFieldForceCS::FParameters>();
		Params->FieldOrigin = SceneData->WorldGridCenter - FVector3f(Config.WorldExtent) * 0.5f;
		Params->Time = SceneData->CurrentTime;
		Params->FieldExtent = FVector3f(Config.WorldExtent);
		Params->SourceCount = SourceCount;
		Params->ResolutionX = Res.X;
		Params->ResolutionY = Res.Y;
		Params->ResolutionZ = Res.Z;
		Params->DeltaTime = SceneData->DeltaTime;
		Params->Dissipation = ClampedDissipation;
		Params->Padding0 = 0.0f;
		Params->Padding1 = 0.0f;
		Params->Padding2 = 0.0f;
		Params->WindSources = GraphBuilder.CreateSRV(SourceBuffer);
		Params->AdvectedField = GraphBuilder.CreateSRV(AdvectedTex);
		Params->AdvectedFieldSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Params->OutputField = GraphBuilder.CreateUAV(ForcedTex);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("WindField.Force %dx%dx%d (%d sources)", Res.X, Res.Y, Res.Z, SourceCount),
			ERDGPassFlags::Compute,
			ForceShader,
			Params,
			GroupCount);
	}

	// Diffusion outputs to DiffusedTex when composition is needed, else directly to WindFieldTex
	FRDGTextureRef DiffusedTex = nullptr;
	FRDGTextureRef DiffusionOutputTarget;
	if (bHasDirectionalWind)
	{
		DiffusedTex = GraphBuilder.CreateTexture(IntermediateDesc, TEXT("WindField.Diffused"));
		DiffusionOutputTarget = DiffusedTex;
	}
	else
	{
		DiffusionOutputTarget = WindFieldTex;
	}

	// ====================================================================
	// Pass 3: Diffusion
	// Input:  ForcedTex (output of force injection)
	// Output: DiffusionOutputTarget
	// Jacobi method iterates multiple times with ping-pong textures.
	// ====================================================================
	{
		FWindFieldDiffusionCS::FPermutationDomain DiffusionPermutation;
		DiffusionPermutation.Set<FWindFieldDiffusionCS::FDiffusionMethod>(static_cast<int32>(Config.DiffusionMethod));
		TShaderMapRef<FWindFieldDiffusionCS> DiffusionShader(ShaderMap, DiffusionPermutation);

		const int32 NumIterations = (Config.DiffusionMethod == EWindDiffusionMethod::Jacobi)
			? FMath::Max(Config.JacobiIterations, 1) : 1;

		// Create ping-pong textures for multi-iteration Jacobi
		FRDGTextureRef DiffPing = nullptr;
		FRDGTextureRef DiffPong = nullptr;
		if (NumIterations > 1)
		{
			DiffPing = GraphBuilder.CreateTexture(IntermediateDesc, TEXT("WindField.DiffPing"));
			DiffPong = GraphBuilder.CreateTexture(IntermediateDesc, TEXT("WindField.DiffPong"));
		}

		for (int32 Iter = 0; Iter < NumIterations; ++Iter)
		{
			const bool bLastIter = (Iter == NumIterations - 1);

			// Determine input: first iteration reads ForcedTex, subsequent iterations read previous output
			FRDGTextureRef InputTex;
			if (Iter == 0)
				InputTex = ForcedTex;
			else
				InputTex = ((Iter % 2) == 1) ? DiffPing : DiffPong;

			// Determine output: last iteration writes to DiffusionOutputTarget, otherwise ping-pong
			FRDGTextureRef OutputTex;
			if (bLastIter)
				OutputTex = DiffusionOutputTarget;
			else
				OutputTex = ((Iter % 2) == 0) ? DiffPing : DiffPong;

			FWindFieldDiffusionCS::FParameters* Params = GraphBuilder.AllocParameters<FWindFieldDiffusionCS::FParameters>();
			Params->FieldOrigin = SceneData->WorldGridCenter - FVector3f(Config.WorldExtent) * 0.5f;
			Params->DeltaTime = SceneData->DeltaTime;
			Params->FieldExtent = FVector3f(Config.WorldExtent);
			Params->Viscosity = Config.Viscosity;
			Params->ResolutionX = Res.X;
			Params->ResolutionY = Res.Y;
			Params->ResolutionZ = Res.Z;
			Params->Padding0 = 0.0f;
			Params->InputField = GraphBuilder.CreateSRV(InputTex);
			Params->InputFieldSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Params->OutputField = GraphBuilder.CreateUAV(OutputTex);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("WindField.Diffusion %dx%dx%d [%d/%d]", Res.X, Res.Y, Res.Z, Iter + 1, NumIterations),
				ERDGPassFlags::Compute,
				DiffusionShader,
				Params,
				GroupCount);
		}
	}

	// ====================================================================
	// Pass 4: Composition — add directional wind to final output
	// Input:  DiffusedTex (simulation result)
	// Output: WindFieldTex (final double-buffered RT)
	// Only dispatched when directional wind is active.
	// ====================================================================
	if (bHasDirectionalWind)
	{
		TShaderMapRef<FWindFieldComposeCS> ComposeShader(ShaderMap);
		const FWindDirectionalData& DirData = SceneData->DirectionalData;

		FWindFieldComposeCS::FParameters* Params = GraphBuilder.AllocParameters<FWindFieldComposeCS::FParameters>();
		Params->FieldOrigin = SceneData->WorldGridCenter - FVector3f(Config.WorldExtent) * 0.5f;
		Params->Time = SceneData->CurrentTime;
		Params->FieldExtent = FVector3f(Config.WorldExtent);
		Params->Padding0 = 0.0f;
		Params->ResolutionX = Res.X;
		Params->ResolutionY = Res.Y;
		Params->ResolutionZ = Res.Z;
		Params->Padding1 = 0.0f;
		Params->DirectionalWindDirection = DirData.WindDirection;
		Params->DirectionalWindStrength = DirData.Strength;
		Params->DirectionalNoiseStrength = DirData.NoiseStrength;

		if (DirData.HasNoiseTexture())
		{
			Params->bHasDirectionalNoise = 1;
			Params->DirectionalNoiseTexture = DirData.NoiseTextureRHI;
			Params->DirectionalNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
			Params->DirNoiseTiling = DirData.Tiling;
			Params->DirNoiseIntensityMin = DirData.IntensityMin;
			Params->DirNoiseIntensityMax = DirData.IntensityMax;
			Params->DirNoiseScrollSpeed = DirData.ScrollSpeed;
		}
		else
		{
			Params->bHasDirectionalNoise = 0;
			Params->DirectionalNoiseTexture = GWhiteTexture->TextureRHI;
			Params->DirectionalNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
			Params->DirNoiseTiling = 1.0f;
			Params->DirNoiseIntensityMin = 0.0f;
			Params->DirNoiseIntensityMax = 1.0f;
			Params->DirNoiseScrollSpeed = 0.0f;
		}
		Params->Padding2 = 0.0f;

		Params->SimulationField = GraphBuilder.CreateSRV(DiffusedTex);
		Params->SimulationFieldSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Params->OutputField = GraphBuilder.CreateUAV(WindFieldTex);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("WindField.Compose %dx%dx%d", Res.X, Res.Y, Res.Z),
			ERDGPassFlags::Compute,
			ComposeShader,
			Params,
			GroupCount);
	}

	// Mark texture as globally readable for subsequent passes and material sampling
	GraphBuilder.UseExternalAccessMode(WindFieldTex, ERHIAccess::SRVMask, ERHIPipeline::All);

	// ====================================================================
	// Debug Pass: 3D Wind Field → 2D Slice Atlas
	// Renders all Y-slices of the wind field into a 2D grid texture.
	// ====================================================================
	if (CVarWindFieldDebugSlice.GetValueOnRenderThread() != 0)
	{
		// Compute atlas layout: slices arranged in a grid
		const uint32 SliceCols = FMath::CeilToInt32(FMath::Sqrt(static_cast<float>(Res.Z)));
		const uint32 SliceRows = FMath::DivideAndRoundUp(Res.Z, static_cast<int32>(SliceCols));
		const uint32 AtlasWidth  = SliceCols * Res.X;
		const uint32 AtlasHeight = SliceRows * Res.Y;

		// Ensure debug RT exists and matches size
		FPooledRenderTargetDesc DebugDesc = FPooledRenderTargetDesc::Create2DDesc(
			FIntPoint(AtlasWidth, AtlasHeight),
			PF_FloatRGBA,
			FClearValueBinding::Black,
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_UAV,
			false);

		if (!SceneData->DebugSliceRT
			|| SceneData->DebugSliceRT->GetDesc().Extent.X != static_cast<int32>(AtlasWidth)
			|| SceneData->DebugSliceRT->GetDesc().Extent.Y != static_cast<int32>(AtlasHeight))
		{
			SceneData->DebugSliceRT.SafeRelease();
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, DebugDesc,
				SceneData->DebugSliceRT, TEXT("WindField.DebugSlice"));
		}

		FRDGTextureRef DebugAtlasTex = GraphBuilder.RegisterExternalTexture(SceneData->DebugSliceRT);

		TShaderMapRef<FWindFieldDebugSliceCS> DebugShader(ShaderMap);
		FWindFieldDebugSliceCS::FParameters* DebugParams = GraphBuilder.AllocParameters<FWindFieldDebugSliceCS::FParameters>();
		DebugParams->VolumeResX = Res.X;
		DebugParams->VolumeResY = Res.Y;
		DebugParams->VolumeResZ = Res.Z;
		DebugParams->SliceCols = SliceCols;
		DebugParams->Padding0 = 0;
		DebugParams->Padding1 = 0;
		DebugParams->Padding2 = 0;
		DebugParams->WindFieldVolume = GraphBuilder.CreateSRV(WindFieldTex);
		DebugParams->WindFieldVolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		DebugParams->OutputAtlas = GraphBuilder.CreateUAV(DebugAtlasTex);

		const FIntVector DebugGroupCount(
			FMath::DivideAndRoundUp(AtlasWidth, 8u),
			FMath::DivideAndRoundUp(AtlasHeight, 8u),
			1);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("WindField.DebugSlice %dx%d (%d slices)", AtlasWidth, AtlasHeight, Res.Z),
			ERDGPassFlags::Compute,
			DebugShader,
			DebugParams,
			DebugGroupCount);

		GraphBuilder.UseExternalAccessMode(DebugAtlasTex, ERHIAccess::SRVMask, ERHIPipeline::All);
	}
	else
	{
		// Free debug RT when not in use
		SceneData->DebugSliceRT.SafeRelease();
	}

	// Flip double buffer for next frame
	SceneData->CurrentRTIndex = ReadIdx;
	SceneData->bNeedsUpdate = false;
}

void FWindFieldSceneExtension::FUpdater::PostSceneUpdate(
	FRDGBuilder& GraphBuilder,
	const FScenePostUpdateChangeSet& ChangeSet)
{
	(void)GraphBuilder;
	(void)ChangeSet;
}

// ============================================================================
// Scene Uniform Buffer — Wind Field Parameters
// ============================================================================

BEGIN_SHADER_PARAMETER_STRUCT(FWindFieldParameters, WINDSYSTEMRUNTIME_API)
	SHADER_PARAMETER(FVector3f, WindFieldOrigin)
	SHADER_PARAMETER(float, WindFieldPad0)
	SHADER_PARAMETER(FVector3f, WindFieldInvExtent)
	SHADER_PARAMETER(float, WindFieldPad1)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, WindFieldTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, WindFieldSampler)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, WindFieldDebugSlice)
	SHADER_PARAMETER_SAMPLER(SamplerState, WindFieldDebugSliceSampler)
END_SHADER_PARAMETER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FWindFieldParameters, WindField, WINDSYSTEMRUNTIME_API)

namespace WindField
{
	static void GetDefaultParameters(FWindFieldParameters& OutParams, FRDGBuilder& GraphBuilder)
	{
		OutParams.WindFieldOrigin = FVector3f::ZeroVector;
		OutParams.WindFieldPad0 = 0.0f;
		OutParams.WindFieldInvExtent = FVector3f::OneVector;
		OutParams.WindFieldPad1 = 0.0f;
		OutParams.WindFieldTexture = GraphBuilder.CreateSRV(GSystemTextures.GetWhiteDummy(GraphBuilder));
		OutParams.WindFieldSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutParams.WindFieldDebugSlice = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
		OutParams.WindFieldDebugSliceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}
}

IMPLEMENT_SCENE_UB_STRUCT(FWindFieldParameters, WindField, WindField::GetDefaultParameters);

// ============================================================================
// FRenderer — fill scene uniform buffer with wind field data
// ============================================================================

void FWindFieldSceneExtension::FRenderer::PreRender(FRDGBuilder& GraphBuilder)
{
}

void FWindFieldSceneExtension::FRenderer::UpdateSceneUniformBuffer(
	FRDGBuilder& GraphBuilder,
	FSceneUniformBuffer& Buffer)
{
	check(IsInRenderingThread());

	FWindFieldParameters Params;
	const FWindFieldConfig& Config = SceneData->CurrentConfig;

	FVector3f Origin = SceneData->WorldGridCenter - FVector3f(Config.WorldExtent) * 0.5f;
	FVector3f InvExtent(
		1.0f / FMath::Max((float)Config.WorldExtent.X, 1.0f),
		1.0f / FMath::Max((float)Config.WorldExtent.Y, 1.0f),
		1.0f / FMath::Max((float)Config.WorldExtent.Z, 1.0f));

	Params.WindFieldOrigin = Origin;
	Params.WindFieldPad0 = 0.0f;
	Params.WindFieldInvExtent = InvExtent;
	Params.WindFieldPad1 = 0.0f;

	// Use the most recently written RT for material sampling
	const int32 ReadIdx = 1 - SceneData->CurrentRTIndex;
	if (SceneData->WindFieldRT[ReadIdx])
	{
		Params.WindFieldTexture = GraphBuilder.CreateSRV(
			GraphBuilder.RegisterExternalTexture(SceneData->WindFieldRT[ReadIdx], TEXT("WindField.Current")));
	}
	else
	{
		Params.WindFieldTexture = GraphBuilder.CreateSRV(GSystemTextures.GetWhiteDummy(GraphBuilder));
	}
	Params.WindFieldSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// Debug slice atlas
	if (SceneData->DebugSliceRT)
	{
		Params.WindFieldDebugSlice = GraphBuilder.CreateSRV(
			GraphBuilder.RegisterExternalTexture(SceneData->DebugSliceRT, TEXT("WindField.DebugSlice")));
	}
	else
	{
		Params.WindFieldDebugSlice = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
	}
	Params.WindFieldDebugSliceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	Buffer.Set(SceneUB::WindField, Params);
}
