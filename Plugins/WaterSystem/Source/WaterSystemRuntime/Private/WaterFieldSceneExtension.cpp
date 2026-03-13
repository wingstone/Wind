// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterFieldSceneExtension.h"
#include "WaterSubsystem.h"
#include "WaterFieldShaders.h"
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

	UWaterSubsystem* SubSystem = Scene.GetWorld()->GetSubsystem<UWaterSubsystem>();
	SubSystem->ResetState();
	UE_LOG(LogWaterFieldSceneExtension, Log, TEXT("WaterFieldSceneExtension initialized"));
}

ISceneExtensionUpdater* FWaterFieldSceneExtension::CreateUpdater()
{
	return new FUpdater(this);
}

ISceneExtensionRenderer* FWaterFieldSceneExtension::CreateRenderer(
	FSceneRendererBase& InSceneRenderer,
	const FEngineShowFlags& EngineShowFlags)
{
	return new FRenderer(InSceneRenderer, this);
}

void FWaterFieldSceneExtension::SetConfig_RenderThread(const FWaterFluidConfig& NewConfig)
{
	check(IsInRenderingThread());
	CurrentConfig = NewConfig;
}

void FWaterFieldSceneExtension::SetScrollOffset_RenderThread(const FIntVector2& NewGridScrollOffset)
{
	check(IsInRenderingThread());
	GridScrollOffset = NewGridScrollOffset;	
}

void FWaterFieldSceneExtension::SetPendingInteractions_RenderThread(const TArray<FWaterInteractionData>& PendingInteractions)
{
	check(IsInRenderingThread());
	CurrentInteractions.Append(PendingInteractions);
}

void FWaterFieldSceneExtension::ResetState_RenderThread(FRHICommandListImmediate& RHICmdList, bool bNewEnable)
{
	check(IsInRenderingThread());

	CurrentInteractions.Empty();
	CurrentConfig.SolverType = EFluidSolverType::ShallowWater;
	CurrentConfig.GridSize = 256;
	CurrentConfig.WorldSize = 10000.0f;
	CurrentConfig.Density = 1.0f;

	WorldGridOrigin = FVector2f::ZeroVector;
	GridScrollOffset = FIntVector2::ZeroValue;

	bEnableSimulation = bNewEnable;

	for (uint32 Index = 0; Index < UE_ARRAY_COUNT(HeightFieldRT); ++Index)
	{
		HeightFieldRT[Index].SafeRelease();
	}

	for (uint32 Index = 0; Index < UE_ARRAY_COUNT(VelocityFieldRT); ++Index)
	{
		VelocityFieldRT[Index].SafeRelease();
	}

	TempHeightFieldRT.SafeRelease();
	TempVelocityFieldRT.SafeRelease();
	PressureFieldRT.SafeRelease();
	DivergenceFieldRT.SafeRelease();
}

// ============================================================================
// Helper
// ============================================================================

FORCEINLINE FPooledRenderTargetDesc CreateWindRenderTargetDesc(FIntPoint RenderTargetSize, EPixelFormat Format = EPixelFormat::PF_R16F)
{
	return FPooledRenderTargetDesc::Create2DDesc(
		RenderTargetSize,
		Format,
		FClearValueBinding::None,
		TexCreate_None,
		TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV,
		false
	);
}

// ============================================================================
// FUpdater — fetch game-thread data for GPU upload
// ============================================================================

void FWaterFieldSceneExtension::FUpdater::ApplyScroll_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	const FWaterFluidConfig& Config = SceneData->CurrentConfig;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);

	FIntPoint TexelOffset(SceneData->GridScrollOffset.X, SceneData->GridScrollOffset.Y);

	if (TexelOffset.X == 0 && TexelOffset.Y == 0)
	{
		SceneData->GridScrollOffset = FIntVector2::ZeroValue;
		return;
	}

	// Update world grid origin by the quantized amount
	SceneData->WorldGridOrigin += FVector2f(static_cast<float>(TexelOffset.X), static_cast<float>(TexelOffset.Y)) * CellSize;
	SceneData->GridScrollOffset = FIntVector2::ZeroValue;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());
	const FIntPoint GridExtent(GridSize, GridSize);
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(GridExtent, FIntPoint(16, 16));

	// Ensure temp textures exist for the swap
	GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->TempHeightFieldRT, TEXT("TempHeightFieldRT"));
	GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->TempVelocityFieldRT, TEXT("TempVelocityFieldRT"));

	// Scroll all 3 height field textures
	{
		const TShaderMapRef<FWaterScrollHeightCS> ScrollCS(GlobalShaderMap);

		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->HeightFieldRT[i]) continue;

			FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i]);
			FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(SceneData->TempHeightFieldRT);

			FWaterScrollHeightCS::FParameters* Params = GraphBuilder.AllocParameters<FWaterScrollHeightCS::FParameters>();
			Params->GridSize = GridSize;
			Params->ScrollTexelOffsetX = TexelOffset.X;
			Params->ScrollTexelOffsetY = TexelOffset.Y;
			Params->SourceTexture = GraphBuilder.CreateSRV(Source);
			Params->DestHeightTexture = GraphBuilder.CreateUAV(Dest);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.ScrollHeight_%d", i),
				ERDGPassFlags::Compute,
				ScrollCS,
				Params,
				GroupCount);

			// Swap so HeightFieldRT[i] now points to the scrolled result
			Swap(SceneData->HeightFieldRT[i], SceneData->TempHeightFieldRT);
		}
	}

	// Scroll both velocity field textures
	{
		const TShaderMapRef<FWaterScrollVelocityCS> ScrollCS(GlobalShaderMap);

		for (uint32 i = 0; i < 2; i++)
		{
			if (!SceneData->VelocityFieldRT[i]) continue;

			FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[i]);
			FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(SceneData->TempVelocityFieldRT);

			FWaterScrollVelocityCS::FParameters* Params = GraphBuilder.AllocParameters<FWaterScrollVelocityCS::FParameters>();
			Params->GridSize = GridSize;
			Params->ScrollTexelOffsetX = TexelOffset.X;
			Params->ScrollTexelOffsetY = TexelOffset.Y;
			Params->SourceTexture = GraphBuilder.CreateSRV(Source);
			Params->DestVelocityTexture = GraphBuilder.CreateUAV(Dest);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.ScrollVelocity_%d", i),
				ERDGPassFlags::Compute,
				ScrollCS,
				Params,
				GroupCount);

			Swap(SceneData->VelocityFieldRT[i], SceneData->TempVelocityFieldRT);
		}
	}
}

void FWaterFieldSceneExtension::FUpdater::ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	if (!SceneData->HeightFieldRT[0] || !SceneData->HeightFieldRT[1] || !SceneData->HeightFieldRT[2] || !SceneData->VelocityFieldRT[0] || !SceneData->VelocityFieldRT[1])
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	const FWaterFluidConfig& Config = SceneData->CurrentConfig;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);
	const FVector2f GridOrigin(SceneData->WorldGridOrigin - FVector2f(0.5f * Config.WorldSize));
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(GridSize, GridSize), FIntPoint(16, 16));


	FRDGTextureRef HeightCurrent = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[SceneData->CurrentHeightIndex], TEXT("Water.Height.Current"));
	FRDGTextureRef HeightNext = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[(SceneData->CurrentHeightIndex + 1)%3], TEXT("Water.Height.Next"));
	FRDGTextureRef HeightPrevious = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[(SceneData->CurrentHeightIndex + 2)%3], TEXT("Water.Height.Previous"));
	FRDGTextureRef VelocityCurrent = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[SceneData->CurrentVelocityIndex], TEXT("Water.Velocity.Current"));
	FRDGTextureRef VelocityNext = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[(SceneData->CurrentVelocityIndex + 1)%2], TEXT("Water.Velocity.Next"));

	SceneData->CurrentHeightIndex = (SceneData->CurrentHeightIndex + 1) % 3;
	SceneData->CurrentVelocityIndex = (SceneData->CurrentVelocityIndex + 1) % 2;

	// 1) Apply interaction impulses to current fields.
	if (SceneData->CurrentInteractions.Num() > 0)
	{
		const TShaderMapRef<FWaterInteractionApplicationCS> InteractionCS(GlobalShaderMap);
		for (const FWaterInteractionData& Interaction : SceneData->CurrentInteractions)
		{
			FWaterInteractionApplicationCS::FParameters* Parameters = GraphBuilder.AllocParameters<FWaterInteractionApplicationCS::FParameters>();
			Parameters->GridSize = GridSize;
			Parameters->CellSize = CellSize;
			Parameters->GridOrigin = GridOrigin;
			Parameters->InteractionDirectionalStrength = Interaction.StrengthParameter.X;
			Parameters->InteractionOmniStrength = Interaction.StrengthParameter.Y;
			Parameters->InteractionVortexStrength = Interaction.StrengthParameter.Z;
			Parameters->InteractionRadius = Interaction.RadiusParameter.X;
			Parameters->InteractionRadiusWidth = Interaction.RadiusParameter.Y;
			Parameters->InteractionGaussianFalloff = FMath::Max(Interaction.GaussianFalloff, 1e-4f);
			Parameters->InteractionShapeType = (1u << static_cast<uint32>(Interaction.ShapeType));
			Parameters->InteractionEmissionTypeMask = (1u << static_cast<uint32>(Interaction.EmissionType));
			Parameters->InteractionPosition = FVector2f(Interaction.Position);
			Parameters->InteractionDirection = FVector2f(Interaction.ForceDirection);
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightCurrent);
			Parameters->VelocityField = GraphBuilder.CreateUAV(VelocityCurrent);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.Interaction"),
				ERDGPassFlags::Compute,
				InteractionCS,
				Parameters,
				GroupCount);
		}
	}
	SceneData->CurrentInteractions.Reset();

	// 2) Semi-Lagrangian advection for height + velocity.
	{
		const TShaderMapRef<FSWAdvectionCS> AdvectionCS(GlobalShaderMap);
		FSWAdvectionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSWAdvectionCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->GridOrigin = GridOrigin;
		Parameters->DeltaTime = Config.TimeStep;
		Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightCurrent);
		Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightNext);
		Parameters->CurrentVelocityField = GraphBuilder.CreateSRV(VelocityCurrent);
		Parameters->NextVelocityField = GraphBuilder.CreateUAV(VelocityNext);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Water.ShallowWater.Advection"),
			ERDGPassFlags::Compute,
			AdvectionCS,
			Parameters,
			GroupCount);
	}

	// 3) Height diffusion/wave integration step using current and previous heights.
	{
		TShaderMapRef<FSWDiffusionCS> DiffusionCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSWDiffusionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSWDiffusionCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->GridOrigin = GridOrigin;
		Parameters->Alpha = Config.SurfaceTension;
		Parameters->Beta = Config.TimeStep;
		Parameters->Damping = 1.0f - FMath::Clamp(Config.Damping, 0.0f, 1.0f);
		Parameters->PrevHeightField = GraphBuilder.CreateSRV(HeightPrevious);
		Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightCurrent);
		Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightNext);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Water.ShallowWater.Diffusion"),
			ERDGPassFlags::Compute,
			DiffusionCS,
			Parameters,
			GroupCount);
	}
}
void FWaterFieldSceneExtension::FUpdater::ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	// TODO: Implement Navier-Stokes solver dispatch
	// Requires shader conversion to texture-based operations
	UE_LOG(LogWaterFieldSceneExtension, VeryVerbose, TEXT("Navier-Stokes solver (stub)"));
}

void FWaterFieldSceneExtension::FUpdater::PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms)
{
	const FWaterFluidConfig& Config = SceneData->CurrentConfig;
	FIntPoint GridExtent(Config.GridSize, Config.GridSize);

	if (SceneData->CurrentConfig.SolverType == EFluidSolverType::ShallowWater)
	{
		const bool bTexturesExist = SceneData->HeightFieldRT[0].IsValid();

		for (uint32 i = 0; i < 3; i++)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->HeightFieldRT[i], TEXT("HeightFieldRT"));
		}

		for (uint32 i = 0; i < 2; i++)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->VelocityFieldRT[i], TEXT("VelocityFieldRT"));
		}

		// Apply pending scroll before simulation
		if (SceneData->GridScrollOffset != FIntVector2::ZeroValue)
		{
			if (bTexturesExist)
			{
				ApplyScroll_RenderThread(GraphBuilder);
			}
			else
			{
				// Textures just created — no data to scroll, just update origin
				const float CellSize = Config.WorldSize / static_cast<float>(FMath::Max(Config.GridSize, 1));
				FIntPoint TexelOffset(SceneData->GridScrollOffset.X, SceneData->GridScrollOffset.Y);
				SceneData->WorldGridOrigin += FVector2f(static_cast<float>(TexelOffset.X), static_cast<float>(TexelOffset.Y)) * CellSize;
				SceneData->GridScrollOffset = FIntVector2::ZeroValue;
			}
		}

		ExecuteShallowWaterSolver_RenderThread(GraphBuilder);
	}
	else
	{
		ExecuteNavierStokesSolver_RenderThread(GraphBuilder);
	}
}

void FWaterFieldSceneExtension::FUpdater::PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet)
{
	(void)GraphBuilder;
	(void)ChangeSet;
}

// -------------------------- Render -----------------------//


BEGIN_SHADER_PARAMETER_STRUCT(FWaterSimulationParameters, WATERSYSTEMRUNTIME_API)
	SHADER_PARAMETER(FVector4f, UVScaleOffset)
	SHADER_PARAMETER(FVector4f, WaterMapSizeAndInv)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, WaterHeightMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, WaterHeightMapTextureSampler)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, WaterVelocityMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, WaterVelocityMapTextureSampler)
END_SHADER_PARAMETER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FWaterSimulationParameters, WaterSimulation, WATERSYSTEMRUNTIME_API)

namespace WaterSimulation
{
	static void GetDefaultParameters(FWaterSimulationParameters& OutParameters, FRDGBuilder& GraphBuilder)
	{
		OutParameters.UVScaleOffset = FVector4f::One();
		OutParameters.WaterMapSizeAndInv = FVector4f::One();
		OutParameters.WaterHeightMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
		OutParameters.WaterHeightMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutParameters.WaterVelocityMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
		OutParameters.WaterVelocityMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}
}

IMPLEMENT_SCENE_UB_STRUCT(FWaterSimulationParameters, WaterSimulation, WaterSimulation::GetDefaultParameters);

void FWaterFieldSceneExtension::FRenderer::PreRender(FRDGBuilder& GraphBuilder)
{
}

void FWaterFieldSceneExtension::FRenderer::UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& Buffer)
{
	check(IsInRenderingThread());

	FWaterSimulationParameters Parameters;
	// UV = (WorldPos - GridBottomLeft) / WorldSize
	//    = WorldPos / WorldSize - WorldGridOrigin / WorldSize + 0.5
	FVector2f UVOffset = FVector2f(0.5f) - SceneData->WorldGridOrigin / SceneData->CurrentConfig.WorldSize;

	Parameters.UVScaleOffset = FVector4f(1.0f / SceneData->CurrentConfig.WorldSize, 1.0f / SceneData->CurrentConfig.WorldSize, UVOffset.X, UVOffset.Y);

	Parameters.WaterMapSizeAndInv = FVector4f(SceneData->CurrentConfig.GridSize, SceneData->CurrentConfig.GridSize, 1.0f / SceneData->CurrentConfig.GridSize, 1.0f / SceneData->CurrentConfig.GridSize);

	Parameters.WaterHeightMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[SceneData->CurrentHeightIndex], TEXT("Water.Height.Current")));
	Parameters.WaterHeightMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.WaterVelocityMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[SceneData->CurrentVelocityIndex], TEXT("Water.Velocity.Current")));
	Parameters.WaterVelocityMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	Buffer.Set(SceneUB::WaterSimulation, Parameters);
}