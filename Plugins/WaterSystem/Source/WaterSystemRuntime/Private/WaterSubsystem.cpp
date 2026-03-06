// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterSubsystem.h"
#include "WaterInteractionComponent.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "RendererInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaterSystem, Log, All);

// Forward declare to avoid circular dependency
class FWaterFieldSceneExtension;

void UWaterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogWaterSystem, Log, TEXT("WaterSubsystem initialized - GridSize: %d, WorldSize: %.1f"), 
		FluidConfig.GridSize, FluidConfig.WorldSize);
}

void UWaterSubsystem::Deinitialize()
{
	InteractionComponents.Empty();

	Super::Deinitialize();
}

void UWaterSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEnableSimulation)
		return;

	// Update scene extension on render thread
	UpdateSceneExtension(DeltaTime);
}

TStatId UWaterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWaterSubsystem, STATGROUP_Tickables);
}

bool UWaterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor || WorldType == EWorldType::GamePreview;
}

void UWaterSubsystem::CreateSplash(FVector2D Position, float Strength, float Radius, float Duration)
{
	SendDisturbance(Position, Strength, Radius, Duration);
	UE_LOG(LogWaterSystem, Verbose, TEXT("Created splash at (%.1f, %.1f) - Strength: %.1f, Radius: %.1f"), Position.X, Position.Y, Strength, Radius);
}

void UWaterSubsystem::ApplyInteraction(const FWaterInteractionData& Interaction)
{
	SendInteraction(Interaction);
}

void UWaterSubsystem::RegisterInteractionComponent(UWaterInteractionComponent* Component)
{
	if (Component && !InteractionComponents.Contains(Component))
	{
		InteractionComponents.Add(Component);
		UE_LOG(LogWaterSystem, Verbose, TEXT("Registered interaction component: %s"), *Component->GetName());
	}
}

void UWaterSubsystem::UnregisterInteractionComponent(UWaterInteractionComponent* Component)
{
	if (Component)
	{
		InteractionComponents.Remove(Component);
		UE_LOG(LogWaterSystem, Verbose, TEXT("Unregistered interaction component: %s"), *Component->GetName());
	}
}

FWaterFieldSceneExtension* UWaterSubsystem::GetSceneExtension() const
{
	if (UWorld* World = GetWorld())
	{
		if (FSceneInterface* Scene = World->Scene)
		{
			// Get the scene extension - this would be registered by the renderer module
			// For now, return nullptr as placeholder
			// TODO: Implement proper scene extension retrieval
			return nullptr;
		}
	}
	return nullptr;
}

void UWaterSubsystem::UpdateSceneExtension(float DeltaTime)
{
	// Send configuration updates to render thread via ENQUEUE_RENDER_COMMAND
	FWaterFluidConfig ConfigCopy = FluidConfig;
	
	ENQUEUE_RENDER_COMMAND(UpdateWaterConfig)(
		[ConfigCopy](FRHICommandListImmediate& RHICmdList)
		{
			// TODO: Get scene extension from scene and update config
			// SceneExtension->SetConfig(ConfigCopy);
		});
}

void UWaterSubsystem::SendDisturbance(const FVector2D& Position, float Strength, float Radius, float Duration)
{
	// Enqueue command to render thread
	ENQUEUE_RENDER_COMMAND(AddWaterDisturbance)(
		[Position, Strength, Radius, Duration](FRHICommandListImmediate& RHICmdList)
		{
			// TODO: Get scene extension and add disturbance
			// SceneExtension->AddDisturbance(Position, Strength, Radius, Duration);
		});
}

void UWaterSubsystem::SendInteraction(const FWaterInteractionData& Interaction)
{
	// Copy interaction data
	FWaterInteractionData InteractionCopy = Interaction;
	
	ENQUEUE_RENDER_COMMAND(AddWaterInteraction)(
		[InteractionCopy](FRHICommandListImmediate& RHICmdList)
		{
			// TODO: Get scene extension and add interaction
			// SceneExtension->AddInteraction(InteractionCopy);
		});
}
