// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterInteractionComponent.h"
#include "WaterSubsystem.h"
#include "Engine/World.h"

UWaterInteractionComponent::UWaterInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	bIsIntersectionUseful = false;
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
	{
		bIsIntersectionUseful = false;
		return;
	}

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

UE_DISABLE_OPTIMIZATION_SHIP
void UWaterInteractionComponent::UpdateInteraction(float DeltaTime)
{
	FVector CurrentPosition = GetComponentLocation();
	FVector CurrentVelocity = (CurrentPosition - LastPosition) / DeltaTime;
	bool bIsSubmerged = IsSubmerged(CurrentPosition);

	// Continuous wave generation
	if (bIsSubmerged)
	{
		FVector2D Velocity2D(CurrentVelocity.X, CurrentVelocity.Y);
		float Speed2D = Velocity2D.Size();

		if (Speed2D > MinSpeedForInteraction) // Minimum speed to generate waves
		{
			bIsIntersectionUseful = true;

			CurrentInteractionData.Position = FVector2D(CurrentPosition.X, CurrentPosition.Y);
			CurrentInteractionData.ForceDirection = Velocity2D.GetSafeNormal();
			CurrentInteractionData.RadiusParameter = FVector2D(Radius, Width);
			CurrentInteractionData.StrengthParameter = FVector(DirectionalStrength, OmniStrength, VortexStrength);
			CurrentInteractionData.HeightIntensity = HeightIntensity;
			CurrentInteractionData.GaussianFalloff = GaussianFalloff;
			CurrentInteractionData.ShapeType = ShapeType;
			CurrentInteractionData.EmissionType = EmissionType;
		}
		else
		{
			bIsIntersectionUseful = false;
		}
	}
	else
	{
		bIsIntersectionUseful = false;
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
UE_ENABLE_OPTIMIZATION_SHIP

UWaterSubsystem* UWaterInteractionComponent::GetWaterSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UWaterSubsystem>();
	}
	return nullptr;
}
