// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterSubsystem.h"
#include "WaterSystemSettings.h"
#include "WaterFieldSceneExtension.h"
#include "WaterInteractionComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SceneInterface.h"
#include "ScenePrivate.h"
#include "RendererInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaterSystem, Log, All);

// Forward declare to avoid circular dependency
class FWaterFieldSceneExtension;

void UWaterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UWaterSystemSettings* Settings = UWaterSystemSettings::Get();
	FluidConfig = Settings->DefaultFluidConfig;
	bEnableSimulation = Settings->bEnableSimulation;

	UE_LOG(LogWaterSystem, Log, TEXT("WaterSubsystem initialized - GridSize: %d, WorldSize: %.1f"), 
		FluidConfig.GridSize, FluidConfig.WorldSize);
}

void UWaterSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UWaterSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEnableSimulation)
		return;

	// Get view location
	FVector ViewLocation = FVector::ZeroVector;
	if (UWorld* World = GetWorld())
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		if (PlayerController != nullptr)
		{
			ViewLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
		}
		else
		{
			auto ViewLocations = World->ViewLocationsRenderedLastFrame;
			if (ViewLocations.Num() > 0)
			{
				ViewLocation = ViewLocations[0];
			}
		}
	}

	FIntVector2 ViewLocationInt = FIntVector2(FMath::RoundToInt(ViewLocation.X), FMath::RoundToInt(ViewLocation.Y));
	
	// Update scrolling origin if view has moved significantly to maintain precision
	FIntVector2 ViewDelta = ViewLocationInt - LastViewLocationInt;
	int64 ViewDistSq = static_cast<int64>(ViewDelta.X) * ViewDelta.X + static_cast<int64>(ViewDelta.Y) * ViewDelta.Y;
	if (ViewDistSq > FMath::Square(FluidConfig.WorldSize * 0.25f))
	{
		if (GetWorld() && GetWorld()->Scene)
		{
			FIntVector2 ScrollOffset = ViewLocationInt - LastViewLocationInt;

			const uint32 GridSize = FMath::Max(FluidConfig.GridSize, 1);
			const float CellSize = FluidConfig.WorldSize / static_cast<float>(GridSize);

			FIntVector2 GridScrollOffset = FIntVector2(
				FMath::RoundToInt32(ScrollOffset.X / CellSize),
				FMath::RoundToInt32(ScrollOffset.Y / CellSize));

			ENQUEUE_RENDER_COMMAND(UpdateFluidConfig)(
			[WorldScene = GetWorld()->Scene, GridScrollOffset = GridScrollOffset](FRHICommandListImmediate& RHICmdList)
				{
					if (WorldScene->GetRenderScene())
					{
						if (FWaterFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FWaterFieldSceneExtension>())
						{
							UE_LOG(LogWaterSystem, Log, TEXT("Updating fluid config on render thread"));
							SceneExtension->SetScrollOffset_RenderThread(GridScrollOffset);
						}
					}
				});

			LastViewLocationInt = ViewLocationInt;
		}
	}
	
	UpdateInteractions();
}

TStatId UWaterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWaterSubsystem, STATGROUP_Tickables);
}

bool UWaterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor || WorldType == EWorldType::GamePreview;
}

void UWaterSubsystem::SetFluidConfig(const FWaterFluidConfig& NewConfig)
{
	FluidConfig = NewConfig;
	UpdateFluidConfig();
}

void UWaterSubsystem::ApplyInteraction(const FWaterInteractionData& Interaction)
{
	PendingInteractions.Add(Interaction);
}

void UWaterSubsystem::RegisterInteractionComponent(UWaterInteractionComponent* Component)
{
	// No-op: interactions are applied per-frame via ApplyInteraction
}

void UWaterSubsystem::UnregisterInteractionComponent(UWaterInteractionComponent* Component)
{
	// No-op: interactions are applied per-frame via ApplyInteraction
}

void UWaterSubsystem::UpdateFluidConfig()
{
	if (GetWorld() && GetWorld()->Scene)
	{
		ENQUEUE_RENDER_COMMAND(UpdateFluidConfig)(
		[WorldScene = GetWorld()->Scene, FluidConfig = FluidConfig](FRHICommandListImmediate& RHICmdList)
			{
				if (WorldScene->GetRenderScene())
				{
					if (FWaterFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FWaterFieldSceneExtension>())
					{
						UE_LOG(LogWaterSystem, Log, TEXT("Updating fluid config on render thread"));
						SceneExtension->SetConfig_RenderThread(FluidConfig);
					}
				}
			});
	}
}

void UWaterSubsystem::UpdateInteractions()
{
	if (GetWorld() && GetWorld()->Scene)
	{
		ENQUEUE_RENDER_COMMAND(UpdateInteractions)(
		[WorldScene = GetWorld()->Scene, PendingInteractions = PendingInteractions](FRHICommandListImmediate& RHICmdList) 
			{
				if (WorldScene->GetRenderScene())
				{
					if (FWaterFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FWaterFieldSceneExtension>())
					{
						SceneExtension->SetPendingInteractions_RenderThread(PendingInteractions);
					}
				}
			});
	}
}


void UWaterSubsystem::ResetState()
{
	if (GetWorld() && GetWorld()->Scene)
	{
		ENQUEUE_RENDER_COMMAND(UWaterSubsystem_ResetState)(
		[WorldScene = GetWorld()->Scene, bEnableSimulation = bEnableSimulation](FRHICommandListImmediate& RHICmdList)
			{
				if (WorldScene->GetRenderScene())
				{
					if (FWaterFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FWaterFieldSceneExtension>())
					{
						UE_LOG(LogWaterSystem, Log, TEXT("Resetting state on render thread"));
						SceneExtension->ResetState_RenderThread(RHICmdList, bEnableSimulation);
					}
				}
			});
	}
}
