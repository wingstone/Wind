// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterFluidGPUData.h"

FWaterFluidGPUData::FWaterFluidGPUData()
	: SimulationTime(0.0f)
{
}

FWaterFluidGPUData::~FWaterFluidGPUData()
{
}

void FWaterFluidGPUData::SetConfig(const FWaterFluidConfig& InConfig)
{
	Config = InConfig;
}

void FWaterFluidGPUData::AddDisturbance(const FVector2D& Position, float Strength, float Radius, float Duration)
{
	FGPUWaterDisturbance Disturbance;
	Disturbance.Position = Position;
	Disturbance.Strength = Strength;
	Disturbance.Radius = Radius;
	Disturbance.Duration = Duration;
	Disturbance.ElapsedTime = 0.0f;
	
	ActiveDisturbances.Add(Disturbance);
}

void FWaterFluidGPUData::AddInteraction(const FWaterInteractionData& Interaction)
{
	PendingInteractions.Add(Interaction);
}

void FWaterFluidGPUData::Update(float DeltaTime)
{
	SimulationTime += DeltaTime;
	UpdateDisturbances(DeltaTime);
}

void FWaterFluidGPUData::UpdateDisturbances(float DeltaTime)
{
	// Update and remove expired disturbances
	for (int32 i = ActiveDisturbances.Num() - 1; i >= 0; --i)
	{
		ActiveDisturbances[i].ElapsedTime += DeltaTime;
		
		if (ActiveDisturbances[i].ElapsedTime >= ActiveDisturbances[i].Duration)
		{
			ActiveDisturbances.RemoveAtSwap(i);
		}
	}
}

void FWaterFluidGPUData::ClearInteractions()
{
	PendingInteractions.Empty();
}
