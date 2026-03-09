// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterFieldSceneExtension.h"
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

	Scene = &InScene;
	UWaterSubsystem* SubSystem = UWaterSubsystem::GetSubsystem(Scene->GetWorld());
	SubSystem->Reset();
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

void FWaterFieldSceneExtension::AddInteraction_RenderThread(const FWaterInteractionData& Interaction)
{
	check(IsInRenderingThread());
	CurrentInteractions.Add(Interaction);
}

void FWaterFieldSceneExtension::ResetState_RenderThread(FRHICommandListImmediate& RHICmdList, bool bNewEnable)
{
	check(IsInRenderingThread());

	CurrentInteractions.Empty();
	CurrentConfig.SolverType = EFluidSolverType::ShallowWater;
	CurrentConfig.GridSize = 256;
	CurrentConfig.WorldSize = 10000.0f;
	CurrentConfig.Density = 1.0f;

	HeightFieldRT.SafeRelease();
	VelocityFieldRT.SafeRelease();
	TempHeightFieldRT.SafeRelease();
	TempVelocityFieldRT.SafeRelease();
	PressureFieldRT.SafeRelease();
	DivergenceFieldRT.SafeRelease();
}

// ============================================================================
// FUpdater — fetch game-thread data for GPU upload
// ============================================================================

void FWaterFieldSceneExtension::FUpdater::ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	if (!SceneData->HeightFieldRT[0] || !SceneData->HeightFieldRT[1] || !SceneData->HeightFieldRT[2] || !SceneData->VelocityFieldRT[0] || !SceneData->VelocityFieldRT[1])
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene->GetFeatureLevel());

	const FWaterFluidConfig& Config = SceneData->CurrentConfig;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);
	const FVector2f GridOrigin(-0.5f * Config.WorldSize, -0.5f * Config.WorldSize);
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(GridSize, GridSize), FIntPoint(16, 16));


	FRDGTextureRef HeightCurrent = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[SceneData->CurrentHeightIndex], TEXT("Water.Height.Current"));
	FRDGTextureRef HeightNext = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[(SceneData->CurrentHeightIndex + 1)%3], TEXT("Water.Height.Next"));
	FRDGTextureRef HeightPrevious = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[(SceneData->CurrentHeightIndex + 2)%3], TEXT("Water.Height.Previous"));
	FRDGTextureRef VelocityCurrent = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[SceneData->CurrentVelocityIndex], TEXT("Water.Velocity.Current"));
	FRDGTextureRef VelocityNext = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[(SceneData->CurrentVelocityIndex + 1)%2], TEXT("Water.Velocity.Next"));

	CurrentHeightIndex = (CurrentHeightIndex + 1) % 3;
	CurrentVelocityIndex = (CurrentVelocityIndex + 1) % 2;

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
		Parameters->SourceTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
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

void FWaterFieldSceneExtension::FUpdater::PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms)
{
	const FWaterFluidConfig& Config = SceneData->CurrentConfig;
	FIntPoint GridExtent(Config.GridSize, Config.GridSize);

	if (SceneData->CurrentConfig.SolverType == EFluidSolverType::ShallowWater)
	{
		for (uint32 i = 0; i < 3; i++)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->HeightFieldRT[i], TEXT("HeightFieldRT"));
		}

		for (uint32 i = 0; i < 2; i++)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->VelocityFieldRT[i], TEXT("VelocityFieldRT"));
		}
		
		ExecuteShallowWaterSolver_RenderThread(GraphBuilder);
	}
	else
	{
		ExecuteNavierStokesSolver_RenderThread(GraphBuilder);
	}
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
	FVector2f GridOffset = SceneData->WorldGridOrigin / SceneData->CurrentConfig.WorldSize;

	Parameters.UVScaleOffset = FVector4f(1.0f / SceneData->CurrentConfig.WorldSize, 1.0f / SceneData->CurrentConfig.WorldSize, GridOffset.X, GridOffset.Y);

	Parameters.WaterMapSizeAndInv = FVector4f(SceneData->CurrentConfig.GridSize, SceneData->CurrentConfig.GridSize, 1.0f / SceneData->CurrentConfig.GridSize, 1.0f / SceneData->CurrentConfig.GridSize);

	Parameters.WaterHeightMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[SceneData->CurrentHeightIndex], TEXT("Water.Height.Current")));
	Parameters.WaterHeightMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.WaterVelocityMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[SceneData->CurrentVelocityIndex], TEXT("Water.Velocity.Current")));
	Parameters.WaterVelocityMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	Buffer.Set(SceneUB::WaterSimulation, Parameters);
}