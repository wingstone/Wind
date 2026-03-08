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

void UWaterSubsystem::UpdateFluidConfig(float DeltaTime)
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
						SceneExtension->SetConfig(FluidConfig);
					}
				}
			});
	}
}

void UWaterSubsystem::SendInteraction(const FWaterInteractionData& Interaction)
{
	if (GetWorld() && GetWorld()->Scene)
	{
		ENQUEUE_RENDER_COMMAND(SendInteraction)(
		[WorldScene = GetWorld()->Scene, Interaction](FRHICommandListImmediate& RHICmdList)
			{
				if (WorldScene->GetRenderScene())
				{
					if (FWaterFieldSceneExtension* SceneExtension = WorldScene->GetRenderScene()->GetExtensionPtr<FWaterFieldSceneExtension>())
					{
						UE_LOG(LogWaterSystem, Log, TEXT("Sending interaction on render thread"));
						SceneExtension->AddInteraction(Interaction);
					}
				}
			});
	}
}
