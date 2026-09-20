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
#if WITH_EDITOR
	UFluid2DSystemSettings::OnSettingsChange.AddUObject(this, &UFluid2DSubsystem::LoadGlobalFluidConfig);
#endif
}

void UFluid2DSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UFluid2DSystemSettings* Settings = UFluid2DSystemSettings::Get();
	FluidConfig = Settings->DefaultFluidConfig;

	ResetState();

	UE_LOG(LogFluid2DSystem, Log, TEXT("Fluid2DSubsystem initialized - GridSize: %d, WorldSize: %.1f"), 
		FluidConfig.GridSize, FluidConfig.WorldSize);
}

void UFluid2DSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UFluid2DSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!FluidConfig.bEnableSimulation)
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
	const FVector ScrollTargetLocation = GetSampleLocation();
	FIntVector2 ScrollTargetLocationInt = FIntVector2(FMath::RoundToInt(ScrollTargetLocation.X), FMath::RoundToInt(ScrollTargetLocation.Y));
	
	// Update scrolling origin if view has moved significantly to maintain precision
	FIntVector2 ViewDelta = ScrollTargetLocationInt - LastScrollTargetLocationInt;
	int32 MaxOffset = FMath::Max(FMath::Abs(ViewDelta.X), FMath::Abs(ViewDelta.Y));
	if (MaxOffset > FluidConfig.WorldSize * 0.25f)
	{
		if (GetWorld() && GetWorld()->Scene)
		{
			FIntVector2 ScrollOffset = ScrollTargetLocationInt - LastScrollTargetLocationInt;

			const uint32 GridSize = FMath::Max(FluidConfig.GridSize, 1);
			const float CellSize = FluidConfig.WorldSize / static_cast<float>(GridSize);

			FIntVector2 GridScrollOffset = FIntVector2(
				FMath::RoundToInt32(ScrollOffset.X / CellSize),
				FMath::RoundToInt32(ScrollOffset.Y / CellSize));

			ENQUEUE_RENDER_COMMAND(ScrollOffset)(
			[WorldScene = GetWorld()->Scene, GridScrollOffset = GridScrollOffset](FRHICommandListImmediate& RHICmdList)
				{
					if (WorldScene->GetRenderScene())
					{
						if (FFluid2DFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FFluid2DFieldSceneExtension>())
						{
							UE_LOG(LogFluid2DSystem, Log, TEXT("Scroll offset on render thread"));
							SceneExtension->SetScrollOffset_RenderThread(GridScrollOffset);
						}
					}
				});

			LastScrollTargetLocationInt = ScrollTargetLocationInt;
		}
	}
}

void UFluid2DSubsystem::UpdateFluidConfig()
{
	if (GetWorld() && GetWorld()->Scene)
	{
		FFluid2DFluidConfig ApplyFluidConfig = FluidConfig;
		if (RegisteredFluidConfigComponents.Num() > 0 && RegisteredFluidConfigComponents[0])
		{
			// For now just take the first registered config component, we can blend them later if needed.
			// The project-settings default (loaded in Initialize / LoadGlobalFluidConfig) is used when no component is registered.
			ApplyFluidConfig = RegisteredFluidConfigComponents[0]->GetFluidConfig();
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
						UE_LOG(LogFluid2DSystem, Log, TEXT("Updating fluid config on render thread"));
						SceneExtension->SetConfig_RenderThread(FluidConfig);
					}
				}
			});
	}
}

void UFluid2DSubsystem::LoadGlobalFluidConfig(const UFluid2DSystemSettings* Settings, EPropertyChangeType::Type ChangeType)
{
	FluidConfig = Settings->DefaultFluidConfig;
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

	// Resolve which fluid config is currently in effect for texture / tiling / noise-modulation.
	const FFluid2DFluidConfig& ActiveConfig = (RegisteredFluidConfigComponents.Num() > 0 && RegisteredFluidConfigComponents[0])
		? RegisteredFluidConfigComponents[0]->FluidConfig
		: FluidConfig;

	FFluid2DGlobalFlowData FlowData;

	float BlendWeight = 0.0f;
	if (AFluid2DFlowVolume* Volume = ResolveFlowVolumeAt(GetSampleLocation(), BlendWeight))
	{
		const float SpeedWeighted = Volume->Speed * BlendWeight;
		FlowData.FlowDirection = Volume->FlowDirection.GetSafeNormal();
		FlowData.FlowNoiseIntensityMin = ActiveConfig.FlowNoiseIntensityMin * SpeedWeighted;
		FlowData.FlowNoiseIntensityMax = ActiveConfig.FlowNoiseIntensityMax * SpeedWeighted;
	}

	FlowData.FlowNoiseTiling = ActiveConfig.FlowNoiseTiling;
	if (ActiveConfig.FlowNoiseTexture && ActiveConfig.FlowNoiseTexture->GetResource())
	{
		FlowData.FlowNoiseTextureRHI = ActiveConfig.FlowNoiseTexture->GetResource()->TextureRHI;
	}

	ENQUEUE_RENDER_COMMAND(UpdateGlobalFlow)(
	[WorldScene = GetWorld()->Scene, FlowData = MoveTemp(FlowData)](FRHICommandListImmediate& RHICmdList)
		{
			if (WorldScene->GetRenderScene())
			{
				if (FFluid2DFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FFluid2DFieldSceneExtension>())
				{
					SceneExtension->SetGlobalFlowData_RenderThread(FlowData);
				}
			}
		});
}

AFluid2DFlowVolume* UFluid2DSubsystem::ResolveFlowVolumeAt(const FVector& WorldPos, float& OutBlendWeight) const
{
	// Priority-first, break at first hit. Volumes are kept ordered by InsertFlowVolume so
	// higher-priority bounded volumes are tried first; unbound volumes at the tail act as fallbacks.
	OutBlendWeight = 0.0f;
	for (AFluid2DFlowVolume* Volume : FlowVolumes)
	{
		if (!Volume || !Volume->bEnabled)
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
	float BlendWeight = 0.0f;
	AFluid2DFlowVolume* Volume = ResolveFlowVolumeAt(WorldPos, BlendWeight);
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
