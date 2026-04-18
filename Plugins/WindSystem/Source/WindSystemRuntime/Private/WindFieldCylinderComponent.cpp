// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldCylinderComponent.h"

UWindFieldCylinderComponent::UWindFieldCylinderComponent()
{
}

FGPUWindSourceData UWindFieldCylinderComponent::ToGPUData() const
{
	FGPUWindSourceData Data = {};
	Data.Position = FVector3f(GetComponentLocation());
	Data.Strength = Strength;
	Data.Direction = FVector3f(GetForwardVector());
	Data.Radius = FMath::Max(Radius, 1.0f);
	Data.InnerRadius = InnerRadius;
	Data.FalloffExponent = FalloffExponent;
	Data.WindType = static_cast<uint32>(GetWindType());
	Data.HalfHeight = FMath::Max(HalfHeight, 1.0f);
	Data.EndRadius = FMath::Max(EndRadius, 0.0f);
	return Data;
}
