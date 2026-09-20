// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DFlowVolume.h"
#include "Fluid2DSubsystem.h"
#include "Components/BrushComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"

AFluid2DFlowVolume::AFluid2DFlowVolume()
	: Super()
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// Volume needs physics data so EncompassesPoint can return a valid distance-to-collision.
	GetBrushComponent()->bAlwaysCreatePhysicsState = true;
	GetBrushComponent()->Mobility = EComponentMobility::Movable;

	bEnabled = true;
	bUnbound = false;
	Priority = 0.0f;
	BlendRadius = 100.0f;
	FlowDirection = FVector2D(1.0, 0.0);
	Speed = 100.0f;
}

void AFluid2DFlowVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if (UWorld* World = GetWorld())
	{
		if (UFluid2DSubsystem* Fluid2DSys = World->GetSubsystem<UFluid2DSubsystem>())
		{
			Fluid2DSys->InsertFlowVolume(this);
		}
	}
}

void AFluid2DFlowVolume::PostUnregisterAllComponents()
{
	if (UWorld* World = GetWorld())
	{
		if (UFluid2DSubsystem* Fluid2DSys = World->GetSubsystem<UFluid2DSubsystem>())
		{
			Fluid2DSys->RemoveFlowVolume(this);
		}
	}

	Super::PostUnregisterAllComponents();
}
