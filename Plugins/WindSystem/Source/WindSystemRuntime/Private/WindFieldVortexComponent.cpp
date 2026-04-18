// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldVortexComponent.h"

UWindFieldVortexComponent::UWindFieldVortexComponent()
{
}

FGPUWindSourceData UWindFieldVortexComponent::ToGPUData() const
{
	FGPUWindSourceData Data = {};
	Data.Position = FVector3f(GetComponentLocation());
	Data.Strength = Strength;
	Data.Direction = FVector3f(GetForwardVector());
	Data.Radius = FMath::Max(Radius, 1.0f);
	Data.InnerRadius = InnerRadius;
	Data.FalloffExponent = FalloffExponent;
	Data.WindType = static_cast<uint32>(GetWindType());
	return Data;
}
