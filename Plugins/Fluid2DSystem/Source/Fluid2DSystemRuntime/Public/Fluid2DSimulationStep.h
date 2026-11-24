// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Fixed-step scheduling for the fluid simulation.
 *
 * The simulation advances in fixed steps of TimeStep instead of once per frame, so its update rate is
 * independent of the frame rate: at a high frame rate most frames run no step at all, at a low frame rate
 * a single frame runs several steps to catch up with the elapsed real time.
 */
struct FFluid2DFixedStepAccumulator
{
	/** Real time (seconds) accumulated since the last simulated step. Never reaches StepTime after Accumulate(). */
	float PendingTime = 0.0f;

	/**
	 * Adds DeltaTime and returns how many whole fixed steps should be simulated this frame.
	 * The backlog is clamped to MaxSteps so a long hitch cannot snowball into an unbounded catch-up burst.
	 */
	int32 Accumulate(float DeltaTime, float StepTime, int32 MaxSteps)
	{
		const float ClampedStepTime = FMath::Max(StepTime, 1e-4f);
		const int32 ClampedMaxSteps = FMath::Max(MaxSteps, 1);

		PendingTime = FMath::Min(PendingTime + FMath::Max(DeltaTime, 0.0f), ClampedStepTime * static_cast<float>(ClampedMaxSteps));

		const int32 NumSteps = FMath::FloorToInt(PendingTime / ClampedStepTime);
		if (NumSteps <= 0)
		{
			return 0;
		}

		// Keep the remaining fraction unclamped so the average over many frames stays exactly one step per StepTime.
		PendingTime = FMath::Min(PendingTime - static_cast<float>(NumSteps) * ClampedStepTime, ClampedStepTime);
		return NumSteps;
	}

	void Reset()
	{
		PendingTime = 0.0f;
	}
};
