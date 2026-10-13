// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalTypes.h"
#include "Math/RandomStream.h"

float FPetalVariation::Hash01(int32 Index, int32 Seed, int32 Stream)
{
	// A cheap positional hash feeding FRandomStream: stable across runs and platforms, which is
	// what lets a recycled instance keep the same look forever.
	const uint32 Key = static_cast<uint32>(Seed) * 73856093u
		^ (static_cast<uint32>(Index) + 1u) * 19349663u
		^ (static_cast<uint32>(Stream) + 1u) * 83492791u;

	FRandomStream Stream0(static_cast<int32>(Key));
	return Stream0.GetFraction();
}

float FPetalVariation::MakeScale(int32 Index, int32 Seed, float BaseScale, float Jitter)
{
	const float SafeBase = FMath::Max(BaseScale, 0.0f);
	const float SafeJitter = FMath::Clamp(Jitter, 0.0f, 1.0f);
	const float Offset = (Hash01(Index, Seed, 0) * 2.0f - 1.0f) * SafeJitter;

	return SafeBase * (1.0f + Offset);
}

float FPetalVariation::MakeRollDegrees(int32 Index, int32 Seed)
{
	return Hash01(Index, Seed, 1) * 360.0f;
}

float FPetalVariation::MakeTiltDegrees(int32 Index, int32 Seed, float MaxTiltDegrees)
{
	const float MaxTilt = FMath::Max(MaxTiltDegrees, 0.0f);
	return (Hash01(Index, Seed, 2) * 2.0f - 1.0f) * MaxTilt;
}

float FPetalVariation::MakePhase(int32 Index, int32 Seed)
{
	return Hash01(Index, Seed, 3) * 2.0f * static_cast<float>(UE_PI);
}

float FPetalVariation::MakeVariationIndex(int32 Index, int32 Seed, int32 VariationCount)
{
	const int32 Count = FMath::Max(VariationCount, 1);
	return FMath::FloorToFloat(Hash01(Index, Seed, 4) * Count);
}

FTransform FPetalVariation::MakeHiddenTransform()
{
	return FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(HiddenScale));
}
