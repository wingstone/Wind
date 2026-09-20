// Copyright TADemo. All Rights Reserved.

#include "Actors/LocalWeatherVolume.h"

#include "Components/BrushComponent.h"
#include "Subsystems/WeatherSubsystem.h"

ALocalWeatherVolume::ALocalWeatherVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (UBrushComponent* BrushComp = GetBrushComponent())
	{
		BrushComp->SetGenerateOverlapEvents(false);
		BrushComp->SetCollisionProfileName(TEXT("NoCollision"));
	}
}

float ALocalWeatherVolume::GetInfluenceWeight(const FVector& WorldLocation)
{
	UBrushComponent* BrushComp = GetBrushComponent();
	if (!BrushComp) return 0.0f;

	// Fully inside?
	if (EncompassesPoint(WorldLocation))
	{
		return 1.0f;
	}
	if (FalloffDistance <= 0.0f)
	{
		return 0.0f;
	}

	FVector ClosestPoint;
	const float DistOutside = BrushComp->GetDistanceToCollision(WorldLocation, ClosestPoint);
	if (DistOutside < 0.0f) return 0.0f; // invalid (no collision data)
	if (DistOutside >= FalloffDistance) return 0.0f;
	return FMath::Clamp(1.0f - (DistOutside / FalloffDistance), 0.0f, 1.0f);
}

void ALocalWeatherVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->RegisterLocalVolume(this);
		}
	}
}

void ALocalWeatherVolume::PostUnregisterAllComponents()
{
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->UnregisterLocalVolume(this);
		}
	}
	Super::PostUnregisterAllComponents();
}
