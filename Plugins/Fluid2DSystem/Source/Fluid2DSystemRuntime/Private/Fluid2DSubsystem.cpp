// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Fluid2DSystemSettings.h"
#include "Fluid2DFieldSceneExtension.h"
#include "Fluid2DInteractionComponent.h"
#include "Fluid2DFluidConfigComponent.h"
#include "Fluid2DGlobalFlowComponent.h"
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
}

void UFluid2DSubsystem::RegisterGlobalFlowComponent(UFluid2DGlobalFlowComponent* Component)
{
	RegisteredGlobalFlowComponents.AddUnique(Component);
}

void UFluid2DSubsystem::UnregisterGlobalFlowComponent(UFluid2DGlobalFlowComponent* Component)
{
	RegisteredGlobalFlowComponents.Remove(Component);
}

void UFluid2DSubsystem::ScrollWorldGrid()
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
	if (!GetWorld() || !GetWorld()->Scene || RegisteredGlobalFlowComponents.Num() == 0)
	{
		return;
	}

	// Collect the first active global flow component
	FFluid2DGlobalFlowData FlowData;
	for (UFluid2DGlobalFlowComponent* Component : RegisteredGlobalFlowComponents)
	{
		if (Component && Component->IsFlowEnabled())
		{
			FlowData = Component->GetGlobalFlowData();
			break;
		}
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
