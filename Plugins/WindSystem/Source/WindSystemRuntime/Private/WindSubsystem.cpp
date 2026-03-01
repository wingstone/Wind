// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindSubsystem.h"
#include "WindFieldSourceComponent.h"
#include "WindFieldGPUData.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

DECLARE_STATS_GROUP(TEXT("WindSystem"), STATGROUP_WindSystem, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("WindSystem Tick"), STAT_WindSystem_Tick, STATGROUP_WindSystem);

// ============================================================================
// USubsystem interface
// ============================================================================

void UWindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FieldProxy = MakeShared<FWindFieldProxy>();
	FWindFieldProxyRegistry::Get().Register(GetWorld(), FieldProxy);
}

void UWindSubsystem::Deinitialize()
{
	FWindFieldProxyRegistry::Get().Unregister(GetWorld());
	FieldProxy.Reset();
	RegisteredSources.Empty();

	Super::Deinitialize();
}

void UWindSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_WindSystem_Tick);

	Super::Tick(DeltaTime);
	UpdateGPUData();
}

TStatId UWindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWindSubsystem, STATGROUP_WindSystem);
}

bool UWindSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Support game worlds, PIE, and editor preview
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor;
}

// ============================================================================
// Wind source management
// ============================================================================

void UWindSubsystem::RegisterWindSource(UWindFieldSourceComponent* Source)
{
	if (Source && !RegisteredSources.Contains(Source))
	{
		RegisteredSources.Add(Source);
	}
}

void UWindSubsystem::UnregisterWindSource(UWindFieldSourceComponent* Source)
{
	RegisteredSources.Remove(Source);
}

// ============================================================================
// CPU sampling (fallback for AI, character movement, etc.)
// ============================================================================

FWindSample UWindSubsystem::SampleWindAtLocation(FVector WorldPosition) const
{
	FWindSample Result;

	for (const UWindFieldSourceComponent* Source : RegisteredSources)
	{
		if (!Source || !Source->IsActive())
		{
			continue;
		}

		const FGPUWindSourceData GPUData = Source->ToGPUData();
		FVector3f Pos = FVector3f(WorldPosition);

		switch (static_cast<EWindFieldSourceType>(GPUData.WindType))
		{
		case EWindFieldSourceType::Directional:
		{
			FVector3f Wind = GPUData.Direction * GPUData.Strength;
			Result.WindVelocity += FVector(Wind);
			break;
		}
		case EWindFieldSourceType::Point:
		{
			FVector3f Delta = Pos - GPUData.Position;
			float Distance = Delta.Length();
			if (Distance < GPUData.Radius && Distance > KINDA_SMALL_NUMBER)
			{
				FVector3f Dir = Delta / Distance;
				float NormDist = FMath::Clamp((Distance - GPUData.InnerRadius) / FMath::Max(GPUData.Radius - GPUData.InnerRadius, 1.0f), 0.0f, 1.0f);
				float Atten = FMath::Pow(1.0f - NormDist, GPUData.FalloffExponent);
				Result.WindVelocity += FVector(Dir * GPUData.Strength * Atten);
			}
			break;
		}
		case EWindFieldSourceType::Vortex:
		{
			FVector3f Delta = Pos - GPUData.Position;
			float HorizDist = FMath::Sqrt(Delta.X * Delta.X + Delta.Z * Delta.Z);
			if (HorizDist < GPUData.Radius && HorizDist > KINDA_SMALL_NUMBER)
			{
				FVector3f Tangent(-Delta.Z / HorizDist, 0.0f, Delta.X / HorizDist);
				float NormDist = FMath::Clamp(HorizDist / GPUData.Radius, 0.0f, 1.0f);
				float InnerFade = FMath::Clamp(HorizDist / FMath::Max(GPUData.InnerRadius, 1.0f), 0.0f, 1.0f);
				float Atten = (1.0f - NormDist) * InnerFade;
				Result.WindVelocity += FVector(Tangent * GPUData.Strength * Atten);
			}
			break;
		}
		default:
			break;
		}
	}

	return Result;
}

// ============================================================================
// GPU data push
// ============================================================================

void UWindSubsystem::UpdateGPUData()
{
	if (!FieldProxy.IsValid())
	{
		return;
	}

	TArray<FGPUWindSourceData> Sources;
	Sources.Reserve(RegisteredSources.Num());

	for (const UWindFieldSourceComponent* Source : RegisteredSources)
	{
		if (Source && Source->IsActive())
		{
			Sources.Add(Source->ToGPUData());
		}
	}

	const FVector3f Center = FVector3f(GetFieldCenterPosition());
	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	FieldProxy->UpdateSources_GameThread(MoveTemp(Sources), Center, Time, WindFieldConfig);
}

FVector UWindSubsystem::GetFieldCenterPosition() const
{
	// Try to use the first player's camera position
	const UWorld* World = GetWorld();
	if (World)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			return PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	return FVector::ZeroVector;
}
