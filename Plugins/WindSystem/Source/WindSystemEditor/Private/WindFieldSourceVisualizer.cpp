// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldSourceVisualizer.h"
#include "WindFieldSourceComponent.h"
#include "WindFieldDirectionalComponent.h"
#include "WindFieldPointComponent.h"
#include "WindFieldVortexComponent.h"
#include "WindFieldCylinderComponent.h"
#include "SceneManagement.h"
#include "PrimitiveDrawingUtils.h"

static const FColor WindColorDirectional(100, 200, 255);
static const FColor WindColorPoint(255, 180, 50);
static const FColor WindColorVortex(180, 100, 255);
static const FColor WindColorCylinder(50, 220, 130);
static const FColor WindColorInner(255, 255, 100, 128);

void FWindFieldSourceVisualizer::DrawVisualization(
	const UActorComponent* Component,
	const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	// Handle standalone directional component (not a UWindFieldSourceComponent)
	if (const UWindFieldDirectionalComponent* Dir = Cast<UWindFieldDirectionalComponent>(Component))
	{
		DrawDirectionalSource(Dir->GetComponentLocation(), Dir->GetForwardVector(), Dir->Strength, PDI);
		return;
	}

	const UWindFieldSourceComponent* WindSource = Cast<UWindFieldSourceComponent>(Component);
	if (!WindSource)
	{
		return;
	}

	const FVector Location = WindSource->GetComponentLocation();

	if (const UWindFieldPointComponent* Point = Cast<UWindFieldPointComponent>(WindSource))
	{
		DrawPointSource(Location, Point->Radius, Point->InnerRadius, PDI);
	}
	else if (const UWindFieldVortexComponent* Vortex = Cast<UWindFieldVortexComponent>(WindSource))
	{
		DrawVortexSource(Location, Vortex->Radius, Vortex->InnerRadius, WindSource->GetUpVector(), PDI);
	}
	else if (const UWindFieldCylinderComponent* Cyl = Cast<UWindFieldCylinderComponent>(WindSource))
	{
		DrawCylinderSource(Location, WindSource->GetForwardVector(), Cyl->Radius, Cyl->EndRadius, Cyl->HalfHeight, Cyl->InnerRadius, PDI);
	}
}

void FWindFieldSourceVisualizer::DrawDirectionalSource(
	const FVector& Location,
	const FVector& Direction,
	float Strength,
	FPrimitiveDrawInterface* PDI)
{
	// Draw multiple parallel arrows showing wind direction
	const float ArrowLength = FMath::Clamp(Strength * 0.5f, 50.0f, 500.0f);
	const float Spacing = 100.0f;

	FVector Right = FVector::CrossProduct(Direction, FVector::UpVector);
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(Direction, FVector::RightVector);
	}
	Right.Normalize();
	FVector Up = FVector::CrossProduct(Right, Direction);
	Up.Normalize();

	for (int32 x = -1; x <= 1; ++x)
	{
		for (int32 y = -1; y <= 1; ++y)
		{
			FVector Offset = Right * (x * Spacing) + Up * (y * Spacing);
			FVector Start = Location + Offset;
			FVector End = Start + Direction * ArrowLength;

			PDI->DrawLine(Start, End, WindColorDirectional, SDPG_World, 2.0f);
			// Arrowhead
			FVector ArrowTip1 = End - Direction * 20.0f + Right * 10.0f;
			FVector ArrowTip2 = End - Direction * 20.0f - Right * 10.0f;
			PDI->DrawLine(End, ArrowTip1, WindColorDirectional, SDPG_World, 2.0f);
			PDI->DrawLine(End, ArrowTip2, WindColorDirectional, SDPG_World, 2.0f);
		}
	}
}

void FWindFieldSourceVisualizer::DrawPointSource(
	const FVector& Location,
	float Radius,
	float InnerRadius,
	FPrimitiveDrawInterface* PDI)
{
	// Outer radius sphere
	DrawWireSphere(PDI, Location, WindColorPoint, Radius, 32, SDPG_World, 1.5f);

	// Inner radius sphere
	if (InnerRadius > 0.0f)
	{
		DrawWireSphere(PDI, Location, WindColorInner, InnerRadius, 16, SDPG_World, 1.0f);
	}

	// Draw outward arrows
	const int32 ArrowCount = 8;
	for (int32 i = 0; i < ArrowCount; ++i)
	{
		float Angle = (2.0f * PI * i) / ArrowCount;
		FVector Dir(FMath::Cos(Angle), 0.0f, FMath::Sin(Angle));
		FVector Start = Location + Dir * InnerRadius;
		FVector End = Location + Dir * Radius * 0.7f;
		PDI->DrawLine(Start, End, WindColorPoint, SDPG_World, 1.5f);
	}
}

void FWindFieldSourceVisualizer::DrawVortexSource(
	const FVector& Location,
	float Radius,
	float InnerRadius,
	const FVector& UpAxis,
	FPrimitiveDrawInterface* PDI)
{
	// Draw spiral indicating rotation
	const int32 Segments = 64;
	const float Turns = 2.0f;

	FVector Right = FVector::CrossProduct(UpAxis, FVector::ForwardVector);
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(UpAxis, FVector::RightVector);
	}
	Right.Normalize();
	FVector Forward = FVector::CrossProduct(Right, UpAxis);
	Forward.Normalize();

	FVector PrevPoint = Location + Right * InnerRadius;
	for (int32 i = 1; i <= Segments; ++i)
	{
		float Alpha = static_cast<float>(i) / Segments;
		float Angle = Alpha * Turns * 2.0f * PI;
		float R = FMath::Lerp(InnerRadius, Radius, Alpha);
		FVector Point = Location + (Right * FMath::Cos(Angle) + Forward * FMath::Sin(Angle)) * R;
		PDI->DrawLine(PrevPoint, Point, WindColorVortex, SDPG_World, 1.5f);
		PrevPoint = Point;
	}

	// Outer and inner circles
	DrawCircle(PDI, Location, Right, Forward, FLinearColor(WindColorVortex), Radius, 32, SDPG_World, 1.0f);
	if (InnerRadius > 0.0f)
	{
		DrawCircle(PDI, Location, Right, Forward, FLinearColor(WindColorInner), InnerRadius, 16, SDPG_World, 1.0f);
	}
}

void FWindFieldSourceVisualizer::DrawCylinderSource(
	const FVector& Location,
	const FVector& Axis,
	float Radius,
	float EndRadius,
	float HalfHeight,
	float InnerRadius,
	FPrimitiveDrawInterface* PDI)
{
	// Build an orthonormal basis from the axis
	FVector Right = FVector::CrossProduct(Axis, FVector::UpVector);
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(Axis, FVector::RightVector);
	}
	Right.Normalize();
	FVector Up = FVector::CrossProduct(Right, Axis);
	Up.Normalize();

	const FVector BaseCenter = Location - Axis * HalfHeight;
	const FVector TopCenter  = Location + Axis * HalfHeight;

	// Base circle
	DrawCircle(PDI, BaseCenter, Right, Up, FLinearColor(WindColorCylinder), Radius, 32, SDPG_World, 1.5f);
	// Top circle
	DrawCircle(PDI, TopCenter, Right, Up, FLinearColor(WindColorCylinder), EndRadius, 32, SDPG_World, 1.5f);

	// Silhouette lines connecting base to top
	const int32 LineCount = 8;
	for (int32 i = 0; i < LineCount; ++i)
	{
		float Angle = (2.0f * PI * i) / LineCount;
		FVector Dir = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
		FVector BasePoint = BaseCenter + Dir * Radius;
		FVector TopPoint  = TopCenter  + Dir * EndRadius;
		PDI->DrawLine(BasePoint, TopPoint, WindColorCylinder, SDPG_World, 1.5f);
	}

	// Direction arrows along axis
	const int32 ArrowRings = 3;
	for (int32 r = 0; r < ArrowRings; ++r)
	{
		float t = static_cast<float>(r + 1) / (ArrowRings + 1);
		FVector Center = FMath::Lerp(BaseCenter, TopCenter, t);
		float LocalR = FMath::Lerp(Radius, EndRadius, t) * 0.5f;
		FVector ArrowEnd = Center + Axis * (HalfHeight * 0.15f);

		for (int32 i = 0; i < 4; ++i)
		{
			float Angle = (2.0f * PI * i) / 4;
			FVector Offset = (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * LocalR;
			PDI->DrawLine(Center + Offset, ArrowEnd + Offset, WindColorCylinder, SDPG_World, 1.0f);
		}
	}

	// Inner radius circles (if set)
	if (InnerRadius > 0.0f)
	{
		DrawCircle(PDI, BaseCenter, Right, Up, FLinearColor(WindColorInner), FMath::Min(InnerRadius, Radius), 16, SDPG_World, 1.0f);
		DrawCircle(PDI, TopCenter, Right, Up, FLinearColor(WindColorInner), FMath::Min(InnerRadius, EndRadius), 16, SDPG_World, 1.0f);
	}
}
