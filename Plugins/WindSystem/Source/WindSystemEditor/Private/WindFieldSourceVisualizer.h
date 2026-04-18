// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class FPrimitiveDrawInterface;
class FSceneView;
class UActorComponent;

/**
 * Editor visualizer for wind field source components.
 * Draws influence radius, direction arrows, and per-type indicators.
 */
class FWindFieldSourceVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

private:
	void DrawDirectionalSource(const FVector& Location, const FVector& Direction, float Strength, FPrimitiveDrawInterface* PDI);
	void DrawPointSource(const FVector& Location, float Radius, float InnerRadius, FPrimitiveDrawInterface* PDI);
	void DrawVortexSource(const FVector& Location, float Radius, float InnerRadius, const FVector& UpAxis, FPrimitiveDrawInterface* PDI);
	void DrawCylinderSource(const FVector& Location, const FVector& Axis, float Radius, float EndRadius, float HalfHeight, float InnerRadius, FPrimitiveDrawInterface* PDI);
};
