// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DGlobalFlowActor.h"

AFluid2DGlobalFlowActor::AFluid2DGlobalFlowActor()
{
	PrimaryActorTick.bCanEverTick = false;
	GlobalFlowComponent = CreateDefaultSubobject<UFluid2DGlobalFlowComponent>(TEXT("GlobalFlowComponent"));
	RootComponent = GlobalFlowComponent;
}
