// Copyright TADemo. All Rights Reserved.

#include "Actors/SeasonActor.h"

#include "Subsystems/WeatherSubsystem.h"

ASeasonActor::ASeasonActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASeasonActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->SetSeasonHostActor(this);
		}
	}
}

void ASeasonActor::PostUnregisterAllComponents()
{
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->SetSeasonHostActor(nullptr);
		}
	}
	Super::PostUnregisterAllComponents();
}
