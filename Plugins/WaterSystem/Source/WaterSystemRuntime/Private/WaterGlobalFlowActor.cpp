// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterGlobalFlowActor.h"

AWaterGlobalFlowActor::AWaterGlobalFlowActor()
{
	PrimaryActorTick.bCanEverTick = false;
	GlobalFlowComponent = CreateDefaultSubobject<UWaterGlobalFlowComponent>(TEXT("GlobalFlowComponent"));
	RootComponent = GlobalFlowComponent;
}
