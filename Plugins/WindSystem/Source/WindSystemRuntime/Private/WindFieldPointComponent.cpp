// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldPointComponent.h"
#include "DrawDebugHelpers.h"

UWindFieldPointComponent::UWindFieldPointComponent()
{
}

FGPUWindSourceData UWindFieldPointComponent::ToGPUData() const
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

void UWindFieldPointComponent::DrawDebug(float Lifetime) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetComponentLocation();
	const FColor OuterColor = FColor::Cyan;
	const FColor InnerColor = FColor::Yellow;
	const FColor ArrowColor = FColor::Green;
	constexpr int32 Segments = 24;
	constexpr float Thickness = 1.5f;

	// Outer radius sphere
	DrawDebugSphere(World, Center, Radius, Segments, OuterColor, false, Lifetime, 0, Thickness);

	// Inner radius sphere
	if (InnerRadius > KINDA_SMALL_NUMBER)
	{
		DrawDebugSphere(World, Center, InnerRadius, Segments / 2, InnerColor, false, Lifetime, 0, Thickness * 0.5f);
	}

	// Radial arrows showing wind direction (outward from center)
	constexpr int32 NumArrows = 6;
	const float ArrowDist = (Radius + InnerRadius) * 0.5f;
	for (int32 i = 0; i < NumArrows; ++i)
	{
		const float Angle = (2.0f * PI * i) / NumArrows;
		FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		FVector ArrowStart = Center + Dir * InnerRadius;
		FVector ArrowEnd = Center + Dir * ArrowDist;
		DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 15.0f, ArrowColor, false, Lifetime, 0, Thickness);
	}
}
