// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldPointComponent.h"

UWindFieldPointComponent::UWindFieldPointComponent()
{
	// Point wind defaults: local area, strong falloff
	Radius = 500.0f;
	InnerRadius = 50.0f;
	Strength = 300.0f;
	FalloffExponent = 2.0f;
	GustAmount = 0.1f;
	NoiseStrength = 0.4f;
}

FGPUWindSourceData UWindFieldPointComponent::ToGPUData() const
{
	FGPUWindSourceData Data = Super::ToGPUData();

	// Point wind: ensure minimum radius for GPU math
	Data.Radius = FMath::Max(Data.Radius, 1.0f);

	return Data;
}
