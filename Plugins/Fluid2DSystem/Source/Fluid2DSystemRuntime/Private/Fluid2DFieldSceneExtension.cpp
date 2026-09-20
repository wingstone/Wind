// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DFieldSceneExtension.h"
#include "Fluid2DSubsystem.h"
#include "Fluid2DFieldShaders.h"
//#include "Fluid2DFieldProxy.h"
#include "ScenePrivate.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "PixelShaderUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogFluid2DFieldSceneExtension, Log, All);

IMPLEMENT_SCENE_EXTENSION(FFluid2DFieldSceneExtension);

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
// FFluid2DFieldSceneExtension
// ============================================================================

bool FFluid2DFieldSceneExtension::ShouldCreateExtension(FScene& Scene)
{
	return Scene.GetWorld() != nullptr;
}

FFluid2DFieldSceneExtension::FFluid2DFieldSceneExtension(FScene& InScene)
	: ISceneExtension(InScene)
{
	WorldGridCenter = FVector2f::ZeroVector;
	GridScrollOffset = FIntVector2::ZeroValue;
	CurrentHeightIndex = 0;
	CurrentVelocityIndex = 0;
}

FFluid2DFieldSceneExtension::~FFluid2DFieldSceneExtension()
{
}

void FFluid2DFieldSceneExtension::InitExtension(FScene& InScene)
{	
	check(IsInGameThread());

	UFluid2DSubsystem* SubSystem = Scene.GetWorld()->GetSubsystem<UFluid2DSubsystem>();
	if (SubSystem)
	{
		SubSystem->ResetState();
	}
	UE_LOG(LogFluid2DFieldSceneExtension, Log, TEXT("Fluid2DFieldSceneExtension initialized"));
}

ISceneExtensionUpdater* FFluid2DFieldSceneExtension::CreateUpdater()
{
	return new FUpdater(this);
}

ISceneExtensionRenderer* FFluid2DFieldSceneExtension::CreateRenderer(
	FSceneRendererBase& InSceneRenderer,
	const FEngineShowFlags& EngineShowFlags)
{
	return new FRenderer(InSceneRenderer, this);
}

void FFluid2DFieldSceneExtension::SetConfig_RenderThread(const FFluid2DFluidConfig& NewConfig)
{
	check(IsInRenderingThread());
	CurrentConfig = NewConfig;
}

void FFluid2DFieldSceneExtension::SetScrollOffset_RenderThread(const FIntVector2& NewGridScrollOffset)
{
	check(IsInRenderingThread());
	GridScrollOffset = NewGridScrollOffset;	
}

void FFluid2DFieldSceneExtension::SetInteractionsToApply_RenderThread(const TArray<FFluid2DInteractionData>& InteractionsToApply)
{
	check(IsInRenderingThread());
	CurrentInteractions.Append(InteractionsToApply);
}

void FFluid2DFieldSceneExtension::SetGlobalFlowData_RenderThread(const FFluid2DGlobalFlowData& InFlowData)
{
	check(IsInRenderingThread());
	CurrentGlobalFlowData = InFlowData;
}

void FFluid2DFieldSceneExtension::ResetState_RenderThread(FRHICommandListImmediate& RHICmdList)
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
	OutputVelocityFieldRT.SafeRelease();
	CurrentGlobalFlowData = FFluid2DGlobalFlowData();
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

void FFluid2DFieldSceneExtension::FUpdater::ApplyScroll_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	const FFluid2DFluidConfig& Config = SceneData->CurrentConfig;
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
		const TShaderMapRef<FFluid2DScrollHeightCS> ScrollCS(GlobalShaderMap);

		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->HeightFieldRT[i] || !SceneData->TempHeightFieldRT) continue;

			FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i]);
			FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(SceneData->TempHeightFieldRT);

			FFluid2DScrollHeightCS::FParameters* Params = GraphBuilder.AllocParameters<FFluid2DScrollHeightCS::FParameters>();
			Params->GridSize = GridSize;
			Params->ScrollTexelOffsetX = TexelOffset.X;
			Params->ScrollTexelOffsetY = TexelOffset.Y;
			Params->SourceTexture = GraphBuilder.CreateSRV(Source);
			Params->DestHeightTexture = GraphBuilder.CreateUAV(Dest);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.ScrollHeight_%d", i),
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
		const TShaderMapRef<FFluid2DScrollVelocityCS> ScrollCS(GlobalShaderMap);

		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->VelocityFieldRT[i] || !SceneData->TempVelocityFieldRT) continue;

			FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[i]);
			FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(SceneData->TempVelocityFieldRT);

			FFluid2DScrollVelocityCS::FParameters* Params = GraphBuilder.AllocParameters<FFluid2DScrollVelocityCS::FParameters>();
			Params->GridSize = GridSize;
			Params->ScrollTexelOffsetX = TexelOffset.X;
			Params->ScrollTexelOffsetY = TexelOffset.Y;
			Params->SourceTexture = GraphBuilder.CreateSRV(Source);
			Params->DestVelocityTexture = GraphBuilder.CreateUAV(Dest);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.ScrollVelocity_%d", i),
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
		const TShaderMapRef<FFluid2DScrollHeightCS> ScrollCS(GlobalShaderMap);

		FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(SceneData->PressureFieldRT);
		FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(SceneData->TempPressureFieldRT);

		FFluid2DScrollHeightCS::FParameters* Params = GraphBuilder.AllocParameters<FFluid2DScrollHeightCS::FParameters>();
		Params->GridSize = GridSize;
		Params->ScrollTexelOffsetX = TexelOffset.X;
		Params->ScrollTexelOffsetY = TexelOffset.Y;
		Params->SourceTexture = GraphBuilder.CreateSRV(Source);
		Params->DestHeightTexture = GraphBuilder.CreateUAV(Dest);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.ScrollPressure"),
			ERDGPassFlags::Compute,
			ScrollCS,
			Params,
			GroupCount);

		Swap(SceneData->PressureFieldRT, SceneData->TempPressureFieldRT);
	}
	
	// Clear scroll offset after applying
	SceneData->GridScrollOffset = FIntVector2::ZeroValue;
}

void FFluid2DFieldSceneExtension::FUpdater::ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	if (!SceneData->HeightFieldRT[0] || !SceneData->HeightFieldRT[1] || !SceneData->HeightFieldRT[2] ||
		!SceneData->VelocityFieldRT[0] || !SceneData->VelocityFieldRT[1] || !SceneData->VelocityFieldRT[2])
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	const FFluid2DFluidConfig& Config = SceneData->CurrentConfig;
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
		FFluid2DInteractionApplicationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FFluid2DInteractionApplicationCS::FApplyVelocity >(false);
		auto InteractionCS = GlobalShaderMap->GetShader<FFluid2DInteractionApplicationCS>(PermutationVector);
		for (const FFluid2DInteractionData& Interaction : SceneData->CurrentInteractions)
		{
			FFluid2DInteractionApplicationCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DInteractionApplicationCS::FParameters>();
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
			Parameters->InteractionPrevPosition = FVector2f(Interaction.PrevPosition);
			Parameters->InteractionDirection = FVector2f(Interaction.ForceDirection);
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightRefs[SceneData->CurrentHeightIndex]);
			Parameters->VelocityField = GraphBuilder.CreateUAV(VelocityRefs[SceneData->CurrentVelocityIndex]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.Interaction"),
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
					RDG_EVENT_NAME("Fluid2D.ShallowWater.Advection.Height0"),
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
					RDG_EVENT_NAME("Fluid2D.ShallowWater.Advection.Height1"),
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
				RDG_EVENT_NAME("Fluid2D.ShallowWater.Diffusion"),
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
		const TShaderMapRef<FFluid2DHeightToNormalCS> HeightToNormalCS(GlobalShaderMap);
		FRDGTextureRef NormalOutput = GraphBuilder.RegisterExternalTexture(SceneData->NormalFieldRT, TEXT("Fluid2D.Normal.Current"));

		FFluid2DHeightToNormalCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DHeightToNormalCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->HeightField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
		Parameters->OutNormalField = GraphBuilder.CreateUAV(NormalOutput);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.ShallowWater.HeightToNormal"),
			ERDGPassFlags::Compute,
			HeightToNormalCS,
			Parameters,
			GroupCount);
			
		GraphBuilder.UseExternalAccessMode(NormalOutput, ERHIAccess::SRVMask, ERHIPipeline::All);
	}
}

void FFluid2DFieldSceneExtension::FUpdater::ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder)
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

	const FFluid2DFluidConfig& Config = SceneData->CurrentConfig;
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
		Parameters->FlowNoiseIntensityMin = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMin;
		Parameters->FlowNoiseIntensityMax = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMax;
		Parameters->FlowNoiseTiling = SceneData->CurrentGlobalFlowData.FlowNoiseTiling;
		Parameters->FlowDirection = FVector2f(SceneData->CurrentGlobalFlowData.FlowDirection);
		Parameters->UVScaleOffset = FVector4f(1.0f, 1.0f, GridOrigin.X/Config.WorldSize + 0.5f, GridOrigin.Y/Config.WorldSize + 0.5f);
		Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		Parameters->FlowNoiseTexture = SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI != nullptr ? SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI : GBlackTexture->TextureRHI;
		Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);
		Parameters->OutVelocityField = GraphBuilder.CreateUAV(VelocityRefs[(SceneData->CurrentVelocityIndex + 1) % 3]);
		Parameters->DensityField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
		Parameters->OutDensityField = GraphBuilder.CreateUAV(HeightRefs[(SceneData->CurrentHeightIndex + 1) % 3]);
		
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.NS.Advection"),
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
		FFluid2DInteractionApplicationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FFluid2DInteractionApplicationCS::FApplyVelocity >(true);
		auto InteractionCS = GlobalShaderMap->GetShader<FFluid2DInteractionApplicationCS>(PermutationVector);
		for (const FFluid2DInteractionData& Interaction : SceneData->CurrentInteractions)
		{
			FFluid2DInteractionApplicationCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DInteractionApplicationCS::FParameters>();
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
			Parameters->InteractionPrevPosition = FVector2f(Interaction.PrevPosition);
			Parameters->InteractionDirection = FVector2f(Interaction.ForceDirection);
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightRefs[SceneData->CurrentHeightIndex]);
			Parameters->VelocityField = GraphBuilder.CreateUAV(VelocityRefs[SceneData->CurrentVelocityIndex]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.NS.Interaction"),
				ERDGPassFlags::Compute,
				InteractionCS,
				Parameters,
				GroupCount);
		}
	}
	SceneData->CurrentInteractions.Reset();

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
				RDG_EVENT_NAME("Fluid2D.NS.ComputeVorticity"),
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
				RDG_EVENT_NAME("Fluid2D.NS.VorticityConfinement"),
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
				RDG_EVENT_NAME("Fluid2D.NS.Diffusion_%d", i),
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
			RDG_EVENT_NAME("Fluid2D.NS.ComputeDivergence"),
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
				RDG_EVENT_NAME("Fluid2D.NS.PressureSolve_%d", i),
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
			RDG_EVENT_NAME("Fluid2D.NS.Projection"),
			ERDGPassFlags::Compute,
			ProjectionCS,
			Parameters,
			GroupCount);
			
		SceneData->CurrentVelocityIndex = (SceneData->CurrentVelocityIndex + 1) % 3;
	}

	// 6) Compose output: simulation velocity + global flow noise → OutputVelocityFieldRT
	{
		if (!SceneData->OutputVelocityFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(FIntPoint(GridSize, GridSize), EPixelFormat::PF_G16R16F), SceneData->OutputVelocityFieldRT, TEXT("OutputVelocityFieldRT"));
			FRDGTextureRef OutputVelTexture = GraphBuilder.RegisterExternalTexture(SceneData->OutputVelocityFieldRT, TEXT("Fluid2D.OutputVelocity.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, OutputVelTexture, FLinearColor::Black);
		}
		FRDGTextureRef OutputVelocityRef = GraphBuilder.RegisterExternalTexture(SceneData->OutputVelocityFieldRT);

		const TShaderMapRef<FFluid2DComposeVelocityCS> ComposeCS(GlobalShaderMap);
		FFluid2DComposeVelocityCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DComposeVelocityCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->FlowNoiseIntensityMin = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMin;
		Parameters->FlowNoiseIntensityMax = SceneData->CurrentGlobalFlowData.FlowNoiseIntensityMax;
		Parameters->FlowNoiseTiling = SceneData->CurrentGlobalFlowData.FlowNoiseTiling;
		Parameters->FlowDirection = FVector2f(SceneData->CurrentGlobalFlowData.FlowDirection);
		Parameters->UVScaleOffset = FVector4f(1.0f, 1.0f, GridOrigin.X/Config.WorldSize + 0.5f, GridOrigin.Y/Config.WorldSize + 0.5f);
		Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		Parameters->FlowNoiseTexture = SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI != nullptr ? SceneData->CurrentGlobalFlowData.FlowNoiseTextureRHI : GBlackTexture->TextureRHI;
		Parameters->SourceVelocityField = GraphBuilder.CreateSRV(VelocityRefs[SceneData->CurrentVelocityIndex]);
		Parameters->OutVelocityField = GraphBuilder.CreateUAV(OutputVelocityRef);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.NS.ComposeOutputVelocity"),
			ERDGPassFlags::Compute,
			ComposeCS,
			Parameters,
			GroupCount);

		GraphBuilder.UseExternalAccessMode(OutputVelocityRef, ERHIAccess::SRVMask, ERHIPipeline::All);
	}

	// 将SceneData->VelocityFieldRT[i]和SceneData->HeightFieldRT[i]设为全局srv
	GraphBuilder.UseExternalAccessMode(VelocityRefs[SceneData->CurrentVelocityIndex], ERHIAccess::SRVMask, ERHIPipeline::All);
	GraphBuilder.UseExternalAccessMode(HeightRefs[SceneData->CurrentHeightIndex], ERHIAccess::SRVMask, ERHIPipeline::All);
}

void FFluid2DFieldSceneExtension::FUpdater::ExecuteMaskAccumulateSolver_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());

	if (!SceneData->HeightFieldRT[0] || !SceneData->HeightFieldRT[1] || !SceneData->HeightFieldRT[2])
	{
		return;
	}

	
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	const FFluid2DFluidConfig& Config = SceneData->CurrentConfig;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);
	const FVector2f GridOrigin(SceneData->WorldGridCenter - FVector2f(0.5f * Config.WorldSize));
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(GridSize, GridSize), FIntPoint(16, 16));


	FRDGTextureRef HeightRefs[3];
	for (uint32 i = 0; i < 3; i++)
	{
		HeightRefs[i] = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i]);
	}
 
	// 1) Apply interaction impulses to current fields.
	if (SceneData->CurrentInteractions.Num() > 0)
	{
		FFluid2DInteractionApplicationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FFluid2DInteractionApplicationCS::FApplyVelocity >(false);
		auto InteractionCS = GlobalShaderMap->GetShader<FFluid2DInteractionApplicationCS>(PermutationVector);
		for (const FFluid2DInteractionData& Interaction : SceneData->CurrentInteractions)
		{
			FFluid2DInteractionApplicationCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DInteractionApplicationCS::FParameters>();
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
			Parameters->InteractionPrevPosition = FVector2f(Interaction.PrevPosition);
			Parameters->InteractionDirection = FVector2f(Interaction.ForceDirection);
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightRefs[SceneData->CurrentHeightIndex]);
			Parameters->VelocityField = nullptr;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.Interaction"),
				ERDGPassFlags::Compute,
				InteractionCS,
				Parameters,
				GroupCount);
		}
	}
	SceneData->CurrentInteractions.Reset();

	// 2) Copy and fade: HeightFieldRT[cur] → HeightFieldRT[next]
	{
		const TShaderMapRef<FFluid2DMaskAccumulateCS> MaskAccumulateCS(GlobalShaderMap);
		FRDGTextureRef HeightOutput = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[(SceneData->CurrentHeightIndex + 1) % 3], TEXT("Fluid2D.Height.Next"));

		FFluid2DMaskAccumulateCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DMaskAccumulateCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->FadeFactor = Config.MA_FadeFactor;
		Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
		Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightOutput);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.MaskAccumulate"),
			ERDGPassFlags::Compute,
			MaskAccumulateCS,
			Parameters,
			GroupCount);

		SceneData->CurrentHeightIndex = (SceneData->CurrentHeightIndex + 1) % 3;
	}

	// 3) Convert solved height field to packed normal XY (RG16F).
	if (SceneData->NormalFieldRT)
	{
		const TShaderMapRef<FFluid2DHeightToNormalCS> HeightToNormalCS(GlobalShaderMap);
		FRDGTextureRef NormalOutput = GraphBuilder.RegisterExternalTexture(SceneData->NormalFieldRT, TEXT("Fluid2D.Normal.Current"));

		FFluid2DHeightToNormalCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DHeightToNormalCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->HeightField = GraphBuilder.CreateSRV(HeightRefs[SceneData->CurrentHeightIndex]);
		Parameters->OutNormalField = GraphBuilder.CreateUAV(NormalOutput);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.ShallowWater.HeightToNormal"),
			ERDGPassFlags::Compute,
			HeightToNormalCS,
			Parameters,
			GroupCount);
			
		GraphBuilder.UseExternalAccessMode(NormalOutput, ERHIAccess::SRVMask, ERHIPipeline::All);
	}
}

void FFluid2DFieldSceneExtension::FUpdater::PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet)
{
	const FFluid2DFluidConfig& Config = SceneData->CurrentConfig;
	FIntPoint GridExtent(Config.GridSize, Config.GridSize);

	if (SceneData->CurrentConfig.SolverType == EFluidSolverType::ShallowWater)
	{
		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->HeightFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->HeightFieldRT[i], TEXT("HeightFieldRT"));
				FRDGTextureRef HeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i], TEXT("Fluid2D.Height.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, HeightTexture, FLinearColor::Black);
			}	
		}

		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->VelocityFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->VelocityFieldRT[i], TEXT("VelocityFieldRT"));
				FRDGTextureRef VelocityTexture = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[i], TEXT("Fluid2D.Velocity.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, VelocityTexture, FLinearColor::Black);
			}
		}

		if (!SceneData->TempHeightFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->TempHeightFieldRT, TEXT("TempHeightFieldRT"));
			FRDGTextureRef TempHeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempHeightFieldRT, TEXT("Fluid2D.TempHeight.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempHeightTexture, FLinearColor::Black);
		}

		if (!SceneData->TempVelocityFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->TempVelocityFieldRT, TEXT("TempVelocityFieldRT"));
			FRDGTextureRef TempVelocityTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempVelocityFieldRT, TEXT("Fluid2D.TempVelocity.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempVelocityTexture, FLinearColor::Black);
		}

		if (!SceneData->NormalFieldRT)
		{
			// RG: encoded normal xy (unpack via xy*2-1). B: foam curvature (peak-positive -∇²h, unsigned).
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_FloatR11G11B10), SceneData->NormalFieldRT, TEXT("NormalFieldRT"));
			FRDGTextureRef NormalTexture = GraphBuilder.RegisterExternalTexture(SceneData->NormalFieldRT, TEXT("Fluid2D.Normal.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, NormalTexture, FLinearColor::Black);
		}

		// Apply pending scroll before simulation
		if (SceneData->GridScrollOffset != FIntVector2::ZeroValue)
		{
			ApplyScroll_RenderThread(GraphBuilder);
		}

		ExecuteShallowWaterSolver_RenderThread(GraphBuilder);
	}
	else if (SceneData->CurrentConfig.SolverType == EFluidSolverType::NavierStokes)
	{
		// Navier-Stokes solver resources
		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->HeightFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->HeightFieldRT[i], TEXT("HeightFieldRT"));
				FRDGTextureRef HeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i], TEXT("Fluid2D.Height.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, HeightTexture, FLinearColor::Black);
			}
		}

		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->VelocityFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->VelocityFieldRT[i], TEXT("VelocityFieldRT"));
				FRDGTextureRef VelocityTexture = GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[i], TEXT("Fluid2D.Velocity.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, VelocityTexture, FLinearColor::Black);
			}
		}
		
		if (!SceneData->TempHeightFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->TempHeightFieldRT, TEXT("TempHeightFieldRT"));
			FRDGTextureRef TempHeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempHeightFieldRT, TEXT("Fluid2D.TempHeight.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempHeightTexture, FLinearColor::Black);
		}

		if (!SceneData->TempVelocityFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_G16R16F), SceneData->TempVelocityFieldRT, TEXT("TempVelocityFieldRT"));
			FRDGTextureRef TempVelocityTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempVelocityFieldRT, TEXT("Fluid2D.TempVelocity.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempVelocityTexture, FLinearColor::Black);
		}

		if (!SceneData->PressureFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->PressureFieldRT, TEXT("PressureFieldRT"));
			FRDGTextureRef PressureTexture = GraphBuilder.RegisterExternalTexture(SceneData->PressureFieldRT, TEXT("Fluid2D.Pressure.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, PressureTexture, FLinearColor::Black);
		}

		if (!SceneData->TempPressureFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->TempPressureFieldRT, TEXT("TempPressureFieldRT"));
			FRDGTextureRef TempPressureTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempPressureFieldRT, TEXT("Fluid2D.TempPressure.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempPressureTexture, FLinearColor::Black);
		}

		if (!SceneData->DivergenceFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->DivergenceFieldRT, TEXT("DivergenceFieldRT"));
			FRDGTextureRef DivergenceTexture = GraphBuilder.RegisterExternalTexture(SceneData->DivergenceFieldRT, TEXT("Fluid2D.Divergence.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, DivergenceTexture, FLinearColor::Black);
		}

		// Apply pending scroll before simulation
		if (SceneData->GridScrollOffset != FIntVector2::ZeroValue)
		{
			ApplyScroll_RenderThread(GraphBuilder);
		}

		ExecuteNavierStokesSolver_RenderThread(GraphBuilder);
	}
	else if (SceneData->CurrentConfig.SolverType == EFluidSolverType::MaskAccumulate)
	{
		for (uint32 i = 0; i < 3; i++)
		{
			if (!SceneData->HeightFieldRT[i])
			{
				GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->HeightFieldRT[i], TEXT("HeightFieldRT"));
				FRDGTextureRef HeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[i], TEXT("Fluid2D.Height.InitClear"));
				AddClearRenderTargetPass(GraphBuilder, HeightTexture, FLinearColor::Black);
			}
		}
		
		if (!SceneData->TempHeightFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent), SceneData->TempHeightFieldRT, TEXT("TempHeightFieldRT"));
			FRDGTextureRef TempHeightTexture = GraphBuilder.RegisterExternalTexture(SceneData->TempHeightFieldRT, TEXT("Fluid2D.TempHeight.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, TempHeightTexture, FLinearColor::Black);
		}

		if (!SceneData->NormalFieldRT)
		{
			// RG: encoded normal xy (unpack via xy*2-1). B: foam curvature (peak-positive -∇²h, unsigned).
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, EPixelFormat::PF_FloatR11G11B10), SceneData->NormalFieldRT, TEXT("NormalFieldRT"));
			FRDGTextureRef NormalTexture = GraphBuilder.RegisterExternalTexture(SceneData->NormalFieldRT, TEXT("Fluid2D.Normal.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, NormalTexture, FLinearColor::Black);
		}

		// Apply pending scroll before simulation
		if (SceneData->GridScrollOffset != FIntVector2::ZeroValue)
		{
			ApplyScroll_RenderThread(GraphBuilder);
		}

		ExecuteMaskAccumulateSolver_RenderThread(GraphBuilder);
	}
}

void FFluid2DFieldSceneExtension::FUpdater::PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet)
{
	(void)GraphBuilder;
	(void)ChangeSet;
}

// -------------------------- Render -----------------------//


BEGIN_SHADER_PARAMETER_STRUCT(FFluid2DSimulationParameters, FLUID2DSYSTEMRUNTIME_API)
	SHADER_PARAMETER(FVector4f, UVScaleOffset)
	SHADER_PARAMETER(FVector4f, Fluid2DMapSizeAndInv)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DHeightMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, Fluid2DHeightMapTextureSampler)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DVelocityMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, Fluid2DVelocityMapTextureSampler)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DNormalMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, Fluid2DNormalMapTextureSampler)
END_SHADER_PARAMETER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FFluid2DSimulationParameters, Fluid2DSimulation, FLUID2DSYSTEMRUNTIME_API)

namespace Fluid2DSimulation
{
	static void GetDefaultParameters(FFluid2DSimulationParameters& OutParameters, FRDGBuilder& GraphBuilder)
	{
		OutParameters.UVScaleOffset = FVector4f::One();
		OutParameters.Fluid2DMapSizeAndInv = FVector4f::One();
		OutParameters.Fluid2DHeightMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
		OutParameters.Fluid2DHeightMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutParameters.Fluid2DVelocityMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
		OutParameters.Fluid2DVelocityMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutParameters.Fluid2DNormalMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
		OutParameters.Fluid2DNormalMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}
}

IMPLEMENT_SCENE_UB_STRUCT(FFluid2DSimulationParameters, Fluid2DSimulation, Fluid2DSimulation::GetDefaultParameters);

void FFluid2DFieldSceneExtension::FRenderer::PreRender(FRDGBuilder& GraphBuilder)
{
}

void FFluid2DFieldSceneExtension::FRenderer::UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& Buffer)
{
	check(IsInRenderingThread());

	FFluid2DSimulationParameters Parameters;
	// UV = (WorldPos - GridBottomLeft) / WorldSize
	//    = WorldPos / WorldSize - WorldGridOrigin / WorldSize + 0.5
	FVector2f UVOffset = FVector2f(0.5f) - SceneData->WorldGridCenter / SceneData->CurrentConfig.WorldSize;

	Parameters.UVScaleOffset = FVector4f(1.0f / SceneData->CurrentConfig.WorldSize, 1.0f / SceneData->CurrentConfig.WorldSize, UVOffset.X, UVOffset.Y);

	Parameters.Fluid2DMapSizeAndInv = FVector4f(SceneData->CurrentConfig.GridSize, SceneData->CurrentConfig.GridSize, 1.0f / SceneData->CurrentConfig.GridSize, 1.0f / SceneData->CurrentConfig.GridSize);

	if (SceneData->HeightFieldRT[SceneData->CurrentHeightIndex])
	{
		Parameters.Fluid2DHeightMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->HeightFieldRT[SceneData->CurrentHeightIndex], TEXT("Fluid2D.Height.Current")));
	}
	else
	{
		Parameters.Fluid2DHeightMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
	}
	Parameters.Fluid2DHeightMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	if (SceneData->OutputVelocityFieldRT)
	{
		Parameters.Fluid2DVelocityMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->OutputVelocityFieldRT, TEXT("Fluid2D.OutputVelocity.Current")));
	}
	else if (SceneData->VelocityFieldRT[SceneData->CurrentVelocityIndex])
	{
		Parameters.Fluid2DVelocityMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->VelocityFieldRT[SceneData->CurrentVelocityIndex], TEXT("Fluid2D.Velocity.Current")));
	}
	else
	{
		Parameters.Fluid2DVelocityMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
	}
	Parameters.Fluid2DVelocityMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	if (SceneData->NormalFieldRT)
	{
		Parameters.Fluid2DNormalMapTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SceneData->NormalFieldRT, TEXT("Fluid2D.Normal.Current")));
	}
	else
	{
		Parameters.Fluid2DNormalMapTexture = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
	}
	Parameters.Fluid2DNormalMapTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	Buffer.Set(SceneUB::Fluid2DSimulation, Parameters);
}