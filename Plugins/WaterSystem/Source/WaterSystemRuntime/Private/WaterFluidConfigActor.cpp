// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterFluidConfigActor.h"
#include "WaterSubsystem.h"
#include "WaterFluidConfigComponent.h"
#include "WaterSystemSettings.h"
#include "Engine/World.h"

AWaterFluidConfigActor::AWaterFluidConfigActor()
{
	PrimaryActorTick.bCanEverTick = false;
	FluidConfigComponent = CreateDefaultSubobject<UWaterFluidConfigComponent>(TEXT("FluidConfigComponent"));
	RootComponent = FluidConfigComponent;
}
