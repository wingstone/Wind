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

UE_DISABLE_OPTIMIZATION_SHIP

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
	PrevSimulationRT.SafeRelease();
	OutWindFieldRT.SafeRelease();
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

	// Snap the quantized grid center whenever it drifts more than half of the
	// world extent away from the incoming field center. This covers:
	//   1) PIE first frames where InCenter is temporarily (0,0,0) (Pawn/Camera
	//      not ready yet) — we simply do NOT snap, because WorldGridCenter
	//      already matches the editor-viewport-seeded value.
	//   2) Packaged / standalone cold start where the player spawns far from
	//      the world origin — first non-zero InCenter is >HalfExtent away, so
	//      we snap immediately and the volume covers the player.
	//   3) Teleports larger than the volume.
	// Small movements below the threshold are still handled incrementally by
	// UpdateScroll on the game thread.
	const FVector3f HalfExtent = FVector3f(InConfig.WorldExtent) * 0.5f;
	const FVector3f AbsDelta(
		FMath::Abs(InCenter.X - WorldGridCenter.X),
		FMath::Abs(InCenter.Y - WorldGridCenter.Y),
		FMath::Abs(InCenter.Z - WorldGridCenter.Z));
	if (!bGridCenterInitialized
		|| AbsDelta.X > HalfExtent.X
		|| AbsDelta.Y > HalfExtent.Y
		|| AbsDelta.Z > HalfExtent.Z)
	{
		// Only accept a non-zero seed to avoid latching onto an early PIE frame
		// where the view location is not yet available.
		if (!InCenter.IsZero())
		{
			WorldGridCenter = InCenter;
			bGridCenterInitialized = true;
			// A pending scroll offset was computed against the *old*
			// WorldGridCenter (often 0,0,0) — applying it after we've snapped
			// would double-count the delta and push the volume past the player.
			// Discard it; incremental scroll resumes cleanly next frame.
			GridScrollOffset = FIntVector::ZeroValue;
		}
	}

	CurrentTime = InTime;
	DeltaTime = InConfig.TimeStep;
	CurrentConfig = InConfig;
	DirectionalData = InDirectionalData;
	bNeedsUpdate = true;
}

void FWindFieldSceneExtension::SetScrollOffset_RenderThread(const FIntVector& NewGridScrollOffset)
{
	check(IsInRenderingThread());
	GridScrollOffset = NewGridScrollOffset;
}

void FWindFieldSceneExtension::GetRenderState_RenderThread(
	FRHITexture*& OutTexture,
	FVector3f& OutOrigin,
	FVector3f& OutInvExtent,
	FVector3f& OutDirectionalDirection,
	float& OutDirectionalStrength) const
{
	check(IsInRenderingThread());

	OutTexture = OutWindFieldRT ? OutWindFieldRT->GetRHI() : nullptr;

	OutOrigin = WorldGridCenter - FVector3f(CurrentConfig.WorldExtent) * 0.5f;
	OutInvExtent = FVector3f(
		1.0f / FMath::Max(static_cast<float>(CurrentConfig.WorldExtent.X), 1.0f),
		1.0f / FMath::Max(static_cast<float>(CurrentConfig.WorldExtent.Y), 1.0f),
		1.0f / FMath::Max(static_cast<float>(CurrentConfig.WorldExtent.Z), 1.0f));

	if (DirectionalData.IsValid())
	{
		OutDirectionalDirection = DirectionalData.WindDirection;
		OutDirectionalStrength = DirectionalData.Strength;
	}
	else
	{
		OutDirectionalDirection = FVector3f::ZeroVector;
		OutDirectionalStrength = 0.0f;
	}
}

// ============================================================================
// FUpdater — dispatch wind field compute shader
// ============================================================================

FRDGTextureRef FWindFieldSceneExtension::FUpdater::ApplyScroll_RenderThread(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef InField,
	const FRDGTextureDesc& TransientDesc)
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
		return InField;
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

	FRDGTextureRef Dest = GraphBuilder.CreateTexture(TransientDesc, TEXT("WindField.Scrolled"));

	FWindFieldScrollCS::FParameters* Params = GraphBuilder.AllocParameters<FWindFieldScrollCS::FParameters>();
	Params->ResolutionX = Res.X;
	Params->ResolutionY = Res.Y;
	Params->ResolutionZ = Res.Z;
	Params->ScrollOffsetX = TexelOffset.X;
	Params->ScrollOffsetY = TexelOffset.Y;
	Params->ScrollOffsetZ = TexelOffset.Z;
	Params->SourceVolume = GraphBuilder.CreateSRV(InField);
	Params->DestVolume = GraphBuilder.CreateUAV(Dest);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("WindField.Scroll"),
		ERDGPassFlags::Compute,
		ScrollCS,
		Params,
		GroupCount);

	return Dest;
}

void FWindFieldSceneExtension::FUpdater::PreSceneUpdate(
	FRDGBuilder& GraphBuilder,
	const FScenePreUpdateChangeSet& ChangeSet)
{
	const FWindFieldConfig& Config = SceneData->CurrentConfig;
	const FIntVector Res(
		FMath::Max(Config.Resolution.X, 1),
		FMath::Max(Config.Resolution.Y, 1),
		FMath::Max(Config.Resolution.Z, 1));

	// Persistent RTs:
	//   - PrevSimulationRT: previous-frame diffused result (no compose overlay), fed back into advection.
	//   - OutWindFieldRT:   final output with directional-wind compose overlay, sampled by materials.
	// All other RTs used by the simulation are allocated transiently below.
	FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::CreateVolumeDesc(
		Res.X, Res.Y, Res.Z,
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_None,
		TexCreate_ShaderResource | TexCreate_UAV,
		false);

	// If a persistent RT was previously created at a stale resolution
	// (e.g. this pass ran once with a default-constructed CurrentConfig before
	// SetSourceData_RenderThread delivered the real config), release it so the
	// pool re-allocates at the correct size. Without this the RT stays at the
	// first resolution for the entire session — visible as a 64^3 volume in
	// packaged builds when the intended resolution is 128x128x64.
	auto ResolutionMatches = [&Res](const TRefCountPtr<IPooledRenderTarget>& RT)
	{
		if (!RT) { return true; }
		const FIntVector RTSize = RT->GetDesc().GetSize();
		return RTSize.X == Res.X && RTSize.Y == Res.Y && RTSize.Z == Res.Z;
	};
	if (!ResolutionMatches(SceneData->PrevSimulationRT))
	{
		SceneData->PrevSimulationRT.SafeRelease();
	}
	if (!ResolutionMatches(SceneData->OutWindFieldRT))
	{
		SceneData->OutWindFieldRT.SafeRelease();
	}

	if (!SceneData->PrevSimulationRT)
	{
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc,
			SceneData->PrevSimulationRT, TEXT("WindField.PrevSimulation"));
	}
	if (!SceneData->OutWindFieldRT)
	{
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc,
			SceneData->OutWindFieldRT, TEXT("OutWindFieldRT"));
	}

	// Transient intermediate texture description reused for all in-flight fields.
	FRDGTextureDesc IntermediateDesc = FRDGTextureDesc::Create3D(
		FIntVector(Res.X, Res.Y, Res.Z),
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);

	// Register the previous-frame simulation state as the advection input.
	// Diffusion below will overwrite this same pooled RT with the new post-simulation
	// result, which then becomes next frame's advection input.
	FRDGTextureRef PrevSimTex = GraphBuilder.RegisterExternalTexture(SceneData->PrevSimulationRT);
	FRDGTextureRef PrevWindFieldTex = PrevSimTex;

	if (!SceneData->bNeedsUpdate || (SceneData->CurrentSources.Num() == 0 && !SceneData->DirectionalData.IsValid()))
	{
		// Skip both scroll and simulation this frame — scroll offset remains pending until the next simulated frame.
		return;
	}

	// Apply pending scroll to the previous frame before simulation.
	if (SceneData->GridScrollOffset != FIntVector::ZeroValue)
	{
		PrevWindFieldTex = ApplyScroll_RenderThread(GraphBuilder, PrevWindFieldTex, IntermediateDesc);

	}

	// All simulation intermediates are transient. PrevWindFieldTex (from OutWindFieldRT,
	// possibly scrolled above) is the previous-frame input for temporal advection.
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	FRDGTextureRef WindFieldTex = GraphBuilder.CreateTexture(IntermediateDesc, TEXT("WindField.Advected"));

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
	// Output: WindFieldTex
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
		Params->OutputField = GraphBuilder.CreateUAV(WindFieldTex);

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
	// Output: WindFieldTex
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
		Params->OutputField = GraphBuilder.CreateUAV(WindFieldTex);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("WindField.Force %dx%dx%d (%d sources)", Res.X, Res.Y, Res.Z, SourceCount),
			ERDGPassFlags::Compute,
			ForceShader,
			Params,
			GroupCount);
	}

	// Diffusion writes into the persistent PrevSimulationRT so it can be:
	//   1) read by the compose pass below to produce the final OutWindFieldRT this frame, and
	//   2) read by advection as the "previous frame" input next frame.
	// This is the SAME pooled RT as PrevSimTex above — RDG reorders reads/writes correctly
	// because advection has already been scheduled to read it before diffusion writes.
	FRDGTextureRef DiffusionOutputTarget = PrevSimTex;

	// ====================================================================
	// Pass 3: Diffusion
	// Input:  WindFieldTex (output of force injection)
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

			// Determine input: first iteration reads WindFieldTex, subsequent iterations read previous output
			FRDGTextureRef InputTex;
			if (Iter == 0)
				InputTex = WindFieldTex;
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
	// Output: OutWindFieldTex (final output texture)
	// Only dispatched when directional wind is active.
	// ====================================================================
	FRDGTextureRef OutWindFieldTex = GraphBuilder.RegisterExternalTexture(SceneData->OutWindFieldRT);
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

		Params->SimulationField = GraphBuilder.CreateSRV(DiffusionOutputTarget);
		Params->SimulationFieldSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Params->OutputField = GraphBuilder.CreateUAV(OutWindFieldTex);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("WindField.Compose %dx%dx%d", Res.X, Res.Y, Res.Z),
			ERDGPassFlags::Compute,
			ComposeShader,
			Params,
			GroupCount);
	}
	else
	{
		// If no directional wind, just copy diffused result to output
		AddCopyTexturePass(GraphBuilder, DiffusionOutputTarget, OutWindFieldTex);
	}
	
	// Mark texture as globally readable for subsequent passes and material sampling
	GraphBuilder.UseExternalAccessMode(OutWindFieldTex, ERHIAccess::SRVMask, ERHIPipeline::All);

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
		DebugParams->WindFieldVolume = GraphBuilder.CreateSRV(OutWindFieldTex);
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
	SHADER_PARAMETER(FVector3f, WindFieldDirectionalDirection)
	SHADER_PARAMETER(float, WindFieldDirectionalStrength)
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
		OutParams.WindFieldDirectionalDirection = FVector3f::ZeroVector;
		OutParams.WindFieldDirectionalStrength = 0.0f;
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

	// Directional wind data — direction (normalized) and strength for scene buffer consumers
	const FWindDirectionalData& DirData = SceneData->DirectionalData;
	if (DirData.IsValid())
	{
		Params.WindFieldDirectionalDirection = DirData.WindDirection;
		Params.WindFieldDirectionalStrength = DirData.Strength;
	}
	else
	{
		Params.WindFieldDirectionalDirection = FVector3f::ZeroVector;
		Params.WindFieldDirectionalStrength = 0.0f;
	}

	// Use the persistent output RT for material sampling
	if (SceneData->OutWindFieldRT)
	{
		Params.WindFieldTexture = GraphBuilder.CreateSRV(
			GraphBuilder.RegisterExternalTexture(SceneData->OutWindFieldRT, TEXT("WindField.Texture")));
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

UE_ENABLE_OPTIMIZATION_SHIP