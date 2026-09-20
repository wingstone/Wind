// Copyright TADemo. All Rights Reserved.

#include "Actors/WeatherAddonActor.h"

#include "Engine/World.h"
#include "Subsystems/WeatherSubsystem.h"

AWeatherAddonActor::AWeatherAddonActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWeatherAddonActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->RegisterAddonActor(this);
		}
	}
}

void AWeatherAddonActor::PostUnregisterAllComponents()
{
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->UnregisterAddonActor(this);
		}
	}
	Super::PostUnregisterAllComponents();
}

#if WITH_EDITOR
void AWeatherAddonActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// AddonId / AddonAsset may have changed in the editor — re-register so the subsystem
	// rebinds the slot live instead of keeping a stale id.
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->RegisterAddonActor(this);
		}
	}
}
#endif

void AWeatherAddonActor::SetAddonWeight(float NewWeight)
{
	NewWeight = FMath::Clamp(NewWeight, 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(NewWeight, CurrentWeight))
	{
		CurrentWeight = NewWeight;
		OnWeightChanged(CurrentWeight);
	}
}
