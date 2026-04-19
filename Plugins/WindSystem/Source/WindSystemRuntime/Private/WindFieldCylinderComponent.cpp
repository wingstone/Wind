// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldCylinderComponent.h"
#include "DrawDebugHelpers.h"

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

void UWindFieldCylinderComponent::DrawDebug(float Lifetime) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetComponentLocation();
	const FVector Fwd = GetForwardVector();
	const FVector BaseCenter = Center - Fwd * HalfHeight;
	const FVector TopCenter = Center + Fwd * HalfHeight;
	const FColor OuterColor = FColor::Cyan;
	const FColor InnerColor = FColor::Yellow;
	const FColor ArrowColor = FColor::Green;
	const FColor LineColor = FColor::White;
	constexpr int32 Segments = 24;
	constexpr float Thickness = 1.5f;

	// Build local coordinate frame perpendicular to Forward
	FVector Right = FVector::CrossProduct(Fwd, FMath::Abs(Fwd.Z) < 0.99f ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
	FVector Up = FVector::CrossProduct(Right, Fwd).GetSafeNormal();

	// Draw base and top circles
	auto DrawCircle = [&](const FVector& CircleCenter, float CircleRadius, const FColor& Color, float LineThickness)
	{
		if (CircleRadius < KINDA_SMALL_NUMBER)
		{
			return;
		}
		FVector Prev = CircleCenter + Right * CircleRadius;
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Angle = (2.0f * PI * i) / Segments;
			FVector Cur = CircleCenter + (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * CircleRadius;
			DrawDebugLine(World, Prev, Cur, Color, false, Lifetime, 0, LineThickness);
			Prev = Cur;
		}
	};

	// Base circle (−Forward)
	DrawCircle(BaseCenter, Radius, OuterColor, Thickness);
	// Top circle (+Forward)
	DrawCircle(TopCenter, EndRadius, OuterColor, Thickness);

	// Connecting lines between base and top
	constexpr int32 NumLines = 8;
	for (int32 i = 0; i < NumLines; ++i)
	{
		const float Angle = (2.0f * PI * i) / NumLines;
		FVector Dir = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
		FVector BasePoint = BaseCenter + Dir * Radius;
		FVector TopPoint = TopCenter + Dir * EndRadius;
		DrawDebugLine(World, BasePoint, TopPoint, LineColor, false, Lifetime, 0, Thickness * 0.5f);
	}

	// Inner radius circles (if any)
	if (InnerRadius > KINDA_SMALL_NUMBER)
	{
		DrawCircle(BaseCenter, InnerRadius, InnerColor, Thickness * 0.5f);
		DrawCircle(TopCenter, FMath::Min(InnerRadius, EndRadius), InnerColor, Thickness * 0.5f);
	}

	// Direction arrow along the forward axis
	DrawDebugDirectionalArrow(World, BaseCenter, TopCenter, 20.0f, ArrowColor, false, Lifetime, 0, Thickness);
}
