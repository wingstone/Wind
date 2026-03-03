// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterSubsystem.h"
#include "WaterFluidGPUData.h"
#include "WaterInteractionComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaterSystem, Log, All);

void UWaterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Create GPU data manager
	GPUData = MakeShared<FWaterFluidGPUData>();
	GPUData->SetConfig(FluidConfig);

	// Initialize cached fields
	InitializeCachedFields();

	UE_LOG(LogWaterSystem, Log, TEXT("WaterSubsystem initialized - GridSize: %d, WorldSize: %.1f"), 
		FluidConfig.GridSize, FluidConfig.WorldSize);
}

void UWaterSubsystem::Deinitialize()
{
	InteractionComponents.Empty();
	CachedHeightField.Empty();
	CachedVelocityField.Empty();
	GPUData.Reset();

	Super::Deinitialize();
}

void UWaterSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEnableSimulation || !GPUData.IsValid())
		return;

	// Update GPU data
	UpdateGPUData(DeltaTime);
}

TStatId UWaterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWaterSubsystem, STATGROUP_Tickables);
}

bool UWaterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

FWaterSample UWaterSubsystem::SampleWaterAtLocation(FVector2D WorldPosition) const
{
	FWaterSample Sample;

	if (!GPUData.IsValid())
		return Sample;

	// Sample from cached CPU field (approximation)
	Sample.Height = SampleCachedHeight(WorldPosition);
	Sample.Velocity = SampleCachedVelocity(WorldPosition);

	// Calculate pressure (hydrostatic approximation)
	Sample.Pressure = Sample.Height * FluidConfig.Density * FluidConfig.Gravity;

	// Calculate normal from height gradient (for rendering)
	const float Delta = FluidConfig.WorldSize / FluidConfig.GridSize;
	float HeightXP = SampleCachedHeight(WorldPosition + FVector2D(Delta, 0));
	float HeightXM = SampleCachedHeight(WorldPosition - FVector2D(Delta, 0));
	float HeightYP = SampleCachedHeight(WorldPosition + FVector2D(0, Delta));
	float HeightYM = SampleCachedHeight(WorldPosition - FVector2D(0, Delta));

	FVector Gradient;
	Gradient.X = (HeightXP - HeightXM) / (2.0f * Delta);
	Gradient.Y = (HeightYP - HeightYM) / (2.0f * Delta);
	Gradient.Z = 1.0f;

	Sample.Normal = Gradient.GetSafeNormal();

	return Sample;
}

bool UWaterSubsystem::IsUnderwater(FVector WorldPosition) const
{
	FVector2D Position2D(WorldPosition.X, WorldPosition.Y);
	float WaterHeight = GetWaterHeight(Position2D);
	return WorldPosition.Z < WaterHeight;
}

float UWaterSubsystem::GetWaterHeight(FVector2D WorldPosition) const
{
	return SampleCachedHeight(WorldPosition);
}

void UWaterSubsystem::CreateSplash(FVector2D Position, float Strength, float Radius, float Duration)
{
	if (GPUData.IsValid())
	{
		GPUData->AddDisturbance(Position, Strength, Radius, Duration);
		UE_LOG(LogWaterSystem, Verbose, TEXT("Created splash at (%.1f, %.1f) - Strength: %.1f, Radius: %.1f"), 
			Position.X, Position.Y, Strength, Radius);
	}
}

void UWaterSubsystem::ApplyInteraction(const FWaterInteractionData& Interaction)
{
	if (GPUData.IsValid())
	{
		GPUData->AddInteraction(Interaction);
	}
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

void UWaterSubsystem::UpdateGPUData(float DeltaTime)
{
	if (!GPUData.IsValid())
		return;

	// Update GPU data
	GPUData->SetConfig(FluidConfig);
	GPUData->Update(DeltaTime);

	// TODO: Push to render thread via proxy
	// ENQUEUE_RENDER_COMMAND(UpdateWaterField)([...]{ ... });

	// For now, just clear processed interactions
	GPUData->ClearInteractions();
}

FVector2D UWaterSubsystem::GetFieldCenterPosition() const
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			FVector CamLoc;
			FRotator CamRot;
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			return FVector2D(CamLoc.X, CamLoc.Y);
		}
	}
	return FVector2D::ZeroVector;
}

FIntPoint UWaterSubsystem::WorldToGrid(FVector2D WorldPosition) const
{
	FVector2D Center = GetFieldCenterPosition();
	FVector2D LocalPos = WorldPosition - Center;

	float CellSize = FluidConfig.WorldSize / FluidConfig.GridSize;
	int32 X = FMath::FloorToInt(LocalPos.X / CellSize) + FluidConfig.GridSize / 2;
	int32 Y = FMath::FloorToInt(LocalPos.Y / CellSize) + FluidConfig.GridSize / 2;

	return FIntPoint(X, Y);
}

FVector2D UWaterSubsystem::GridToWorld(FIntPoint GridPosition) const
{
	FVector2D Center = GetFieldCenterPosition();
	float CellSize = FluidConfig.WorldSize / FluidConfig.GridSize;

	FVector2D LocalPos;
	LocalPos.X = (GridPosition.X - FluidConfig.GridSize / 2) * CellSize;
	LocalPos.Y = (GridPosition.Y - FluidConfig.GridSize / 2) * CellSize;

	return Center + LocalPos;
}

void UWaterSubsystem::InitializeCachedFields()
{
	int32 TotalCells = FluidConfig.GridSize * FluidConfig.GridSize;
	CachedHeightField.SetNumZeroed(TotalCells);
	CachedVelocityField.SetNumZeroed(TotalCells);
}

float UWaterSubsystem::SampleCachedHeight(FVector2D WorldPosition) const
{
	FIntPoint GridPos = WorldToGrid(WorldPosition);

	if (GridPos.X < 0 || GridPos.X >= FluidConfig.GridSize ||
		GridPos.Y < 0 || GridPos.Y >= FluidConfig.GridSize)
	{
		return 0.0f;
	}

	int32 Index = GridPos.Y * FluidConfig.GridSize + GridPos.X;
	if (Index >= 0 && Index < CachedHeightField.Num())
	{
		return CachedHeightField[Index];
	}

	return 0.0f;
}

FVector2D UWaterSubsystem::SampleCachedVelocity(FVector2D WorldPosition) const
{
	FIntPoint GridPos = WorldToGrid(WorldPosition);

	if (GridPos.X < 0 || GridPos.X >= FluidConfig.GridSize ||
		GridPos.Y < 0 || GridPos.Y >= FluidConfig.GridSize)
	{
		return FVector2D::ZeroVector;
	}

	int32 Index = GridPos.Y * FluidConfig.GridSize + GridPos.X;
	if (Index >= 0 && Index < CachedVelocityField.Num())
	{
		return CachedVelocityField[Index];
	}

	return FVector2D::ZeroVector;
}
