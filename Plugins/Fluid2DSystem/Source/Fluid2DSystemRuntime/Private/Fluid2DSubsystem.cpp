// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Fluid2DSystemSettings.h"
#include "Fluid2DFieldSceneExtension.h"
#include "Fluid2DInteractionComponent.h"
#include "Fluid2DFluidConfigComponent.h"
#include "Fluid2DFlowVolume.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SceneInterface.h"
#include "ScenePrivate.h"
#include "RendererInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogFluid2DSystem, Log, All);

// Forward declare to avoid circular dependency
class FFluid2DFieldSceneExtension;

UFluid2DSubsystem::UFluid2DSubsystem()
{
	// FIntVector2 has no initializing default constructor.
	for (FIntVector2& LastScrollTargetLocation : LastScrollTargetLocationInts)
	{
		LastScrollTargetLocation = FIntVector2::ZeroValue;
	}

#if WITH_EDITOR
	UFluid2DSystemSettings::OnSettingsChange.AddUObject(this, &UFluid2DSubsystem::LoadGlobalFluidConfig);
#endif
}

void UFluid2DSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UFluid2DSystemSettings* Settings = UFluid2DSystemSettings::Get();
	DefaultFluidConfig = Settings->DefaultFluidConfig;
	FluidConfig = DefaultFluidConfig;

	ResetState();

	const UEnum* SolverTypeEnum = StaticEnum<EFluidSolverType>();
	const FString PrimarySolverName = SolverTypeEnum->GetNameStringByValue(Fluid2DSolverType::ToIndex(FluidConfig.GetPrimarySolverType()));
	UE_LOG(LogFluid2DSystem, Log, TEXT("Fluid2DSubsystem initialized - primary solver: %s"), *PrimarySolverName);
}

void UFluid2DSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UFluid2DSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!FluidConfig.HasEnabledSolverType())
		return;

	ScrollWorldGrid();
	UpdateInteractions();
	UpdateGlobalFlow();
}

TStatId UFluid2DSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFluid2DSubsystem, STATGROUP_Tickables);
}

bool UFluid2DSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor || WorldType == EWorldType::GamePreview;
}

void UFluid2DSubsystem::RegisterInteractionComponent(UFluid2DInteractionComponent* Component)
{
	RegisteredInteractionComponents.AddUnique(Component);
}

void UFluid2DSubsystem::UnregisterInteractionComponent(UFluid2DInteractionComponent* Component)
{
	RegisteredInteractionComponents.Remove(Component);
}

void UFluid2DSubsystem::RegisterFluidConfigComponent(UFluid2DFluidConfigComponent* Component)
{
	RegisteredFluidConfigComponents.AddUnique(Component);
}

void UFluid2DSubsystem::UnregisterFluidConfigComponent(UFluid2DFluidConfigComponent* Component)
{
	RegisteredFluidConfigComponents.Remove(Component);
	// Fall back to the project settings default (or the next registered override) now that this component is gone.
	ResetState();
}

void UFluid2DSubsystem::InsertFlowVolume(AFluid2DFlowVolume* Volume)
{
	if (!Volume)
	{
		return;
	}

	// Dedup, then insert so the array is ordered: bounded volumes by Priority (high first),
	// followed by unbound volumes (also Priority high first). Higher-priority volumes are tried
	// first during blending; unbound volumes act as fallbacks at the tail.
	if (FlowVolumes.Contains(Volume))
	{
		return;
	}

	int32 InsertIndex = FlowVolumes.Num();
	for (int32 i = 0; i < FlowVolumes.Num(); ++i)
	{
		const AFluid2DFlowVolume* CurrentVolume = FlowVolumes[i];
		if (!CurrentVolume)
		{
			continue;
		}

		const bool bCurrentComesBefore = (!CurrentVolume->bUnbound && Volume->bUnbound)
			|| (CurrentVolume->bUnbound == Volume->bUnbound && CurrentVolume->Priority >= Volume->Priority);
		if (!bCurrentComesBefore)
		{
			InsertIndex = i;
			break;
		}
	}
	FlowVolumes.Insert(Volume, InsertIndex);
}

void UFluid2DSubsystem::RemoveFlowVolume(AFluid2DFlowVolume* Volume)
{
	FlowVolumes.RemoveSingle(Volume);
}

FVector UFluid2DSubsystem::GetSampleLocation() const
{
	// Prefer the player pawn location so the simulation follows gameplay position.
	FVector SampleLocation = FVector::ZeroVector;
	if (UWorld* World = GetWorld())
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		if (PlayerController != nullptr)
		{
			if (const APawn* PlayerPawn = PlayerController->GetPawn())
			{
				SampleLocation = PlayerPawn->GetActorLocation();
			}
			else if (PlayerController->PlayerCameraManager != nullptr)
			{
				SampleLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
			}
		}
		else
		{
			const auto& ViewLocations = World->ViewLocationsRenderedLastFrame;
			if (ViewLocations.Num() > 0)
			{
				SampleLocation = ViewLocations[0];
			}
		}
	}
	return SampleLocation;
}

void UFluid2DSubsystem::ScrollWorldGrid()
{
	if (!GetWorld() || !GetWorld()->Scene)
	{
		return;
	}

	const FVector ScrollTargetLocation = GetSampleLocation();
	const FIntVector2 ScrollTargetLocationInt(FMath::RoundToInt(ScrollTargetLocation.X), FMath::RoundToInt(ScrollTargetLocation.Y));

	// Every layer owns its grid, so each one is scrolled with its own cell size and threshold.
	for (int32 SolverIndex = 0; SolverIndex < Fluid2DSolverType::Count; ++SolverIndex)
	{
		const EFluidSolverType SolverType = Fluid2DSolverType::FromIndex(SolverIndex);
		if (!FluidConfig.IsSolverTypeEnabled(SolverType))
		{
			continue;
		}

		const FFluid2DSolverConfigBase& LayerConfig = FluidConfig.GetSolverConfig(SolverType);
		const FIntVector2 ViewDelta = ScrollTargetLocationInt - LastScrollTargetLocationInts[SolverIndex];
		const int32 MaxOffset = FMath::Max(FMath::Abs(ViewDelta.X), FMath::Abs(ViewDelta.Y));
		if (MaxOffset <= LayerConfig.WorldSize * 0.25f)
		{
			continue;
		}

		const uint32 GridSize = FMath::Max(LayerConfig.GridSize, 1);
		const float CellSize = LayerConfig.WorldSize / static_cast<float>(GridSize);

		const FIntVector2 GridScrollOffset = FIntVector2(
			FMath::RoundToInt32(ViewDelta.X / CellSize),
			FMath::RoundToInt32(ViewDelta.Y / CellSize));

		ENQUEUE_RENDER_COMMAND(ScrollOffset)(
		[WorldScene = GetWorld()->Scene, SolverType, GridScrollOffset](FRHICommandListImmediate& RHICmdList)
			{
				if (WorldScene->GetRenderScene())
				{
					if (FFluid2DFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FFluid2DFieldSceneExtension>())
					{
						UE_LOG(LogFluid2DSystem, Verbose, TEXT("Scroll offset on render thread"));
						SceneExtension->SetScrollOffset_RenderThread(SolverType, GridScrollOffset);
					}
				}
			});

		LastScrollTargetLocationInts[SolverIndex] = ScrollTargetLocationInt;
	}
}

void UFluid2DSubsystem::UpdateFluidConfig()
{
	if (!GetWorld() || !GetWorld()->Scene)
	{
		return;
	}

	// Start from the project-settings defaults, then let every registered component override its own layer.
	FFluid2DFluidConfig ApplyFluidConfig = DefaultFluidConfig;
	for (const UFluid2DFluidConfigComponent* ConfigComponent : RegisteredFluidConfigComponents)
	{
		if (!ConfigComponent)
		{
			continue;
		}

		switch (ConfigComponent->SolverType)
		{
		case EFluidSolverType::ShallowWater:
			ApplyFluidConfig.ShallowWater = ConfigComponent->ShallowWater;
			break;
		case EFluidSolverType::NavierStokes:
			ApplyFluidConfig.NavierStokes = ConfigComponent->NavierStokes;
			break;
		case EFluidSolverType::MaskAccumulate:
			ApplyFluidConfig.MaskAccumulate = ConfigComponent->MaskAccumulate;
			break;
		default:
			break;
		}
	}

	// Keep the CPU-side FluidConfig in sync with what we push to the render thread so game-thread readers
	// (e.g. UFluid2DInteractionComponent::IsSubmerged, ScrollWorldGrid) observe the currently-effective config.
	FluidConfig = ApplyFluidConfig;

	ENQUEUE_RENDER_COMMAND(UpdateFluidConfig)(
	[WorldScene = GetWorld()->Scene, FluidConfig = ApplyFluidConfig](FRHICommandListImmediate& RHICmdList)
		{
			if (WorldScene->GetRenderScene())
			{
				if (FFluid2DFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FFluid2DFieldSceneExtension>())
				{
					UE_LOG(LogFluid2DSystem, Verbose, TEXT("Updating fluid config on render thread"));
					SceneExtension->SetConfig_RenderThread(FluidConfig);
				}
			}
		});
}

void UFluid2DSubsystem::LoadGlobalFluidConfig(const UFluid2DSystemSettings* Settings, EPropertyChangeType::Type ChangeType)
{
	DefaultFluidConfig = Settings->DefaultFluidConfig;
	ResetState();
}

void UFluid2DSubsystem::UpdateInteractions()
{
	if (GetWorld() && GetWorld()->Scene && RegisteredInteractionComponents.Num() > 0)
	{
		TArray<FFluid2DInteractionData> InteractionsToApply;
		for (UFluid2DInteractionComponent* Component : RegisteredInteractionComponents)
		{
			if (Component && Component->IsInteractionUseful())
			{
				InteractionsToApply.Add(Component->GetCurrentInteractionData());
			}
		}
		if (InteractionsToApply.Num() > 0)
		{
			ENQUEUE_RENDER_COMMAND(UpdateInteractions)(
			[WorldScene = GetWorld()->Scene, InteractionsToApply = InteractionsToApply](FRHICommandListImmediate& RHICmdList) 
				{
					if (WorldScene->GetRenderScene())
					{
						if (FFluid2DFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FFluid2DFieldSceneExtension>())
						{
							SceneExtension->SetInteractionsToApply_RenderThread(InteractionsToApply);
						}
					}
				});
		}
	}
}

void UFluid2DSubsystem::UpdateGlobalFlow()
{
	if (!GetWorld() || !GetWorld()->Scene)
	{
		return;
	}

	const FVector SampleLocation = GetSampleLocation();

	// Flow noise, tiling and the driving volume are resolved per layer: each layer has its own flow config.
	for (int32 SolverIndex = 0; SolverIndex < Fluid2DSolverType::Count; ++SolverIndex)
	{
		const EFluidSolverType SolverType = Fluid2DSolverType::FromIndex(SolverIndex);
		if (!FluidConfig.IsSolverTypeEnabled(SolverType))
		{
			continue;
		}

		const FFluid2DSolverConfigBase& LayerConfig = FluidConfig.GetSolverConfig(SolverType);

		FFluid2DGlobalFlowData FlowData;
		FlowData.FlowNoiseTiling = LayerConfig.FlowNoiseTiling;

		float BlendWeight = 0.0f;
		if (AFluid2DFlowVolume* Volume = ResolveFlowVolumeAt(SampleLocation, SolverType, BlendWeight))
		{
			const float SpeedWeighted = Volume->Speed * BlendWeight;
			FlowData.FlowDirection = Volume->FlowDirection.GetSafeNormal();
			FlowData.FlowNoiseIntensityMin = LayerConfig.FlowNoiseIntensityMin * SpeedWeighted;
			FlowData.FlowNoiseIntensityMax = LayerConfig.FlowNoiseIntensityMax * SpeedWeighted;
		}

		if (LayerConfig.FlowNoiseTexture && LayerConfig.FlowNoiseTexture->GetResource())
		{
			FlowData.FlowNoiseTextureRHI = LayerConfig.FlowNoiseTexture->GetResource()->TextureRHI;
		}

		ENQUEUE_RENDER_COMMAND(UpdateGlobalFlow)(
		[WorldScene = GetWorld()->Scene, SolverType, FlowData = MoveTemp(FlowData)](FRHICommandListImmediate& RHICmdList)
			{
				if (WorldScene->GetRenderScene())
				{
					if (FFluid2DFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FFluid2DFieldSceneExtension>())
					{
						SceneExtension->SetGlobalFlowData_RenderThread(SolverType, FlowData);
					}
				}
			});
	}
}

AFluid2DFlowVolume* UFluid2DSubsystem::ResolveFlowVolumeAt(const FVector& WorldPos, EFluidSolverType SolverType, float& OutBlendWeight) const
{
	// Priority-first, break at first hit. Volumes are kept ordered by InsertFlowVolume so
	// higher-priority bounded volumes are tried first; unbound volumes at the tail act as fallbacks.
	OutBlendWeight = 0.0f;
	for (AFluid2DFlowVolume* Volume : FlowVolumes)
	{
		if (!Volume || !Volume->bEnabled || !Volume->AppliesTo(SolverType))
		{
			continue;
		}

		if (Volume->bUnbound)
		{
			OutBlendWeight = 1.0f;
			return Volume;
		}

		float DistanceToPoint = 0.0f;
		if (Volume->EncompassesPoint(WorldPos, 0.0f, &DistanceToPoint) && DistanceToPoint >= 0.0f)
		{
			float Weight = 1.0f;
			if (Volume->BlendRadius > 0.0f && DistanceToPoint < Volume->BlendRadius)
			{
				// Full weight deep inside; fades to 0 at the boundary.
				Weight = 1.0f - DistanceToPoint / Volume->BlendRadius;
			}
			OutBlendWeight = Weight;
			return Volume;
		}
	}
	OutBlendWeight = FMath::Clamp(OutBlendWeight, 0.0f, 1.0f);
	return nullptr;
}

FVector2D UFluid2DSubsystem::GetFlowVelocityAt(const FVector& WorldPos) const
{
	return GetFlowVelocityAt(WorldPos, FluidConfig.GetPrimarySolverType());
}

FVector2D UFluid2DSubsystem::GetFlowVelocityAt(const FVector& WorldPos, EFluidSolverType SolverType) const
{
	float BlendWeight = 0.0f;
	AFluid2DFlowVolume* Volume = ResolveFlowVolumeAt(WorldPos, SolverType, BlendWeight);
	if (!Volume)
	{
		return FVector2D::ZeroVector;
	}

	return Volume->FlowDirection.GetSafeNormal() * (Volume->Speed * BlendWeight);
}


void UFluid2DSubsystem::ResetState()
{
	if (GetWorld() && GetWorld()->Scene)
	{
		ENQUEUE_RENDER_COMMAND(UFluid2DSubsystem_ResetState)(
		[WorldScene = GetWorld()->Scene](FRHICommandListImmediate& RHICmdList)
			{
				if (WorldScene->GetRenderScene())
				{
					if (FFluid2DFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FFluid2DFieldSceneExtension>())
					{
						UE_LOG(LogFluid2DSystem, Log, TEXT("Resetting state on render thread"));
						SceneExtension->ResetState_RenderThread(RHICmdList);
					}
				}
			});

		UpdateFluidConfig();
		UpdateGlobalFlow();
	}
}
