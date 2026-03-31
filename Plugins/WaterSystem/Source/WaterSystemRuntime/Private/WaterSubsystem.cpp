// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterSubsystem.h"
#include "WaterSystemSettings.h"
#include "WaterFieldSceneExtension.h"
#include "WaterInteractionComponent.h"
#include "WaterFluidConfigComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SceneInterface.h"
#include "ScenePrivate.h"
#include "RendererInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaterSystem, Log, All);

// Forward declare to avoid circular dependency
class FWaterFieldSceneExtension;

UWaterSubsystem::UWaterSubsystem()
{
#if WITH_EDITOR
	UWaterSystemSettings::OnSettingsChange.AddUObject(this, &UWaterSubsystem::LoadGlobalFluidConfig);
#endif
}

void UWaterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UWaterSystemSettings* Settings = UWaterSystemSettings::Get();
	FluidConfig = Settings->DefaultFluidConfig;

	ResetState();

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

	if (!FluidConfig.bEnableSimulation)
		return;

	ScrollWorldGrid();
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

void UWaterSubsystem::RegisterInteractionComponent(UWaterInteractionComponent* Component)
{
	RegisteredInteractionComponents.AddUnique(Component);
}

void UWaterSubsystem::UnregisterInteractionComponent(UWaterInteractionComponent* Component)
{
	RegisteredInteractionComponents.Remove(Component);
}

void UWaterSubsystem::RegisterFluidConfigComponent(UWaterFluidConfigComponent* Component)
{
	RegisteredFluidConfigComponents.AddUnique(Component);
}

void UWaterSubsystem::UnregisterFluidConfigComponent(UWaterFluidConfigComponent* Component)
{
	RegisteredFluidConfigComponents.Remove(Component);
}

void UWaterSubsystem::ScrollWorldGrid()
{
	// Prefer the player pawn location so the simulation follows gameplay position.
	FVector ScrollTargetLocation = FVector::ZeroVector;
	if (UWorld* World = GetWorld())
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		if (PlayerController != nullptr)
		{
			if (const APawn* PlayerPawn = PlayerController->GetPawn())
			{
				ScrollTargetLocation = PlayerPawn->GetActorLocation();
			}
			else if (PlayerController->PlayerCameraManager != nullptr)
			{
				ScrollTargetLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
			}
		}
		else
		{
			auto ViewLocations = World->ViewLocationsRenderedLastFrame;
			if (ViewLocations.Num() > 0)
			{
				ScrollTargetLocation = ViewLocations[0];
			}
		}
	}

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
						if (FWaterFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FWaterFieldSceneExtension>())
						{
							UE_LOG(LogWaterSystem, Log, TEXT("Scroll offset on render thread"));
							SceneExtension->SetScrollOffset_RenderThread(GridScrollOffset);
						}
					}
				});

			LastScrollTargetLocationInt = ScrollTargetLocationInt;
		}
	}
}

void UWaterSubsystem::UpdateFluidConfig()
{
	if (GetWorld() && GetWorld()->Scene)
	{
		FWaterFluidConfig ApplyFluidConfig = FluidConfig;
		if (RegisteredFluidConfigComponents.Num() > 0)
		{
			// For now just take the first registered config component, we can blend them later if needed
			ApplyFluidConfig = RegisteredFluidConfigComponents[0]->GetFluidConfig();
		}
		ENQUEUE_RENDER_COMMAND(UpdateFluidConfig)(
		[WorldScene = GetWorld()->Scene, FluidConfig = ApplyFluidConfig](FRHICommandListImmediate& RHICmdList)
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

void UWaterSubsystem::LoadGlobalFluidConfig(const UWaterSystemSettings* Settings, EPropertyChangeType::Type ChangeType)
{
	FluidConfig = Settings->DefaultFluidConfig;
	ResetState();
}

void UWaterSubsystem::UpdateInteractions()
{
	if (GetWorld() && GetWorld()->Scene && RegisteredInteractionComponents.Num() > 0)
	{
		TArray<FWaterInteractionData> InteractionsToApply;
		for (UWaterInteractionComponent* Component : RegisteredInteractionComponents)
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
						if (FWaterFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FWaterFieldSceneExtension>())
						{
							SceneExtension->SetInteractionsToApply_RenderThread(InteractionsToApply);
						}
					}
				});
		}
	}
}


void UWaterSubsystem::ResetState()
{
	if (GetWorld() && GetWorld()->Scene)
	{
		ENQUEUE_RENDER_COMMAND(UWaterSubsystem_ResetState)(
		[WorldScene = GetWorld()->Scene](FRHICommandListImmediate& RHICmdList)
			{
				if (WorldScene->GetRenderScene())
				{
					if (FWaterFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FWaterFieldSceneExtension>())
					{
						UE_LOG(LogWaterSystem, Log, TEXT("Resetting state on render thread"));
						SceneExtension->ResetState_RenderThread(RHICmdList);
					}
				}
			});

		UpdateFluidConfig();
	}
}
