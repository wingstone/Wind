// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldDirectionalComponent.h"

UWindFieldDirectionalComponent::UWindFieldDirectionalComponent()
{
	// Directional wind defaults: infinite radius, moderate strength
	Radius = 0.0f;        // 0 = infinite for directional
	Strength = 200.0f;
	GustAmount = 0.3f;
	GustFrequency = 0.5f;
	NoiseStrength = 0.2f;
	NoiseFrequency = 0.5f;
}

FGPUWindSourceData UWindFieldDirectionalComponent::ToGPUData() const
{
	FGPUWindSourceData Data = Super::ToGPUData();

	// Directional wind: direction is the component's forward vector
	Data.Direction = FVector3f(GetForwardVector());

	// Override radius to a very large value for GPU (0 is not valid for GPU math)
	if (Data.Radius <= 0.0f)
	{
		Data.Radius = 1000000.0f;
	}

	return Data;
}
