// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DFluidConfigActor.h"
#include "Fluid2DSubsystem.h"
#include "Fluid2DFluidConfigComponent.h"
#include "Fluid2DSystemSettings.h"
#include "Engine/World.h"

AFluid2DFluidConfigActor::AFluid2DFluidConfigActor()
{
	PrimaryActorTick.bCanEverTick = false;
	FluidConfigComponent = CreateDefaultSubobject<UFluid2DFluidConfigComponent>(TEXT("FluidConfigComponent"));
	RootComponent = FluidConfigComponent;
}
