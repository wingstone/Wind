// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterInteractionComponent.h"
#include "WaterSubsystem.h"
#include "Engine/World.h"

UWaterInteractionComponent::UWaterInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
}

void UWaterInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	LastPosition = GetComponentLocation();
	RegisterWithSubsystem();
}

void UWaterInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void UWaterInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableInteraction)
		return;

	UpdateInteraction(DeltaTime);

	LastPosition = GetComponentLocation();
}

void UWaterInteractionComponent::RegisterWithSubsystem()
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		WaterSys->RegisterInteractionComponent(this);
	}
}

void UWaterInteractionComponent::UnregisterFromSubsystem()
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		WaterSys->UnregisterInteractionComponent(this);
	}
}

void UWaterInteractionComponent::UpdateInteraction(float DeltaTime)
{
	UWaterSubsystem* WaterSys = GetWaterSubsystem();
	if (!WaterSys)
		return;

	FVector CurrentPosition = GetComponentLocation();
	FVector CurrentVelocity = (CurrentPosition - LastPosition) / DeltaTime;
	bool bIsSubmerged = IsSubmerged(CurrentPosition);

	// Continuous wave generation
	if (bIsSubmerged)
	{
		FVector2D Velocity2D(CurrentVelocity.X, CurrentVelocity.Y);
		float Speed2D = Velocity2D.Size();

		if (Speed2D > 10.0f) // Minimum speed to generate waves
		{
			FWaterInteractionData Interaction;

			Interaction.Position = FVector2D(CurrentPosition.X, CurrentPosition.Y);
			Interaction.ForceDirection = Velocity2D.GetSafeNormal();
			Interaction.RadiusParameter = FVector2D(Radius, Width);
			Interaction.StrengthParameter = FVector(DisectionalStrength, OmniStrength, VortexStrength);
			Interaction.GaussianFalloff = GaussianFalloff;
			Interaction.ShapeType = ShapeType;
			Interaction.EmissionType = EmissionType;
			
			WaterSys->ApplyInteraction(Interaction);
		}
	}
}

bool UWaterInteractionComponent::IsSubmerged(FVector CurrentPosition) const
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		const FWaterFluidConfig& Config = WaterSys->FluidConfig;
		return CurrentPosition.Z < Config.WaterLevel;
	}

	return CurrentPosition.Z < 0;
}

UWaterSubsystem* UWaterInteractionComponent::GetWaterSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UWaterSubsystem>();
	}
	return nullptr;
}
