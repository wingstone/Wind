// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldSourceComponent.h"
#include "WindSubsystem.h"
#include "Engine/World.h"

UWindFieldSourceComponent::UWindFieldSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
	SetIsReplicatedByDefault(false);

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}

FGPUWindSourceData UWindFieldSourceComponent::ToGPUData() const
{
	FGPUWindSourceData Data;
	Data.Position = FVector3f(GetComponentLocation());
	Data.Strength = Strength;
	Data.Direction = FVector3f(GetForwardVector());
	Data.Radius = Radius;
	Data.InnerRadius = InnerRadius;
	Data.FalloffExponent = FalloffExponent;
	Data.WindType = static_cast<uint32>(GetWindType());
	Data.GustAmount = GustAmount;
	Data.GustFrequency = GustFrequency;
	Data.NoiseStrength = NoiseStrength;
	Data.NoiseFrequency = NoiseFrequency;
	Data.Padding = 0.0f;
	return Data;
}

void UWindFieldSourceComponent::OnRegister()
{
	Super::OnRegister();
	RegisterWithSubsystem();
}

void UWindFieldSourceComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

void UWindFieldSourceComponent::RegisterWithSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UWindSubsystem* Subsystem = World->GetSubsystem<UWindSubsystem>())
		{
			Subsystem->RegisterWindSource(this);
		}
	}
}

void UWindFieldSourceComponent::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UWindSubsystem* Subsystem = World->GetSubsystem<UWindSubsystem>())
		{
			Subsystem->UnregisterWindSource(this);
		}
	}
}
