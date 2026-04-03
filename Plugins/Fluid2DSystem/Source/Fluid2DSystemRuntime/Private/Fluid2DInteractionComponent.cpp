// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DInteractionComponent.h"
#include "Fluid2DSubsystem.h"
#include "Engine/World.h"

UFluid2DInteractionComponent::UFluid2DInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	bIsInteractionUseful = false;
}

void UFluid2DInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	LastPosition = GetComponentLocation();
	RegisterWithSubsystem();
}

void UFluid2DInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void UFluid2DInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableInteraction)
	{
		bIsInteractionUseful = false;
		return;
	}

	UpdateInteraction(DeltaTime);

	LastPosition = GetComponentLocation();
}

void UFluid2DInteractionComponent::RegisterWithSubsystem()
{
	if (UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem())
	{
		Fluid2DSys->RegisterInteractionComponent(this);
	}
}

void UFluid2DInteractionComponent::UnregisterFromSubsystem()
{
	if (UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem())
	{
		Fluid2DSys->UnregisterInteractionComponent(this);
	}
}

UE_DISABLE_OPTIMIZATION_SHIP
void UFluid2DInteractionComponent::UpdateInteraction(float DeltaTime)
{
	FVector CurrentPosition = GetComponentLocation();
	FVector CurrentVelocity = (CurrentPosition - LastPosition) / DeltaTime;
	bool bIsSubmerged = IsSubmerged(CurrentPosition);

	// Continuous wave generation
	if (bIsSubmerged)
	{
		FVector2D Velocity2D(CurrentVelocity.X, CurrentVelocity.Y);
		float NewComponentVelocity = Velocity2D.Size();

		if (NewComponentVelocity > MinSpeedForInteraction) // Minimum speed to generate waves
		{
			bIsInteractionUseful = true;

			CurrentInteractionData.Position = FVector2D(CurrentPosition.X, CurrentPosition.Y);
			CurrentInteractionData.ForceDirection = bCustomDirection ? CustomDirection.GetSafeNormal() : Velocity2D.GetSafeNormal();
			CurrentInteractionData.RadiusParameter = FVector2D(Radius, Width);
			float DirectionalStrengthFactor = bUseComponentVelocity ? NewComponentVelocity : 1.0f;
			CurrentInteractionData.StrengthParameter = FVector(DirectionalStrength * DirectionalStrengthFactor, OmniStrength, VortexStrength);
			CurrentInteractionData.HeightIntensity = HeightIntensity;
			CurrentInteractionData.PowerFalloff = PowerFalloff;
			CurrentInteractionData.ShapeType = ShapeType;
			CurrentInteractionData.EmissionType = EmissionType;
		}
		else
		{
			bIsInteractionUseful = false;
		}
	}
	else
	{
		bIsInteractionUseful = false;
	}
}

bool UFluid2DInteractionComponent::IsSubmerged(FVector CurrentPosition) const
{
	if (UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem())
	{
		const FFluid2DFluidConfig& Config = Fluid2DSys->FluidConfig;
		return CurrentPosition.Z < Config.Fluid2DLevel;
	}
	
	return CurrentPosition.Z < 0;
}
UE_ENABLE_OPTIMIZATION_SHIP

UFluid2DSubsystem* UFluid2DInteractionComponent::GetFluid2DSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UFluid2DSubsystem>();
	}
	return nullptr;
}
