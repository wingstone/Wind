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
	bool bIsSubmerged = IsSubmergedAt(CurrentPosition);

	// Continuous wave generation
	if (bIsSubmerged)
	{
		// Compare against the local flow velocity: an object drifting with the current should
		// not spawn wakes, only motion relative to the water surface should.
		FVector2D Velocity2D(CurrentVelocity.X, CurrentVelocity.Y);
		FVector2D FlowVelocity2D = FVector2D::ZeroVector;
		if (UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem())
		{
			FlowVelocity2D = Fluid2DSys->GetFlowVelocityAt(CurrentPosition, GetFlowSolverType(Fluid2DSys));
		}
		const FVector2D RelativeVelocity2D = Velocity2D - FlowVelocity2D;
		const float NewComponentVelocity = RelativeVelocity2D.Size();

		if (!bUseComponentVelocity || bUseComponentVelocity && NewComponentVelocity > MinSpeedForInteraction)
		{
			bIsInteractionUseful = true;

			CurrentInteractionData.Position = FVector2D(CurrentPosition.X, CurrentPosition.Y);
			CurrentInteractionData.PrevPosition = FVector2D(LastPosition.X, LastPosition.Y);
			CurrentInteractionData.ForceDirection = bCustomDirection ? CustomDirection.GetSafeNormal() : RelativeVelocity2D.GetSafeNormal();
			CurrentInteractionData.RadiusParameter = FVector2D(Radius, Width);
			float DirectionalStrengthFactor = bUseComponentVelocity ? NewComponentVelocity : 1.0f;
			CurrentInteractionData.StrengthParameter = FVector(DirectionalStrength * DirectionalStrengthFactor, OmniStrength, VortexStrength);
			CurrentInteractionData.HeightIntensity = HeightIntensity;
			CurrentInteractionData.PowerFalloff = PowerFalloff;
			CurrentInteractionData.ShapeType = ShapeType;
			CurrentInteractionData.EmissionType = EmissionType;
			CurrentInteractionData.TargetSolverTypes = TargetSolverTypes;
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

bool UFluid2DInteractionComponent::IsSubmergedIn(EFluidSolverType SolverType) const
{
	const UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem();
	if (!Fluid2DSys || !Fluid2DSys->IsSolverTypeEnabled(SolverType))
	{
		return false;
	}

	return GetComponentLocation().Z < Fluid2DSys->GetFluidLevel(SolverType);
}

bool UFluid2DInteractionComponent::IsSubmerged() const
{
	return IsSubmergedAt(GetComponentLocation());
}

bool UFluid2DInteractionComponent::IsSubmergedAt(const FVector& CurrentPosition) const
{
	const UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem();
	if (!Fluid2DSys)
	{
		return CurrentPosition.Z < 0;
	}

	if (TargetSolverTypes.Num() == 0)
	{
		for (int32 SolverIndex = 0; SolverIndex < Fluid2DSolverType::Count; ++SolverIndex)
		{
			const EFluidSolverType SolverType = Fluid2DSolverType::FromIndex(SolverIndex);
			if (Fluid2DSys->IsSolverTypeEnabled(SolverType) && CurrentPosition.Z < Fluid2DSys->GetFluidLevel(SolverType))
			{
				return true;
			}
		}
		return false;
	}

	for (const EFluidSolverType SolverType : TargetSolverTypes)
	{
		if (Fluid2DSys->IsSolverTypeEnabled(SolverType) && CurrentPosition.Z < Fluid2DSys->GetFluidLevel(SolverType))
		{
			return true;
		}
	}
	return false;
}
UE_ENABLE_OPTIMIZATION_SHIP

EFluidSolverType UFluid2DInteractionComponent::GetFlowSolverType(const UFluid2DSubsystem* Fluid2DSys) const
{
	if (Fluid2DSys)
	{
		for (int32 SolverIndex = 0; SolverIndex < Fluid2DSolverType::Count; ++SolverIndex)
		{
			const EFluidSolverType Candidate = Fluid2DSolverType::FromIndex(SolverIndex);
			if (TargetSolverTypes.Contains(Candidate) && Fluid2DSys->IsSolverTypeEnabled(Candidate))
			{
				return Candidate;
			}
		}

		return Fluid2DSys->FluidConfig.GetPrimarySolverType();
	}

	return EFluidSolverType::ShallowWater;
}

UFluid2DSubsystem* UFluid2DInteractionComponent::GetFluid2DSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UFluid2DSubsystem>();
	}
	return nullptr;
}
