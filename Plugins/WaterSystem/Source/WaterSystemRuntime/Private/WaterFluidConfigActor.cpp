// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterFluidConfigActor.h"
#include "WaterSubsystem.h"
#include "WaterSystemSettings.h"
#include "Engine/World.h"

AWaterFluidConfigActor::AWaterFluidConfigActor()
{
	// Load defaults from Project Settings so the actor starts with matching values
	const UWaterSystemSettings* Settings = UWaterSystemSettings::Get();
	FluidConfig = Settings->DefaultFluidConfig;
	bEnableSimulation = Settings->bEnableSimulation;
}

void AWaterFluidConfigActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyConfigToSubsystem();
}

void AWaterFluidConfigActor::Destroyed()
{
	RevertConfigToDefaults();
	Super::Destroyed();
}

#if WITH_EDITOR
void AWaterFluidConfigActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyConfigToSubsystem();
}
#endif

void AWaterFluidConfigActor::ApplyConfigToSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UWaterSubsystem* Subsystem = World->GetSubsystem<UWaterSubsystem>())
		{
			Subsystem->SetFluidConfig(FluidConfig);
			if (bOverrideEnableSimulation)
			{
				Subsystem->bEnableSimulation = bEnableSimulation;
			}
		}
	}
}

void AWaterFluidConfigActor::RevertConfigToDefaults()
{
	if (UWorld* World = GetWorld())
	{
		if (UWaterSubsystem* Subsystem = World->GetSubsystem<UWaterSubsystem>())
		{
			const UWaterSystemSettings* Settings = UWaterSystemSettings::Get();
			Subsystem->SetFluidConfig(Settings->DefaultFluidConfig);
			Subsystem->bEnableSimulation = Settings->bEnableSimulation;
		}
	}
}
