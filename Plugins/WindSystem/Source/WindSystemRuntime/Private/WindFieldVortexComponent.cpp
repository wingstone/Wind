// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldVortexComponent.h"
#include "DrawDebugHelpers.h"

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

void UWindFieldVortexComponent::DrawDebug(float Lifetime) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetComponentLocation();
	const FVector UpAxis = GetUpVector();
	const FColor OuterColor = FColor::Cyan;
	const FColor InnerColor = FColor::Yellow;
	const FColor ArrowColor = FColor::Green;
	constexpr int32 Segments = 24;
	constexpr float Thickness = 1.5f;

	// Outer radius sphere
	DrawDebugSphere(World, Center, Radius, Segments, OuterColor, false, Lifetime, 0, Thickness);

	// Inner dead-zone sphere
	if (InnerRadius > KINDA_SMALL_NUMBER)
	{
		DrawDebugSphere(World, Center, InnerRadius, Segments / 2, InnerColor, false, Lifetime, 0, Thickness * 0.5f);
	}

	// Tangential swirl arrows around the up axis
	constexpr int32 NumArrows = 8;
	const float ArrowDist = (Radius + InnerRadius) * 0.5f;
	for (int32 i = 0; i < NumArrows; ++i)
	{
		const float Angle0 = (2.0f * PI * i) / NumArrows;
		const float Angle1 = (2.0f * PI * (i + 0.3f)) / NumArrows;

		// Build radial direction perpendicular to UpAxis
		FVector Right = FVector::CrossProduct(UpAxis, FMath::Abs(UpAxis.Z) < 0.99f ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
		FVector Forward = -FVector::CrossProduct(Right, UpAxis).GetSafeNormal();

		FVector Dir0 = (Right * FMath::Cos(Angle0) + Forward * FMath::Sin(Angle0));
		FVector Dir1 = (Right * FMath::Cos(Angle1) + Forward * FMath::Sin(Angle1));

		FVector ArrowStart = Center + Dir0 * ArrowDist;
		FVector ArrowEnd = Center + Dir1 * ArrowDist;
		DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 12.0f, ArrowColor, false, Lifetime, 0, Thickness);
	}

	// Up axis line
	DrawDebugLine(World, Center - UpAxis * Radius * 0.5f, Center + UpAxis * Radius * 0.5f, FColor::Blue, false, Lifetime, 0, Thickness);
}
