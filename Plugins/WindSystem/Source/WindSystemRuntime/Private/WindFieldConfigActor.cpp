// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldConfigActor.h"
#include "WindFieldConfigComponent.h"

AWindFieldConfigActor::AWindFieldConfigActor()
{
	PrimaryActorTick.bCanEverTick = false;
	WindFieldConfigComponent = CreateDefaultSubobject<UWindFieldConfigComponent>(TEXT("WindFieldConfigComponent"));
	RootComponent = WindFieldConfigComponent;
}
