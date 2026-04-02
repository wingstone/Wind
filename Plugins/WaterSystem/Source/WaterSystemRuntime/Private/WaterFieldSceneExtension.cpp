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

static TAutoConsoleVariable<bool> CVarShallowWaterAdvection(
	TEXT("r.ShallowWater.Advection"),
	true,
	TEXT("Whether open shallow water advection"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarShallowWaterDiffusion(
	TEXT("r.ShallowWater.Diffusion"),
	true,
	TEXT("Whether open shallow water diffusion"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNavierStokesAdvection(
	TEXT("r.NavierStokes.Advection"),
	true,
	TEXT("Whether open Navier-Stokes advection"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNavierStokesDiffusion(
	TEXT("r.NavierStokes.Diffusion"),
	true,
	TEXT("Whether open Navier-Stokes diffusion"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNavierStokesProjection(
	TEXT("r.NavierStokes.Projection"),
	true,
	TEXT("Whether open Navier-Stokes projection"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNavierStokesVorticityConfinement(
	TEXT("r.NavierStokes.VorticityConfinement"),
	true,
	TEXT("Whether open Navier-Stokes vorticity confinement"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);


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
	WorldGridCenter = FVector2f::ZeroVector;
	GridScrollOffset = FIntVector2::ZeroValue;
	CurrentHeightIndex = 0;
	CurrentVelocityIndex = 0;
}

FWaterFieldSceneExtension::~FWaterFieldSceneExtension()
{
}

void FWaterFieldSceneExtension::InitExtension(FScene& InScene)
{	
	check(IsInGameThread());

	UWaterSubsystem* SubSystem = Scene.GetWorld()->GetSubsystem<UWaterSubsystem>();
	if (SubSystem)
	{
		SubSystem->ResetState();
	}
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

void FWaterFieldSceneExtension::SetInteractionsToApply_RenderThread(const TArray<FWaterInteractionData>& InteractionsToApply)
{
	check(IsInRenderingThread());
	CurrentInteractions.Append(InteractionsToApply);
}

void FWaterFieldSceneExtension::SetGlobalFlowData_RenderThread(const FWaterGlobalFlowData& InFlowData)
{
	check(IsInRenderingThread());
	CurrentGlobalFlowData = InFlowData;
}

void FWaterFieldSceneExtension::ResetState_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	CurrentInteractions.Empty();
	
	for (uint32 Index = 0; Index < UE_ARRAY_COUNT(HeightFieldRT); ++Index)
	{
		HeightFieldRT[Index].SafeRelease();
	}

	for (uint32 Index = 0; Index < UE_ARRAY_COUNT(VelocityFieldRT); ++Index)
	{
		VelocityFieldRT[Index].SafeRelease();
	}

	WorldGridCenter = FVector2f::ZeroVector;
	GridScrollOffset = FIntVector2::ZeroValue;
	CurrentHeightIndex = 0;
	CurrentVelocityIndex = 0;

	TempHeightFieldRT.SafeRelease();
	TempVelocityFieldRT.SafeRelease();
	NormalFieldRT.SafeRelease();
	PressureFieldRT.SafeRelease();
	TempPressureFieldRT.SafeRelease();
	DivergenceFieldRT.SafeRelease();
	VorticityFieldRT.SafeRelease();
	CurrentGlobalFlowData = FWaterGlobalFlowData();
}

// ============================================================================
// Helper
// ============================================================================

FORCEINLINE FPooledRenderTargetDesc CreateWindRenderTargetDesc(FIntPoint RenderTargetSize, EPixelFormat Format = EPixelFormat::PF_R16F)
{
	return FPooledRenderTargetDesc::Create2DDesc(
		RenderTargetSize,
		Format,
		FClearValueBinding::Black,
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
		return;
	}

	// Update world grid origin by the quantized amount
	SceneData->WorldGridCenter += FVector2f(static_cast<float>(TexelOffset.X), static_cast<float>(TexelOffset.Y)) * CellSize;
	SceneData->GridScrollOffset = FIntVector2::ZeroValue;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());
	const FIntPoint GridExtent(GridSize, GridSize);
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(GridExtent, FIntPoint(16, 16));

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

	// Scroll all 3 velocity field textures
	{
		const TShaderMapRef<FWaterScrollVelocityCS> ScrollCS(GlobalShaderMap);

		for (uint32 i = 0; i < 3; i++)
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

	// Scroll pressure field (NS solver uses it as warm-start for Jacobi iterations)
	if (SceneData->PressureFieldRT && SceneData->TempPressureFieldRT)
	{
		const TShaderMapRef<FWaterScrollHeightCS> ScrollCS(GlobalShaderMap);

		FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(SceneData->PressureFieldRT);
		FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(SceneData->TempPressureFieldRT);

		FWaterScrollHeightCS::FParameters* Params = GraphBuilder.AllocParameters<FWaterScrollHeightCS::FParameters>();
		Params->GridSize = GridSize;
		Params->ScrollTexelOffsetX = TexelOffset.X;
		Params->ScrollTexelOffsetY = TexelOffset.Y;
		Params->SourceTexture = GraphBuilder.CreateSRV(Source);
		Params->DestHeightTexture = GraphBuilder.CreateUAV(Dest);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Water.ScrollPressure"),
			ERDGPassFlags::Compute,
			ScrollCS,
			Params,
			GroupCount);

		Swap(SceneData->PressureFieldRT, SceneData->TempPressureFieldRT);
	}
	
	// Clear scroll offset after applying
	SceneData->GridScrollOffset = FIntVector2::ZeroValue;
}

void FWaterFieldSceneExtension::FUpdater::ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	if (!SceneData->HeightFieldRT[0] || !SceneData->HeightFieldRT[1] || !SceneData->HeightFieldRT[2] ||
		!SceneData->VelocityFieldRT[0] || !SceneData->VelocityFieldRT[1] || !SceneData->VelocityFieldRT[2])
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	const FWaterFluidConfig& Config = SceneData->CurrentConfig;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);
	const FVector2f GridOrigin(SceneData->WorldGridCenter - FVector2f(0.5f * Config.WorldSize));
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(GridSize, GridSize), FIntPoint(16, 16));


	FRDGTextureRef HeightRefs[3];
	FRDGTextureRef VelocityRefs[3];
	for (uint32 i = 0; i < 3; i++)
	{
		HeightRefs[i] = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i]);
		VelocityRefs[i] = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[i]);
	}
 
	// 1) Apply interaction impulses to current fields.
	if (SceneData->CurrentInteractions.Num() > 0)
	{
		FWaterInteractionApplicationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FWaterInteractionApplicationCS::FApplyVelocity >(false);
		auto InteractionCS = GlobalShaderMap->GetShader<FWaterInteractionApplicationCS>(PermutationVector);
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
			Parameters->InteractionPowerFalloff = FMath::Max(Interaction.PowerFalloff, 1e-4f);
			Parameters->InteractionHeightIntensity = Interaction.HeightIntensity;
			Parameters->InteractionShapeType = (1u << static_cast<uint32>(Interaction.ShapeType));
			Parameters->InteractionEmissionTypeMask = (1u << static_cast<uint32>(Interaction.EmissionType));
			Parameters->InteractionPosition = FVector2f(Interaction.Position);
			Parameters->InteractionDirection = FVector2f(Interaction.ForceDirection);
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightRefs[SceneData->CurrentHeightIndex]);
			Parameters->VelocityField = GraphBuilder.CreateUAV(VelocityRefs[SceneData->CurrentVelocityIndex]);

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

	// 2) Semi-Lagrangian advection for height.
	if (CVarShallowWaterAdvection.GetValueOnRenderThread())
	{
		for (int32 i = 0; i < Config.SimulationSubsteps; i++)
		{
			{
				const TShaderMapRef<FSWHeightAdvectionCS> AdvectionCS(GlobalShaderMap);
				FSWHeightAdvectionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSWHeightAdvectionCS::FParameters>();
				Parameters->GridSize = GridSize;
				Parameters->CellSize = CellSize;
				Parameters->GridOrigin = GridOrigin;
				Parameters->DeltaTime = Config.TimeStep;
				Parameters->AdvectionDamping = FMath::Clamp(Config.SW_AdvectionDamping, 0.0f, 1.0f);
				Parameters->FlowNoiseIntensityMin = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMin;
				Parameters->FlowNoiseIntensityMax = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMax;
				Parameters->FlowNoiseTiling = SceneData->CurrentGlobalFlowData.FlowNoiseTiling;
				Parameters->FlowDirection = FVector2f(SceneData->CurrentGlobalFlowData.FlowDirection);
				Parameters->UVScaleOffset = FVector4f(1.0f, 1.0f, GridOrigin.X/Config.WorldSize + 0.5f, GridOrigin.Y/Config.WorldSize + 0.5f);

				Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
				Parameters->FlowNoiseTexture = SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI != nullptr ? SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI : GBlackTexture->TextureRHI;
				Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
				Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
				Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightRefs[(SceneData->CurrentHeightIndex + 1) % 3]);
				Parameters->CurrentVelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Water.ShallowWater.Advection.Height0"),
					ERDGPassFlags::Compute,
					AdvectionCS,
					Parameters,
					GroupCount);
			}

			{
				const TShaderMapRef<FSWHeightAdvectionCS> AdvectionCS(GlobalShaderMap);
				FSWHeightAdvectionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSWHeightAdvectionCS::FParameters>();
				Parameters->GridSize = GridSize;
				Parameters->CellSize = CellSize;
				Parameters->GridOrigin = GridOrigin;
				Parameters->DeltaTime = Config.TimeStep;
				Parameters->AdvectionDamping = FMath::Clamp(Config.SW_AdvectionDamping, 0.0f, 1.0f);
				Parameters->FlowNoiseIntensityMin = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMin;
				Parameters->FlowNoiseIntensityMax = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMax;
				Parameters->FlowNoiseTiling = SceneData->CurrentGlobalFlowData.FlowNoiseTiling;
				Parameters->FlowDirection = FVector2f(SceneData->CurrentGlobalFlowData.FlowDirection);
				Parameters->UVScaleOffset = FVector4f(1.0f, 1.0f, GridOrigin.X/Config.WorldSize + 0.5f, GridOrigin.Y/Config.WorldSize + 0.5f);

				Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
				Parameters->FlowNoiseTexture = SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI != nullptr ? SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI : GBlackTexture->TextureRHI;
				Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
				Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightRefs[(SceneData->CurrentHeightIndex + 2) % 3]);
				Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightRefs[SceneData->CurrentHeightIndex]);
				Parameters->CurrentVelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Water.ShallowWater.Advection.Height1"),
					ERDGPassFlags::Compute,
					AdvectionCS,
					Parameters,
					GroupCount);
			}
	
			SceneData->CurrentHeightIndex = (SceneData->CurrentHeightIndex + 1) % 3;
		}
	}

	// 3) Height diffusion/wave integration step using current and previous heights.
	if (CVarShallowWaterDiffusion.GetValueOnRenderThread())
	{
		for (int32 i = 0; i < Config.SimulationSubsteps; i++)
		{
			TShaderMapRef<FSWDiffusionCS> DiffusionCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FSWDiffusionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSWDiffusionCS::FParameters>();
			Parameters->GridSize = GridSize;
			Parameters->CellSize = CellSize;
			Parameters->GridOrigin = GridOrigin;
			Parameters->Alpha = Config.SW_DiffusionAlpha;
			Parameters->Beta = Config.SW_DiffusionBeta;
			Parameters->DiffusionDamping = FMath::Clamp(Config.SW_DiffusionDamping, 0.0f, 1.0f);
			Parameters->PrevHeightField = GraphBuilder.CreateSRV(HeightRefs[(SceneData->CurrentHeightIndex + 2) % 3]);
			Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
			Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightRefs[(SceneData->CurrentHeightIndex + 1) % 3]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.ShallowWater.Diffusion"),
				ERDGPassFlags::Compute,
				DiffusionCS,
				Parameters,
				GroupCount);
				
			SceneData->CurrentHeightIndex = (SceneData->CurrentHeightIndex + 1) % 3;
		}

		// 将SceneData->HeightFieldRT[i]设为全局srv
		GraphBuilder.UseExternalAccessMode(HeightRefs[SceneData->CurrentHeightIndex], ERHIAccess::SRVMask, ERHIPipeline::All);
	}

	// 4) Convert solved height field to packed normal XY (RG16F).
	if (SceneData->NormalFieldRT)
	{
		const TShaderMapRef<FWaterHeightToNormalCS> HeightToNormalCS(GlobalShaderMap);
		FRDGTextureRef NormalOutput = GraphBuilder.RegisterExternalTexture(SceneData->NormalFieldRT, TEXT("Water.Normal.Current"));

		FWaterHeightToNormalCS::FParameters* Parameters = GraphBuilder.AllocParameters<FWaterHeightToNormalCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->HeightField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
		Parameters->OutNormalField = GraphBuilder.CreateUAV(NormalOutput);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Water.ShallowWater.HeightToNormal"),
			ERDGPassFlags::Compute,
			HeightToNormalCS,
			Parameters,
			GroupCount);
			
		GraphBuilder.UseExternalAccessMode(NormalOutput, ERHIAccess::SRVMask, ERHIPipeline::All);
	}
}

void FWaterFieldSceneExtension::FUpdater::ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	if (!SceneData->VelocityFieldRT[0] || !SceneData->VelocityFieldRT[1] || !SceneData->VelocityFieldRT[2] ||
		!SceneData->HeightFieldRT[0] || !SceneData->HeightFieldRT[1] || !SceneData->HeightFieldRT[2] ||
		!SceneData->PressureFieldRT || !SceneData->TempPressureFieldRT ||
		!SceneData->DivergenceFieldRT)
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	const FWaterFluidConfig& Config = SceneData->CurrentConfig;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);
	const FVector2f GridOrigin(SceneData->WorldGridCenter - FVector2f(0.5f * Config.WorldSize));
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(GridSize, GridSize), FIntPoint(16, 16));

	FRDGTextureRef HeightRefs[3];
	for (uint32 i = 0; i < 3; i++)
	{
		HeightRefs[i] = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i]);
	}
	FRDGTextureRef VelocityRefs[3];
	for (uint32 i = 0; i < 3; i++)
	{
		VelocityRefs[i] = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[i]);
	}
	FRDGTextureRef PressureRef = GraphBuilder.RegisterExternalTexture(SceneData->PressureFieldRT);
	FRDGTextureRef TempPressureRef = GraphBuilder.RegisterExternalTexture(SceneData->TempPressureFieldRT);
	FRDGTextureRef DivergenceRef = GraphBuilder.RegisterExternalTexture(SceneData->DivergenceFieldRT);
	
	// 0) Advection: VelocityRT[cur] → NextVelocityRT
	if (CVarNavierStokesAdvection.GetValueOnRenderThread())
	{
		const TShaderMapRef<FNSAdvectionCS> AdvectionCS(GlobalShaderMap);
		FNSAdvectionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSAdvectionCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->DeltaTime = Config.TimeStep;
		Parameters->CellSize = CellSize;
		Parameters->Damping = FMath::Clamp(Config.NS_Damping, 0.0f, 1.0f);
		Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);
		Parameters->OutVelocityField = GraphBuilder.CreateUAV(VelocityRefs[(SceneData->CurrentVelocityIndex + 1) % 3]);
		Parameters->DensityField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
		Parameters->OutDensityField = GraphBuilder.CreateUAV(HeightRefs[(SceneData->CurrentHeightIndex + 1) % 3]);
		
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Water.NS.Advection"),
			ERDGPassFlags::Compute,
			AdvectionCS,
			Parameters,
			GroupCount);
			
			SceneData->CurrentVelocityIndex = (SceneData->CurrentVelocityIndex + 1) % 3;
			SceneData->CurrentHeightIndex = (SceneData->CurrentHeightIndex + 1) % 3;
	}

	// 1) Apply interaction impulses
	if (SceneData->CurrentInteractions.Num() > 0)
	{
		FWaterInteractionApplicationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FWaterInteractionApplicationCS::FApplyVelocity >(true);
		auto InteractionCS = GlobalShaderMap->GetShader<FWaterInteractionApplicationCS>(PermutationVector);
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
			Parameters->InteractionPowerFalloff = FMath::Max(Interaction.PowerFalloff, 1e-4f);
			Parameters->InteractionHeightIntensity = Interaction.HeightIntensity;
			Parameters->InteractionShapeType = (1u << static_cast<uint32>(Interaction.ShapeType));
			Parameters->InteractionEmissionTypeMask = (1u << static_cast<uint32>(Interaction.EmissionType));
			Parameters->InteractionPosition = FVector2f(Interaction.Position);
			Parameters->InteractionDirection = FVector2f(Interaction.ForceDirection);
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightRefs[SceneData->CurrentHeightIndex]);
			Parameters->VelocityField = GraphBuilder.CreateUAV(VelocityRefs[SceneData->CurrentVelocityIndex]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.NS.Interaction"),
				ERDGPassFlags::Compute,
				InteractionCS,
				Parameters,
				GroupCount);
		}
	}
	SceneData->CurrentInteractions.Reset();

	// 1.5) Apply global flow force
	if (SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI)
	{
		const TShaderMapRef<FWaterGlobalFlowCS> GlobalFlowCS(GlobalShaderMap);
		FWaterGlobalFlowCS::FParameters* Parameters = GraphBuilder.AllocParameters<FWaterGlobalFlowCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->DeltaTime = Config.TimeStep;
		Parameters->FlowNoiseIntensityMin = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMin;
		Parameters->FlowNoiseIntensityMax = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMax;
		Parameters->FlowNoiseTiling = SceneData->CurrentGlobalFlowData.FlowNoiseTiling;
		Parameters->FlowDirection = FVector2f(SceneData->CurrentGlobalFlowData.FlowDirection);
		Parameters->FlowNoiseTexture = SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI;
		Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		Parameters->VelocityField = GraphBuilder.CreateUAV(VelocityRefs[SceneData->CurrentVelocityIndex]);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Water.NS.GlobalFlow"),
			ERDGPassFlags::Compute,
			GlobalFlowCS,
			Parameters,
			GroupCount);
	}

	// 2) Vorticity Confinement (GPU Gems): restore small-scale rotational detail
	//      Step A: compute scalar vorticity ω = curl(u)
	//      Step B: apply confinement force f = ε·h·(N × ω̂)
	if (Config.NS_VorticityConfinement > 0.0f && CVarNavierStokesVorticityConfinement.GetValueOnRenderThread())
	{
		// Ensure vorticity texture exists
		if (!SceneData->VorticityFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(FIntPoint(GridSize, GridSize)), SceneData->VorticityFieldRT, TEXT("VorticityFieldRT"));
		}
		FRDGTextureRef VorticityRef = GraphBuilder.RegisterExternalTexture(SceneData->VorticityFieldRT);

		// Step A: Compute vorticity
		{
			const TShaderMapRef<FNSComputeVorticityCS> ComputeVorticityCS(GlobalShaderMap);
			FNSComputeVorticityCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSComputeVorticityCS::FParameters>();
			Parameters->GridSize = GridSize;
			Parameters->CellSize = CellSize;
			Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);
			Parameters->OutVorticityField = GraphBuilder.CreateUAV(VorticityRef);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.NS.ComputeVorticity"),
				ERDGPassFlags::Compute,
				ComputeVorticityCS,
				Parameters,
				GroupCount);
		}

		// Step B: Apply confinement force
		{
			const TShaderMapRef<FNSVorticityConfinementCS> VorticityConfinementCS(GlobalShaderMap);
			FNSVorticityConfinementCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSVorticityConfinementCS::FParameters>();
			Parameters->GridSize = GridSize;
			Parameters->DeltaTime = Config.TimeStep;
			Parameters->CellSize = CellSize;
			Parameters->VorticityEpsilon = Config.NS_VorticityConfinement;
			Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);
			Parameters->VorticityField = GraphBuilder.CreateSRV(VorticityRef);
			Parameters->OutVelocityField = GraphBuilder.CreateUAV(VelocityRefs[(SceneData->CurrentVelocityIndex + 1) % 3]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.NS.VorticityConfinement"),
				ERDGPassFlags::Compute,
				VorticityConfinementCS,
				Parameters,
				GroupCount);

			SceneData->CurrentVelocityIndex = (SceneData->CurrentVelocityIndex + 1) % 3;
		}
	}

	// 2.5) Diffusion: Jacobi iterations (VelocityRT as source, ping-pong VelocityRTs)
	//    Solves implicit diffusion: (I - ν·Δt·∇²) u^{n+1} = u^n
	int32 NumDiffusionJacobiIterations = FMath::Max(Config.NS_NumDiffusionJacobiIterations, 1);
	NumDiffusionJacobiIterations -= NumDiffusionJacobiIterations % 2;
	if (NumDiffusionJacobiIterations > 0 && CVarNavierStokesDiffusion.GetValueOnRenderThread())
	{
		const TShaderMapRef<FNSDiffusionCS> DiffusionCS(GlobalShaderMap);

		// Arrange ping-pong so the final result lands in VelocityRefs[NextVelIdx]
		FRDGTextureRef DiffPing, DiffPong;
		FRDGTextureRef DensityDiffusionPing, DensityDiffusionPong;
		DiffPing = VelocityRefs[(SceneData->CurrentVelocityIndex + 1) % 3];
		DiffPong = VelocityRefs[(SceneData->CurrentVelocityIndex + 2) % 3];
		DensityDiffusionPing = HeightRefs[(SceneData->CurrentHeightIndex + 1) % 3];
		DensityDiffusionPong = HeightRefs[(SceneData->CurrentHeightIndex + 2) % 3];

		for (int32 i = 0; i < NumDiffusionJacobiIterations; i++)
		{
			FNSDiffusionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSDiffusionCS::FParameters>();
			Parameters->GridSize = GridSize;
			Parameters->DeltaTime = Config.TimeStep;
			Parameters->CellSize = CellSize;
			Parameters->Viscosity = Config.NS_Viscosity;
			Parameters->OriginalVelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);
			Parameters->VelocityField = GraphBuilder.CreateSRV((i == 0) ? VelocityRefs[SceneData->CurrentVelocityIndex] : DiffPing);
			Parameters->OutVelocityField = GraphBuilder.CreateUAV(DiffPong);
			Parameters->OriginalDensityField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
			Parameters->DensityField = GraphBuilder.CreateSRV((i == 0) ? HeightRefs[SceneData->CurrentHeightIndex] : DensityDiffusionPing);
			Parameters->OutDensityField = GraphBuilder.CreateUAV(DensityDiffusionPong);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.NS.Diffusion_%d", i),
				ERDGPassFlags::Compute,
				DiffusionCS,
				Parameters,
				GroupCount);

			Swap(DiffPing, DiffPong);
			Swap(DensityDiffusionPing, DensityDiffusionPong);
		}

		SceneData->CurrentVelocityIndex = (SceneData->CurrentVelocityIndex + 1) % 3;
		SceneData->CurrentHeightIndex = (SceneData->CurrentHeightIndex + 1) % 3;
	}

	// 3) Compute Divergence: VelocityRT[next] → DivergenceRT
	if (CVarNavierStokesProjection.GetValueOnRenderThread())
	{
		const TShaderMapRef<FNSComputeDivergenceCS> ComputeDivergenceCS(GlobalShaderMap);
		FNSComputeDivergenceCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSComputeDivergenceCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);
		Parameters->OutDivergenceField = GraphBuilder.CreateUAV(DivergenceRef);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Water.NS.ComputeDivergence"),
			ERDGPassFlags::Compute,
			ComputeDivergenceCS,
			Parameters,
			GroupCount);
	}

	// 4) Pressure Solve: Jacobi iterations (ping-pong PressureRT ↔ TempPressureRT)
	if (CVarNavierStokesProjection.GetValueOnRenderThread())
	{
		int32 NumPressureJacobiIterations = FMath::Max(Config.NS_NumPressureJacobiIterations, 1);
		NumPressureJacobiIterations -= NumPressureJacobiIterations % 2;
		const TShaderMapRef<FNSPressureSolveCS> PressureSolveCS(GlobalShaderMap);

		FRDGTextureRef PressurePing = PressureRef;
		FRDGTextureRef PressurePong = TempPressureRef;

		for (int32 i = 0; i < NumPressureJacobiIterations; i++)
		{
			FNSPressureSolveCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSPressureSolveCS::FParameters>();
			Parameters->GridSize = GridSize;
			Parameters->DeltaTime = Config.TimeStep;
			Parameters->CellSize = CellSize;
			Parameters->PressureField = GraphBuilder.CreateSRV(PressurePing);
			Parameters->DivergenceField = GraphBuilder.CreateSRV(DivergenceRef);
			Parameters->OutPressureField = GraphBuilder.CreateUAV(PressurePong);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Water.NS.PressureSolve_%d", i),
				ERDGPassFlags::Compute,
				PressureSolveCS,
				Parameters,
				GroupCount);

			Swap(PressurePing, PressurePong);
		}
	}

	// 5) Projection: VelocityRT[next] + PressureRT → VelocityRT[cur]
	if (CVarNavierStokesProjection.GetValueOnRenderThread())
	{
		// Re-register pressure after potential swap
		PressureRef = GraphBuilder.RegisterExternalTexture(SceneData->PressureFieldRT);
		const TShaderMapRef<FNSProjectionCS> ProjectionCS(GlobalShaderMap);
		FNSProjectionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSProjectionCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->DeltaTime = Config.TimeStep;
		Parameters->CellSize = CellSize;
		Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);
		Parameters->PressureField = GraphBuilder.CreateSRV(PressureRef);
		Parameters->OutVelocityField = GraphBuilder.CreateUAV(VelocityRefs[(SceneData->CurrentVelocityIndex + 1) % 3]);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Water.NS.Projection"),
			ERDGPassFlags::Compute,
			ProjectionCS,
			Parameters,
			GroupCount);
			
		SceneData->CurrentVelocityIndex = (SceneData->CurrentVelocityIndex + 1) % 3;
	}

	// 将SceneData->VelocityFieldRT[i]和SceneData->HeightFieldRT[i]设为全局srv
	GraphBuilder.UseExternalAccessMode(VelocityRefs[SceneData->CurrentVelocityIndex], ERHIAccess::SRVMask, ERHIPipeline::All);
	GraphBuilder.UseExternalAccessMode(HeightRefs[SceneData->CurrentHeightIndex], ERHIAccess::SRVMask, ERHIPipeline::All);
}

void FWaterFieldSceneExtension::FUpdater::PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms)
{
	const FWaterFluidConfig& Config = SceneData->CurrentConfig;
	FIntPoint GridExtent(Config.GridSize, Config.GridSize);

	if (SceneData->CurrentConfig.SolverType == EFluidSolverType::ShallowWater)
	{
		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->HeightFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->HeightFieldRT[i], TEXT("HeightFieldRT"));
				FRDGTextureRef HeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i], TEXT("Water.Height.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, HeightTexture, FLinearColor::Black);
			}	
		}

		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->VelocityFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->VelocityFieldRT[i], TEXT("VelocityFieldRT"));
				FRDGTextureRef VelocityTexture = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[i], TEXT("Water.Velocity.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, VelocityTexture, FLinearColor::Black);
			}
		}

		if (!SceneData->TempHeightFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->TempHeightFieldRT, TEXT("TempHeightFieldRT"));
			FRDGTextureRef TempHeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempHeightFieldRT, TEXT("Water.TempHeight.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempHeightTexture, FLinearColor::Black);
		}

		if (!SceneData->TempVelocityFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->TempVelocityFieldRT, TEXT("TempVelocityFieldRT"));
			FRDGTextureRef TempVelocityTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempVelocityFieldRT, TEXT("Water.TempVelocity.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempVelocityTexture, FLinearColor::Black);
		}

		if (!SceneData->NormalFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->NormalFieldRT, TEXT("NormalFieldRT"));
			FRDGTextureRef NormalTexture = GraphBuilder.RegisterExternalTexture(SceneData->NormalFieldRT, TEXT("Water.Normal.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, NormalTexture, FLinearColor::Black);
		}

		// Apply pending scroll before simulation
		if (SceneData->GridScrollOffset != FIntVector2::ZeroValue)
		{
			ApplyScroll_RenderThread(GraphBuilder);
		}

		ExecuteShallowWaterSolver_RenderThread(GraphBuilder);
	}
	else
	{
		// Navier-Stokes solver resources
		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->HeightFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->HeightFieldRT[i], TEXT("HeightFieldRT"));
				FRDGTextureRef HeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i], TEXT("Water.Height.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, HeightTexture, FLinearColor::Black);
			}
		}

		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->VelocityFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->VelocityFieldRT[i], TEXT("VelocityFieldRT"));
				FRDGTextureRef VelocityTexture = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[i], TEXT("Water.Velocity.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, VelocityTexture, FLinearColor::Black);
			}
		}
		
		if (!SceneData->TempHeightFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->TempHeightFieldRT, TEXT("TempHeightFieldRT"));
			FRDGTextureRef TempHeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempHeightFieldRT, TEXT("Water.TempHeight.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempHeightTexture, FLinearColor::Black);
		}

		if (!SceneData->TempVelocityFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->TempVelocityFieldRT, TEXT("TempVelocityFieldRT"));
			FRDGTextureRef TempVelocityTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempVelocityFieldRT, TEXT("Water.TempVelocity.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempVelocityTexture, FLinearColor::Black);
		}

		if (!SceneData->PressureFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->PressureFieldRT, TEXT("PressureFieldRT"));
			FRDGTextureRef PressureTexture = GraphBuilder.RegisterExternalTexture(SceneData->PressureFieldRT, TEXT("Water.Pressure.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, PressureTexture, FLinearColor::Black);
		}

		if (!SceneData->TempPressureFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->TempPressureFieldRT, TEXT("TempPressureFieldRT"));
			FRDGTextureRef TempPressureTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempPressureFieldRT, TEXT("Water.TempPressure.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempPressureTexture, FLinearColor::Black);
		}

		if (!SceneData->DivergenceFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->DivergenceFieldRT, TEXT("DivergenceFieldRT"));
			FRDGTextureRef DivergenceTexture = GraphBuilder.RegisterExternalTexture(SceneData->DivergenceFieldRT, TEXT("Water.Divergence.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, DivergenceTexture, FLinearColor::Black);
		}

		// Apply pending scroll before simulation
		if (SceneData->GridScrollOffset != FIntVector2::ZeroValue)
		{
			ApplyScroll_RenderThread(GraphBuilder);
		}

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
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, WaterNormalMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, WaterNormalMapTextureSampler)
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
		OutParameters.WaterNormalMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
		OutParameters.WaterNormalMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
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
	FVector2f UVOffset = FVector2f(0.5f) - SceneData->WorldGridCenter / SceneData->CurrentConfig.WorldSize;

	Parameters.UVScaleOffset = FVector4f(1.0f / SceneData->CurrentConfig.WorldSize, 1.0f / SceneData->CurrentConfig.WorldSize, UVOffset.X, UVOffset.Y);

	Parameters.WaterMapSizeAndInv = FVector4f(SceneData->CurrentConfig.GridSize, SceneData->CurrentConfig.GridSize, 1.0f / SceneData->CurrentConfig.GridSize, 1.0f / SceneData->CurrentConfig.GridSize);

	if (SceneData->HeightFieldRT[SceneData->CurrentHeightIndex])
	{
		Parameters.WaterHeightMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[SceneData->CurrentHeightIndex], TEXT("Water.Height.Current")));
	}
	else
	{
		Parameters.WaterHeightMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
	}
	Parameters.WaterHeightMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	if (SceneData->VelocityFieldRT[SceneData->CurrentVelocityIndex])
	{
		Parameters.WaterVelocityMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[SceneData->CurrentVelocityIndex], TEXT("Water.Velocity.Current")));
	}
	else
	{
		Parameters.WaterVelocityMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
	}
	Parameters.WaterVelocityMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	if (SceneData->NormalFieldRT)
	{
		Parameters.WaterNormalMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->NormalFieldRT, TEXT("Water.Normal.Current")));
	}
	else
	{
		Parameters.WaterNormalMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
	}
	Parameters.WaterNormalMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	Buffer.Set(SceneUB::WaterSimulation, Parameters);
}