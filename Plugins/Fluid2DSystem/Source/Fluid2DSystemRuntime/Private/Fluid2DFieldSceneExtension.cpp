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
}

FFluid2DFieldSceneExtension::~FFluid2DFieldSceneExtension()
{
}

void FFluid2DSolverLayer::ReleaseResources()
{
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
	NormalFieldRT.SafeRelease();
	PressureFieldRT.SafeRelease();
	TempPressureFieldRT.SafeRelease();
	DivergenceFieldRT.SafeRelease();
	VorticityFieldRT.SafeRelease();
	OutputVelocityFieldRT.SafeRelease();

	CurrentHeightIndex = 0;
	CurrentVelocityIndex = 0;
	WorldGridCenter = FVector2f::ZeroVector;
	GridScrollOffset = FIntVector2::ZeroValue;
	GlobalFlowData = FFluid2DGlobalFlowData();
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

void FFluid2DFieldSceneExtension::SetScrollOffset_RenderThread(EFluidSolverType SolverType, const FIntVector2& NewGridScrollOffset)
{
	check(IsInRenderingThread());
	SolverLayers[Fluid2DSolverType::ToIndex(SolverType)].GridScrollOffset = NewGridScrollOffset;
}

void FFluid2DFieldSceneExtension::SetInteractionsToApply_RenderThread(const TArray<FFluid2DInteractionData>& InteractionsToApply)
{
	check(IsInRenderingThread());
	CurrentInteractions.Append(InteractionsToApply);
}

void FFluid2DFieldSceneExtension::SetGlobalFlowData_RenderThread(EFluidSolverType SolverType, const FFluid2DGlobalFlowData& InFlowData)
{
	check(IsInRenderingThread());
	SolverLayers[Fluid2DSolverType::ToIndex(SolverType)].GlobalFlowData = InFlowData;
}

void FFluid2DFieldSceneExtension::ResetState_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	CurrentInteractions.Empty();

	for (FFluid2DSolverLayer& Layer : SolverLayers)
	{
		Layer.ReleaseResources();
	}
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

void FFluid2DFieldSceneExtension::FUpdater::ApplyScroll_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, const FFluid2DSolverConfigBase& Config, const FIntPoint& TexelOffset)
{
	check(IsInRenderingThread());

	if (TexelOffset.X == 0 && TexelOffset.Y == 0)
	{
		return;
	}

	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);

	// Update this layer's world grid origin by the quantized amount
	Layer.WorldGridCenter += FVector2f(static_cast<float>(TexelOffset.X), static_cast<float>(TexelOffset.Y)) * CellSize;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());
	const FIntPoint GridExtent(GridSize, GridSize);
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(GridExtent, FIntPoint(16, 16));

	// Scroll all 3 height field textures
	{
		const TShaderMapRef<FFluid2DScrollHeightCS> ScrollCS(GlobalShaderMap);

		for (uint32 i = 0; i < 3; i++)
		{
			if (!Layer.HeightFieldRT[i] || !Layer.TempHeightFieldRT) continue;

			FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(Layer.HeightFieldRT[i]);
			FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(Layer.TempHeightFieldRT);

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
			Swap(Layer.HeightFieldRT[i], Layer.TempHeightFieldRT);
		}
	}

	// Scroll all 3 velocity field textures
	{
		const TShaderMapRef<FFluid2DScrollVelocityCS> ScrollCS(GlobalShaderMap);

		for (uint32 i = 0; i < 3; i++)
		{
			if (!Layer.VelocityFieldRT[i] || !Layer.TempVelocityFieldRT) continue;

			FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(Layer.VelocityFieldRT[i]);
			FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(Layer.TempVelocityFieldRT);

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

			Swap(Layer.VelocityFieldRT[i], Layer.TempVelocityFieldRT);
		}
	}

	// Scroll pressure field (NS solver uses it as warm-start for Jacobi iterations)
	if (Layer.PressureFieldRT && Layer.TempPressureFieldRT)
	{
		const TShaderMapRef<FFluid2DScrollHeightCS> ScrollCS(GlobalShaderMap);

		FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(Layer.PressureFieldRT);
		FRDGTextureRef Dest = GraphBuilder.RegisterExternalTexture(Layer.TempPressureFieldRT);

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

		Swap(Layer.PressureFieldRT, Layer.TempPressureFieldRT);
	}
}

void FFluid2DFieldSceneExtension::FUpdater::ExecuteShallowWaterSolver_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, const FSWFluidConfig& Config)
{
	check(IsInRenderingThread());

	if (!Layer.HeightFieldRT[0] || !Layer.HeightFieldRT[1] || !Layer.HeightFieldRT[2] ||
		!Layer.VelocityFieldRT[0] || !Layer.VelocityFieldRT[1] || !Layer.VelocityFieldRT[2])
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	const EFluidSolverType SolverType = EFluidSolverType::ShallowWater;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);
	const FVector2f GridOrigin(Layer.WorldGridCenter - FVector2f(0.5f * Config.WorldSize));
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(GridSize, GridSize), FIntPoint(16, 16));


	FRDGTextureRef HeightRefs[3];
	FRDGTextureRef VelocityRefs[3];
	for (uint32 i = 0; i < 3; i++)
	{
		HeightRefs[i] = GraphBuilder.RegisterExternalTexture(Layer.HeightFieldRT[i]);
		VelocityRefs[i] = GraphBuilder.RegisterExternalTexture(Layer.VelocityFieldRT[i]);
	}
 
	// 1) Apply interaction impulses to current fields.
	if (SceneData->CurrentInteractions.Num() > 0)
	{
		FFluid2DInteractionApplicationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FFluid2DInteractionApplicationCS::FApplyVelocity >(false);
		auto InteractionCS = GlobalShaderMap->GetShader<FFluid2DInteractionApplicationCS>(PermutationVector);
		for (const FFluid2DInteractionData& Interaction : SceneData->CurrentInteractions)
		{
			if (!Interaction.AppliesTo(SolverType))
			{
				continue;
			}

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
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightRefs[Layer.CurrentHeightIndex]);
			Parameters->VelocityField = GraphBuilder.CreateUAV(VelocityRefs[Layer.CurrentVelocityIndex]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.Interaction"),
				ERDGPassFlags::Compute,
				InteractionCS,
				Parameters,
				GroupCount);
		}
	}

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
				Parameters->AdvectionDamping = FMath::Clamp(Config.AdvectionDamping, 0.0f, 1.0f);
				Parameters->FlowNoiseIntensityMin = Layer.GlobalFlowData.FlowNoiseIntensityMin;
				Parameters->FlowNoiseIntensityMax = Layer.GlobalFlowData.FlowNoiseIntensityMax;
				Parameters->FlowNoiseTiling = Layer.GlobalFlowData.FlowNoiseTiling;
				Parameters->FlowDirection = FVector2f(Layer.GlobalFlowData.FlowDirection);
				Parameters->UVScaleOffset = FVector4f(1.0f, 1.0f, GridOrigin.X/Config.WorldSize + 0.5f, GridOrigin.Y/Config.WorldSize + 0.5f);

				Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
				Parameters->FlowNoiseTexture = Layer.GlobalFlowData.FlowNoiseTextureRHI != nullptr ? Layer.GlobalFlowData.FlowNoiseTextureRHI : GBlackTexture->TextureRHI;
				Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
				Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightRefs[Layer.CurrentHeightIndex]);
				Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightRefs[(Layer.CurrentHeightIndex + 1) % 3]);
				Parameters->CurrentVelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);

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
				Parameters->AdvectionDamping = FMath::Clamp(Config.AdvectionDamping, 0.0f, 1.0f);
				Parameters->FlowNoiseIntensityMin = Layer.GlobalFlowData.FlowNoiseIntensityMin;
				Parameters->FlowNoiseIntensityMax = Layer.GlobalFlowData.FlowNoiseIntensityMax;
				Parameters->FlowNoiseTiling = Layer.GlobalFlowData.FlowNoiseTiling;
				Parameters->FlowDirection = FVector2f(Layer.GlobalFlowData.FlowDirection);
				Parameters->UVScaleOffset = FVector4f(1.0f, 1.0f, GridOrigin.X/Config.WorldSize + 0.5f, GridOrigin.Y/Config.WorldSize + 0.5f);

				Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
				Parameters->FlowNoiseTexture = Layer.GlobalFlowData.FlowNoiseTextureRHI != nullptr ? Layer.GlobalFlowData.FlowNoiseTextureRHI : GBlackTexture->TextureRHI;
				Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
				Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightRefs[(Layer.CurrentHeightIndex + 2) % 3]);
				Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightRefs[Layer.CurrentHeightIndex]);
				Parameters->CurrentVelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Fluid2D.ShallowWater.Advection.Height1"),
					ERDGPassFlags::Compute,
					AdvectionCS,
					Parameters,
					GroupCount);
			}
	
			Layer.CurrentHeightIndex = (Layer.CurrentHeightIndex + 1) % 3;
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
			Parameters->Alpha = Config.DiffusionAlpha;
			Parameters->Beta = Config.DiffusionBeta;
			Parameters->DiffusionDamping = FMath::Clamp(Config.DiffusionDamping, 0.0f, 1.0f);
			Parameters->PrevHeightField = GraphBuilder.CreateSRV(HeightRefs[(Layer.CurrentHeightIndex + 2) % 3]);
			Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightRefs[Layer.CurrentHeightIndex]);
			Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightRefs[(Layer.CurrentHeightIndex + 1) % 3]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.ShallowWater.Diffusion"),
				ERDGPassFlags::Compute,
				DiffusionCS,
				Parameters,
				GroupCount);
				
			Layer.CurrentHeightIndex = (Layer.CurrentHeightIndex + 1) % 3;
		}

		// 将Layer.HeightFieldRT[i]设为全局srv
		GraphBuilder.UseExternalAccessMode(HeightRefs[Layer.CurrentHeightIndex], ERHIAccess::SRVMask, ERHIPipeline::All);
	}

	// 4) Convert solved height field to packed normal XY (RG16F).
	if (Layer.NormalFieldRT)
	{
		const TShaderMapRef<FFluid2DHeightToNormalCS> HeightToNormalCS(GlobalShaderMap);
		FRDGTextureRef NormalOutput = GraphBuilder.RegisterExternalTexture(Layer.NormalFieldRT, TEXT("Fluid2D.Normal.Current"));

		FFluid2DHeightToNormalCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DHeightToNormalCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->HeightField = GraphBuilder.CreateSRV(HeightRefs[Layer.CurrentHeightIndex]);
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

void FFluid2DFieldSceneExtension::FUpdater::ExecuteNavierStokesSolver_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, const FNSFluidConfig& Config)
{
	check(IsInRenderingThread());

	if (!Layer.VelocityFieldRT[0] || !Layer.VelocityFieldRT[1] || !Layer.VelocityFieldRT[2] ||
		!Layer.HeightFieldRT[0] || !Layer.HeightFieldRT[1] || !Layer.HeightFieldRT[2] ||
		!Layer.PressureFieldRT || !Layer.TempPressureFieldRT ||
		!Layer.DivergenceFieldRT)
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	const EFluidSolverType SolverType = EFluidSolverType::NavierStokes;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);
	const FVector2f GridOrigin(Layer.WorldGridCenter - FVector2f(0.5f * Config.WorldSize));
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(GridSize, GridSize), FIntPoint(16, 16));

	FRDGTextureRef HeightRefs[3];
	for (uint32 i = 0; i < 3; i++)
	{
		HeightRefs[i] = GraphBuilder.RegisterExternalTexture(Layer.HeightFieldRT[i]);
	}
	FRDGTextureRef VelocityRefs[3];
	for (uint32 i = 0; i < 3; i++)
	{
		VelocityRefs[i] = GraphBuilder.RegisterExternalTexture(Layer.VelocityFieldRT[i]);
	}
	FRDGTextureRef PressureRef = GraphBuilder.RegisterExternalTexture(Layer.PressureFieldRT);
	FRDGTextureRef TempPressureRef = GraphBuilder.RegisterExternalTexture(Layer.TempPressureFieldRT);
	FRDGTextureRef DivergenceRef = GraphBuilder.RegisterExternalTexture(Layer.DivergenceFieldRT);
	
	// 0) Advection: VelocityRT[cur] → NextVelocityRT
	if (CVarNavierStokesAdvection.GetValueOnRenderThread())
	{
		const TShaderMapRef<FNSAdvectionCS> AdvectionCS(GlobalShaderMap);
		FNSAdvectionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSAdvectionCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->DeltaTime = Config.TimeStep;
		Parameters->CellSize = CellSize;
		Parameters->Damping = FMath::Clamp(Config.Damping, 0.0f, 1.0f);
		Parameters->FlowNoiseIntensityMin = Layer.GlobalFlowData.FlowNoiseIntensityMin;
		Parameters->FlowNoiseIntensityMax = Layer.GlobalFlowData.FlowNoiseIntensityMax;
		Parameters->FlowNoiseTiling = Layer.GlobalFlowData.FlowNoiseTiling;
		Parameters->FlowDirection = FVector2f(Layer.GlobalFlowData.FlowDirection);
		Parameters->UVScaleOffset = FVector4f(1.0f, 1.0f, GridOrigin.X/Config.WorldSize + 0.5f, GridOrigin.Y/Config.WorldSize + 0.5f);
		Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		Parameters->FlowNoiseTexture = Layer.GlobalFlowData.FlowNoiseTextureRHI != nullptr ? Layer.GlobalFlowData.FlowNoiseTextureRHI : GBlackTexture->TextureRHI;
		Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);
		Parameters->OutVelocityField = GraphBuilder.CreateUAV(VelocityRefs[(Layer.CurrentVelocityIndex + 1) % 3]);
		Parameters->DensityField = GraphBuilder.CreateSRV(HeightRefs[Layer.CurrentHeightIndex]);
		Parameters->OutDensityField = GraphBuilder.CreateUAV(HeightRefs[(Layer.CurrentHeightIndex + 1) % 3]);
		
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.NS.Advection"),
			ERDGPassFlags::Compute,
			AdvectionCS,
			Parameters,
			GroupCount);
			
			Layer.CurrentVelocityIndex = (Layer.CurrentVelocityIndex + 1) % 3;
			Layer.CurrentHeightIndex = (Layer.CurrentHeightIndex + 1) % 3;
	}

	// 1) Apply interaction impulses
	if (SceneData->CurrentInteractions.Num() > 0)
	{
		FFluid2DInteractionApplicationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FFluid2DInteractionApplicationCS::FApplyVelocity >(true);
		auto InteractionCS = GlobalShaderMap->GetShader<FFluid2DInteractionApplicationCS>(PermutationVector);
		for (const FFluid2DInteractionData& Interaction : SceneData->CurrentInteractions)
		{
			if (!Interaction.AppliesTo(SolverType))
			{
				continue;
			}

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
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightRefs[Layer.CurrentHeightIndex]);
			Parameters->VelocityField = GraphBuilder.CreateUAV(VelocityRefs[Layer.CurrentVelocityIndex]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.NS.Interaction"),
				ERDGPassFlags::Compute,
				InteractionCS,
				Parameters,
				GroupCount);
		}
	}

	// 2) Vorticity Confinement (GPU Gems): restore small-scale rotational detail
	//      Step A: compute scalar vorticity ω = curl(u)
	//      Step B: apply confinement force f = ε·h·(N × ω̂)
	if (Config.VorticityConfinement > 0.0f && CVarNavierStokesVorticityConfinement.GetValueOnRenderThread())
	{
		// Ensure vorticity texture exists
		if (!Layer.VorticityFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(FIntPoint(GridSize, GridSize)), Layer.VorticityFieldRT, TEXT("VorticityFieldRT"));
		}
		FRDGTextureRef VorticityRef = GraphBuilder.RegisterExternalTexture(Layer.VorticityFieldRT);

		// Step A: Compute vorticity
		{
			const TShaderMapRef<FNSComputeVorticityCS> ComputeVorticityCS(GlobalShaderMap);
			FNSComputeVorticityCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSComputeVorticityCS::FParameters>();
			Parameters->GridSize = GridSize;
			Parameters->CellSize = CellSize;
			Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);
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
			Parameters->VorticityEpsilon = Config.VorticityConfinement;
			Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);
			Parameters->VorticityField = GraphBuilder.CreateSRV(VorticityRef);
			Parameters->OutVelocityField = GraphBuilder.CreateUAV(VelocityRefs[(Layer.CurrentVelocityIndex + 1) % 3]);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fluid2D.NS.VorticityConfinement"),
				ERDGPassFlags::Compute,
				VorticityConfinementCS,
				Parameters,
				GroupCount);

			Layer.CurrentVelocityIndex = (Layer.CurrentVelocityIndex + 1) % 3;
		}
	}

	// 2.5) Diffusion: Jacobi iterations (VelocityRT as source, ping-pong VelocityRTs)
	//    Solves implicit diffusion: (I - ν·Δt·∇²) u^{n+1} = u^n
	int32 NumDiffusionJacobiIterations = FMath::Max(Config.NumDiffusionJacobiIterations, 1);
	NumDiffusionJacobiIterations -= NumDiffusionJacobiIterations % 2;
	if (NumDiffusionJacobiIterations > 0 && CVarNavierStokesDiffusion.GetValueOnRenderThread())
	{
		const TShaderMapRef<FNSDiffusionCS> DiffusionCS(GlobalShaderMap);

		// Arrange ping-pong so the final result lands in VelocityRefs[NextVelIdx]
		FRDGTextureRef DiffPing, DiffPong;
		FRDGTextureRef DensityDiffusionPing, DensityDiffusionPong;
		DiffPing = VelocityRefs[(Layer.CurrentVelocityIndex + 1) % 3];
		DiffPong = VelocityRefs[(Layer.CurrentVelocityIndex + 2) % 3];
		DensityDiffusionPing = HeightRefs[(Layer.CurrentHeightIndex + 1) % 3];
		DensityDiffusionPong = HeightRefs[(Layer.CurrentHeightIndex + 2) % 3];

		for (int32 i = 0; i < NumDiffusionJacobiIterations; i++)
		{
			FNSDiffusionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSDiffusionCS::FParameters>();
			Parameters->GridSize = GridSize;
			Parameters->DeltaTime = Config.TimeStep;
			Parameters->CellSize = CellSize;
			Parameters->Viscosity = Config.Viscosity;
			Parameters->OriginalVelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);
			Parameters->VelocityField = GraphBuilder.CreateSRV((i == 0) ? VelocityRefs[Layer.CurrentVelocityIndex] : DiffPing);
			Parameters->OutVelocityField = GraphBuilder.CreateUAV(DiffPong);
			Parameters->OriginalDensityField = GraphBuilder.CreateSRV(HeightRefs[Layer.CurrentHeightIndex]);
			Parameters->DensityField = GraphBuilder.CreateSRV((i == 0) ? HeightRefs[Layer.CurrentHeightIndex] : DensityDiffusionPing);
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

		Layer.CurrentVelocityIndex = (Layer.CurrentVelocityIndex + 1) % 3;
		Layer.CurrentHeightIndex = (Layer.CurrentHeightIndex + 1) % 3;
	}

	// 3) Compute Divergence: VelocityRT[next] → DivergenceRT
	if (CVarNavierStokesProjection.GetValueOnRenderThread())
	{
		const TShaderMapRef<FNSComputeDivergenceCS> ComputeDivergenceCS(GlobalShaderMap);
		FNSComputeDivergenceCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSComputeDivergenceCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);
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
		int32 NumPressureJacobiIterations = FMath::Max(Config.NumPressureJacobiIterations, 1);
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
		PressureRef = GraphBuilder.RegisterExternalTexture(Layer.PressureFieldRT);
		const TShaderMapRef<FNSProjectionCS> ProjectionCS(GlobalShaderMap);
		FNSProjectionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNSProjectionCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->DeltaTime = Config.TimeStep;
		Parameters->CellSize = CellSize;
		Parameters->VelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);
		Parameters->PressureField = GraphBuilder.CreateSRV(PressureRef);
		Parameters->OutVelocityField = GraphBuilder.CreateUAV(VelocityRefs[(Layer.CurrentVelocityIndex + 1) % 3]);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.NS.Projection"),
			ERDGPassFlags::Compute,
			ProjectionCS,
			Parameters,
			GroupCount);
			
		Layer.CurrentVelocityIndex = (Layer.CurrentVelocityIndex + 1) % 3;
	}

	// 6) Compose output: simulation velocity + global flow noise → OutputVelocityFieldRT
	{
		if (!Layer.OutputVelocityFieldRT)
		{
			GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(FIntPoint(GridSize, GridSize), EPixelFormat::PF_G16R16F), Layer.OutputVelocityFieldRT, TEXT("OutputVelocityFieldRT"));
			FRDGTextureRef OutputVelTexture = GraphBuilder.RegisterExternalTexture(Layer.OutputVelocityFieldRT, TEXT("Fluid2D.OutputVelocity.InitClear"));
			AddClearRenderTargetPass(GraphBuilder, OutputVelTexture, FLinearColor::Black);
		}
		FRDGTextureRef OutputVelocityRef = GraphBuilder.RegisterExternalTexture(Layer.OutputVelocityFieldRT);

		const TShaderMapRef<FFluid2DComposeVelocityCS> ComposeCS(GlobalShaderMap);
		FFluid2DComposeVelocityCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DComposeVelocityCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->FlowNoiseIntensityMin = Layer.GlobalFlowData.FlowNoiseIntensityMin;
		Parameters->FlowNoiseIntensityMax = Layer.GlobalFlowData.FlowNoiseIntensityMax;
		Parameters->FlowNoiseTiling = Layer.GlobalFlowData.FlowNoiseTiling;
		Parameters->FlowDirection = FVector2f(Layer.GlobalFlowData.FlowDirection);
		Parameters->UVScaleOffset = FVector4f(1.0f, 1.0f, GridOrigin.X/Config.WorldSize + 0.5f, GridOrigin.Y/Config.WorldSize + 0.5f);
		Parameters->FlowNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		Parameters->FlowNoiseTexture = Layer.GlobalFlowData.FlowNoiseTextureRHI != nullptr ? Layer.GlobalFlowData.FlowNoiseTextureRHI : GBlackTexture->TextureRHI;
		Parameters->SourceVelocityField = GraphBuilder.CreateSRV(VelocityRefs[Layer.CurrentVelocityIndex]);
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

	// 将Layer.VelocityFieldRT[i]和Layer.HeightFieldRT[i]设为全局srv
	GraphBuilder.UseExternalAccessMode(VelocityRefs[Layer.CurrentVelocityIndex], ERHIAccess::SRVMask, ERHIPipeline::All);
	GraphBuilder.UseExternalAccessMode(HeightRefs[Layer.CurrentHeightIndex], ERHIAccess::SRVMask, ERHIPipeline::All);
}

void FFluid2DFieldSceneExtension::FUpdater::ExecuteMaskAccumulateSolver_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, const FMAFluidConfig& Config)
{
	check(IsInRenderingThread());

	if (!Layer.HeightFieldRT[0] || !Layer.HeightFieldRT[1] || !Layer.HeightFieldRT[2])
	{
		return;
	}

	
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel());

	const EFluidSolverType SolverType = EFluidSolverType::MaskAccumulate;
	const uint32 GridSize = FMath::Max(Config.GridSize, 1);
	const float CellSize = Config.WorldSize / static_cast<float>(GridSize);
	const FVector2f GridOrigin(Layer.WorldGridCenter - FVector2f(0.5f * Config.WorldSize));
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(GridSize, GridSize), FIntPoint(16, 16));


	FRDGTextureRef HeightRefs[3];
	for (uint32 i = 0; i < 3; i++)
	{
		HeightRefs[i] = GraphBuilder.RegisterExternalTexture(Layer.HeightFieldRT[i]);
	}
 
	// 1) Apply interaction impulses to current fields.
	if (SceneData->CurrentInteractions.Num() > 0)
	{
		FFluid2DInteractionApplicationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FFluid2DInteractionApplicationCS::FApplyVelocity >(false);
		auto InteractionCS = GlobalShaderMap->GetShader<FFluid2DInteractionApplicationCS>(PermutationVector);
		for (const FFluid2DInteractionData& Interaction : SceneData->CurrentInteractions)
		{
			if (!Interaction.AppliesTo(SolverType))
			{
				continue;
			}

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
			Parameters->HeightField = GraphBuilder.CreateUAV(HeightRefs[Layer.CurrentHeightIndex]);
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

	// 2) Copy and fade: HeightFieldRT[cur] → HeightFieldRT[next]
	{
		const TShaderMapRef<FFluid2DMaskAccumulateCS> MaskAccumulateCS(GlobalShaderMap);
		FRDGTextureRef HeightOutput = GraphBuilder.RegisterExternalTexture(Layer.HeightFieldRT[(Layer.CurrentHeightIndex + 1) % 3], TEXT("Fluid2D.Height.Next"));

		FFluid2DMaskAccumulateCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DMaskAccumulateCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->FadeFactor = Config.FadeFactor;
		Parameters->CurrentHeightField = GraphBuilder.CreateSRV(HeightRefs[Layer.CurrentHeightIndex]);
		Parameters->NextHeightField = GraphBuilder.CreateUAV(HeightOutput);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fluid2D.MaskAccumulate"),
			ERDGPassFlags::Compute,
			MaskAccumulateCS,
			Parameters,
			GroupCount);

		Layer.CurrentHeightIndex = (Layer.CurrentHeightIndex + 1) % 3;
	}

	// 3) Convert solved height field to packed normal XY (RG16F).
	if (Layer.NormalFieldRT)
	{
		const TShaderMapRef<FFluid2DHeightToNormalCS> HeightToNormalCS(GlobalShaderMap);
		FRDGTextureRef NormalOutput = GraphBuilder.RegisterExternalTexture(Layer.NormalFieldRT, TEXT("Fluid2D.Normal.Current"));

		FFluid2DHeightToNormalCS::FParameters* Parameters = GraphBuilder.AllocParameters<FFluid2DHeightToNormalCS::FParameters>();
		Parameters->GridSize = GridSize;
		Parameters->CellSize = CellSize;
		Parameters->HeightField = GraphBuilder.CreateSRV(HeightRefs[Layer.CurrentHeightIndex]);
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
	const FFluid2DFluidConfig& FluidConfig = SceneData->CurrentConfig;

	// Solver types coexist: every enabled one runs on its own layer, with its own grid and parameters.
	for (int32 SolverIndex = 0; SolverIndex < Fluid2DSolverType::Count; ++SolverIndex)
	{
		const EFluidSolverType SolverType = Fluid2DSolverType::FromIndex(SolverIndex);
		FFluid2DSolverLayer& Layer = SceneData->SolverLayers[SolverIndex];

		if (!FluidConfig.IsSolverTypeEnabled(SolverType))
		{
			Layer.ReleaseResources();
			continue;
		}

		const FFluid2DSolverConfigBase& LayerConfig = FluidConfig.GetSolverConfig(SolverType);

		EnsureLayerResources_RenderThread(GraphBuilder, Layer, SolverType, LayerConfig);

		if (Layer.GridScrollOffset != FIntVector2::ZeroValue)
		{
			ApplyScroll_RenderThread(GraphBuilder, Layer, LayerConfig, FIntPoint(Layer.GridScrollOffset.X, Layer.GridScrollOffset.Y));
			Layer.GridScrollOffset = FIntVector2::ZeroValue;
		}

		ExecuteSolver_RenderThread(GraphBuilder, Layer, SolverType);
	}

	// Every enabled layer above consumed the pending interactions.
	SceneData->CurrentInteractions.Reset();
}

void FFluid2DFieldSceneExtension::FUpdater::EnsureLayerResources_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, EFluidSolverType SolverType, const FFluid2DSolverConfigBase& Config)
{
	const int32 GridSize = FMath::Max(Config.GridSize, 1);
	const FIntPoint GridExtent(GridSize, GridSize);

	const bool bUseVelocityField = SolverType != EFluidSolverType::MaskAccumulate;
	const bool bUsePressureField = SolverType == EFluidSolverType::NavierStokes;
	const bool bUseNormalField = SolverType != EFluidSolverType::NavierStokes;

	const auto EnsureRenderTarget = [&GraphBuilder, &GridExtent](TRefCountPtr<IPooledRenderTarget>& RenderTarget, EPixelFormat Format, const TCHAR* DebugName)
	{
		if (RenderTarget.IsValid())
		{
			return;
		}

		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, CreateWindRenderTargetDesc(GridExtent, Format), RenderTarget, DebugName);
		FRDGTextureRef Texture = GraphBuilder.RegisterExternalTexture(RenderTarget);
		AddClearRenderTargetPass(GraphBuilder, Texture, FLinearColor::Black);
	};

	for (uint32 i = 0; i < 3; i++)
	{
		EnsureRenderTarget(Layer.HeightFieldRT[i], EPixelFormat::PF_R16F, TEXT("HeightFieldRT"));
		if (bUseVelocityField)
		{
			EnsureRenderTarget(Layer.VelocityFieldRT[i], EPixelFormat::PF_G16R16F, TEXT("VelocityFieldRT"));
		}
	}

	EnsureRenderTarget(Layer.TempHeightFieldRT, EPixelFormat::PF_R16F, TEXT("TempHeightFieldRT"));
	if (bUseVelocityField)
	{
		EnsureRenderTarget(Layer.TempVelocityFieldRT, EPixelFormat::PF_G16R16F, TEXT("TempVelocityFieldRT"));
	}

	if (bUsePressureField)
	{
		EnsureRenderTarget(Layer.PressureFieldRT, EPixelFormat::PF_R16F, TEXT("PressureFieldRT"));
		EnsureRenderTarget(Layer.TempPressureFieldRT, EPixelFormat::PF_R16F, TEXT("TempPressureFieldRT"));
		EnsureRenderTarget(Layer.DivergenceFieldRT, EPixelFormat::PF_R16F, TEXT("DivergenceFieldRT"));
	}

	if (bUseNormalField)
	{
		// RG: encoded normal xy (unpack via xy*2-1). B: foam curvature (peak-positive -∇²h, unsigned).
		EnsureRenderTarget(Layer.NormalFieldRT, EPixelFormat::PF_FloatR11G11B10, TEXT("NormalFieldRT"));
	}
}

void FFluid2DFieldSceneExtension::FUpdater::ExecuteSolver_RenderThread(FRDGBuilder& GraphBuilder, FFluid2DSolverLayer& Layer, EFluidSolverType SolverType)
{
	const FFluid2DFluidConfig& FluidConfig = SceneData->CurrentConfig;

	switch (SolverType)
	{
	case EFluidSolverType::ShallowWater:
		ExecuteShallowWaterSolver_RenderThread(GraphBuilder, Layer, FluidConfig.ShallowWater);
		break;
	case EFluidSolverType::NavierStokes:
		ExecuteNavierStokesSolver_RenderThread(GraphBuilder, Layer, FluidConfig.NavierStokes);
		break;
	case EFluidSolverType::MaskAccumulate:
		ExecuteMaskAccumulateSolver_RenderThread(GraphBuilder, Layer, FluidConfig.MaskAccumulate);
		break;
	default:
		break;
	}
}

void FFluid2DFieldSceneExtension::FUpdater::PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet)
{
	(void)GraphBuilder;
	(void)ChangeSet;
}

// -------------------------- Render -----------------------//


BEGIN_SHADER_PARAMETER_STRUCT(FFluid2DSimulationParameters, FLUID2DSYSTEMRUNTIME_API)
	// Default output: the enabled solver layer with the lowest index. Existing materials sample these.
	SHADER_PARAMETER(FVector4f, UVScaleOffset)
	SHADER_PARAMETER(FVector4f, Fluid2DMapSizeAndInv)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DHeightMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, Fluid2DHeightMapTextureSampler)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DVelocityMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, Fluid2DVelocityMapTextureSampler)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DNormalMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, Fluid2DNormalMapTextureSampler)

	// Per-solver-type output. Each layer has its own grid mapping, so UV/size are per layer as well.
	SHADER_PARAMETER(FVector4f, Fluid2DShallowWaterUVScaleOffset)
	SHADER_PARAMETER(FVector4f, Fluid2DShallowWaterMapSizeAndInv)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DShallowWaterHeightMapTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DShallowWaterVelocityMapTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DShallowWaterNormalMapTexture)
	SHADER_PARAMETER(FVector4f, Fluid2DNavierStokesUVScaleOffset)
	SHADER_PARAMETER(FVector4f, Fluid2DNavierStokesMapSizeAndInv)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DNavierStokesHeightMapTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DNavierStokesVelocityMapTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DNavierStokesNormalMapTexture)
	SHADER_PARAMETER(FVector4f, Fluid2DMaskAccumulateUVScaleOffset)
	SHADER_PARAMETER(FVector4f, Fluid2DMaskAccumulateMapSizeAndInv)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DMaskAccumulateHeightMapTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DMaskAccumulateVelocityMapTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Fluid2DMaskAccumulateNormalMapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, Fluid2DFieldSampler)
END_SHADER_PARAMETER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FFluid2DSimulationParameters, Fluid2DSimulation, FLUID2DSYSTEMRUNTIME_API)

namespace Fluid2DSimulation
{
	static void GetDefaultParameters(FFluid2DSimulationParameters& OutParameters, FRDGBuilder& GraphBuilder)
	{
		const FRDGTextureSRVRef BlackDummy = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
		FRHISamplerState* BilinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		OutParameters.UVScaleOffset = FVector4f::One();
		OutParameters.Fluid2DMapSizeAndInv = FVector4f::One();
		OutParameters.Fluid2DHeightMapTexture = BlackDummy;
		OutParameters.Fluid2DHeightMapTextureSampler = BilinearClamp;
		OutParameters.Fluid2DVelocityMapTexture = BlackDummy;
		OutParameters.Fluid2DVelocityMapTextureSampler = BilinearClamp;
		OutParameters.Fluid2DNormalMapTexture = BlackDummy;
		OutParameters.Fluid2DNormalMapTextureSampler = BilinearClamp;

		OutParameters.Fluid2DShallowWaterUVScaleOffset = FVector4f::One();
		OutParameters.Fluid2DShallowWaterMapSizeAndInv = FVector4f::One();
		OutParameters.Fluid2DShallowWaterHeightMapTexture = BlackDummy;
		OutParameters.Fluid2DShallowWaterVelocityMapTexture = BlackDummy;
		OutParameters.Fluid2DShallowWaterNormalMapTexture = BlackDummy;
		OutParameters.Fluid2DNavierStokesUVScaleOffset = FVector4f::One();
		OutParameters.Fluid2DNavierStokesMapSizeAndInv = FVector4f::One();
		OutParameters.Fluid2DNavierStokesHeightMapTexture = BlackDummy;
		OutParameters.Fluid2DNavierStokesVelocityMapTexture = BlackDummy;
		OutParameters.Fluid2DNavierStokesNormalMapTexture = BlackDummy;
		OutParameters.Fluid2DMaskAccumulateUVScaleOffset = FVector4f::One();
		OutParameters.Fluid2DMaskAccumulateMapSizeAndInv = FVector4f::One();
		OutParameters.Fluid2DMaskAccumulateHeightMapTexture = BlackDummy;
		OutParameters.Fluid2DMaskAccumulateVelocityMapTexture = BlackDummy;
		OutParameters.Fluid2DMaskAccumulateNormalMapTexture = BlackDummy;
		OutParameters.Fluid2DFieldSampler = BilinearClamp;
	}
}

IMPLEMENT_SCENE_UB_STRUCT(FFluid2DSimulationParameters, Fluid2DSimulation, Fluid2DSimulation::GetDefaultParameters);

void FFluid2DFieldSceneExtension::FRenderer::PreRender(FRDGBuilder& GraphBuilder)
{
}

void FFluid2DFieldSceneExtension::FRenderer::UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& Buffer)
{
	check(IsInRenderingThread());

	const FFluid2DFluidConfig& FluidConfig = SceneData->CurrentConfig;

	FFluid2DSimulationParameters Parameters;

	const auto GetFieldSRV = [&GraphBuilder](TRefCountPtr<IPooledRenderTarget>& RenderTarget, const TCHAR* DebugName) -> FRDGTextureSRVRef
	{
		return RenderTarget.IsValid()
			? GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(RenderTarget, DebugName))
			: GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
	};

	const auto BindLayerOutput = [this, &GraphBuilder, &FluidConfig, &GetFieldSRV](EFluidSolverType SolverType,
		FVector4f& OutUVScaleOffset, FVector4f& OutMapSizeAndInv,
		FRDGTextureSRV*& OutHeight, FRDGTextureSRV*& OutVelocity, FRDGTextureSRV*& OutNormal)
	{
		const FFluid2DSolverConfigBase& LayerConfig = FluidConfig.GetSolverConfig(SolverType);
		const float WorldSize = FMath::Max(LayerConfig.WorldSize, 1.0f);
		const float GridSize = static_cast<float>(FMath::Max(LayerConfig.GridSize, 1));

		OutMapSizeAndInv = FVector4f(GridSize, GridSize, 1.0f / GridSize, 1.0f / GridSize);

		if (!FluidConfig.IsSolverTypeEnabled(SolverType))
		{
			const FRDGTextureSRVRef BlackDummy = GraphBuilder.CreateSRV(GSystemTextures.GetBlackDummy(GraphBuilder));
			OutUVScaleOffset = FVector4f(1.0f / WorldSize, 1.0f / WorldSize, 0.5f, 0.5f);
			OutHeight = BlackDummy;
			OutVelocity = BlackDummy;
			OutNormal = BlackDummy;
			return;
		}

		FFluid2DSolverLayer& Layer = SceneData->SolverLayers[Fluid2DSolverType::ToIndex(SolverType)];

		// UV = (WorldPos - GridBottomLeft) / WorldSize = WorldPos / WorldSize - WorldGridOrigin / WorldSize + 0.5
		const FVector2f UVOffset = FVector2f(0.5f) - Layer.WorldGridCenter / WorldSize;

		OutUVScaleOffset = FVector4f(1.0f / WorldSize, 1.0f / WorldSize, UVOffset.X, UVOffset.Y);
		OutHeight = GetFieldSRV(Layer.GetCurrentHeightField(), TEXT("Fluid2D.Height.Current"));
		OutVelocity = GetFieldSRV(Layer.GetCurrentVelocityField(), TEXT("Fluid2D.Velocity.Current"));
		OutNormal = GetFieldSRV(Layer.GetNormalField(), TEXT("Fluid2D.Normal.Current"));
	};

	BindLayerOutput(EFluidSolverType::ShallowWater,
		Parameters.Fluid2DShallowWaterUVScaleOffset, Parameters.Fluid2DShallowWaterMapSizeAndInv,
		Parameters.Fluid2DShallowWaterHeightMapTexture, Parameters.Fluid2DShallowWaterVelocityMapTexture, Parameters.Fluid2DShallowWaterNormalMapTexture);

	BindLayerOutput(EFluidSolverType::NavierStokes,
		Parameters.Fluid2DNavierStokesUVScaleOffset, Parameters.Fluid2DNavierStokesMapSizeAndInv,
		Parameters.Fluid2DNavierStokesHeightMapTexture, Parameters.Fluid2DNavierStokesVelocityMapTexture, Parameters.Fluid2DNavierStokesNormalMapTexture);

	BindLayerOutput(EFluidSolverType::MaskAccumulate,
		Parameters.Fluid2DMaskAccumulateUVScaleOffset, Parameters.Fluid2DMaskAccumulateMapSizeAndInv,
		Parameters.Fluid2DMaskAccumulateHeightMapTexture, Parameters.Fluid2DMaskAccumulateVelocityMapTexture, Parameters.Fluid2DMaskAccumulateNormalMapTexture);

	// The default output mirrors the primary layer so single-fluid materials keep working.
	BindLayerOutput(FluidConfig.GetPrimarySolverType(),
		Parameters.UVScaleOffset, Parameters.Fluid2DMapSizeAndInv,
		Parameters.Fluid2DHeightMapTexture, Parameters.Fluid2DVelocityMapTexture, Parameters.Fluid2DNormalMapTexture);

	FRHISamplerState* BilinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.Fluid2DHeightMapTextureSampler = BilinearClamp;
	Parameters.Fluid2DVelocityMapTextureSampler = BilinearClamp;
	Parameters.Fluid2DNormalMapTextureSampler = BilinearClamp;
	Parameters.Fluid2DFieldSampler = BilinearClamp;

	Buffer.Set(SceneUB::Fluid2DSimulation, Parameters);
}
