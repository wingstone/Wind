// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Fluid2DSimulationStep.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The fluid simulation advances in fixed steps of TimeStep driven by elapsed real time instead of the frame
 * rate. These tests pin that contract down: skipping at a high frame rate, catching up at a low one, and
 * never letting a hitch snowball.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluid2DStepAccumulatorFrameRateTest,
	"Wind.Fluid2D.StepAccumulator.FrameRateIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFluid2DStepAccumulatorFrameRateTest::RunTest(const FString& Parameters)
{
	const float StepTime = 1.0f / 60.0f;

	// 60 fps: exactly one fixed step per frame, no leftover.
	{
		FFluid2DFixedStepAccumulator Accumulator;
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			const int32 NumSteps = Accumulator.Accumulate(StepTime, StepTime, 4);
			if (!TestEqual(FString::Printf(TEXT("60 fps frame %d must simulate one step"), Frame), NumSteps, 1))
			{
				return false;
			}
		}
		TestEqual(TEXT("60 fps must not leave a backlog"), Accumulator.PendingTime, 0.0f);
	}

	// 240 fps: most frames are skipped, and the total still matches real time over a second.
	{
		FFluid2DFixedStepAccumulator Accumulator;
		const float FrameDelta = 1.0f / 240.0f;
		int32 TotalSteps = 0;
		int32 SkippedFrames = 0;
		for (int32 Frame = 0; Frame < 240; ++Frame)
		{
			const int32 NumSteps = Accumulator.Accumulate(FrameDelta, StepTime, 4);
			TotalSteps += NumSteps;
			SkippedFrames += NumSteps == 0 ? 1 : 0;
		}

		TestTrue(FString::Printf(TEXT("High frame rate must skip frames (skipped %d of 240)"), SkippedFrames), SkippedFrames > 0);
		TestTrue(FString::Printf(TEXT("A second at 240 fps must simulate about 60 steps, got %d"), TotalSteps),
			FMath::Abs(TotalSteps - 60) <= 1);
	}

	// 10 fps: every frame simulates several steps so the simulation keeps up with real time.
	{
		FFluid2DFixedStepAccumulator Accumulator;
		const float FrameDelta = 1.0f / 10.0f;
		const int32 MaxSteps = 8;
		int32 TotalSteps = 0;
		for (int32 Frame = 0; Frame < 10; ++Frame)
		{
			const int32 NumSteps = Accumulator.Accumulate(FrameDelta, StepTime, MaxSteps);
			TestTrue(FString::Printf(TEXT("10 fps frame %d must catch up with several steps, got %d"), Frame, NumSteps), NumSteps > 1);
			TestTrue(TEXT("A frame never exceeds MaxStepsPerUpdate"), NumSteps <= MaxSteps);
			TotalSteps += NumSteps;
		}
		TestTrue(FString::Printf(TEXT("A second at 10 fps must simulate about 60 steps, got %d"), TotalSteps),
			FMath::Abs(TotalSteps - 60) <= 1);
	}

	// The leftover always stays below one step, so the schedule never drifts.
	{
		FFluid2DFixedStepAccumulator Accumulator;
		for (int32 Frame = 0; Frame < 100; ++Frame)
		{
			Accumulator.Accumulate(1.0f / 37.0f, StepTime, 8);
			if (Accumulator.PendingTime >= StepTime)
			{
				AddError(FString::Printf(TEXT("Frame %d left a full step pending (%f >= %f)"), Frame, Accumulator.PendingTime, StepTime));
				return false;
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluid2DStepAccumulatorHitchTest,
	"Wind.Fluid2D.StepAccumulator.HitchIsClamped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFluid2DStepAccumulatorHitchTest::RunTest(const FString& Parameters)
{
	const float StepTime = 1.0f / 60.0f;
	const int32 MaxSteps = 4;

	FFluid2DFixedStepAccumulator Accumulator;

	// A one second hitch must not queue a full second worth of catch-up steps.
	const int32 HitchSteps = Accumulator.Accumulate(1.0f, StepTime, MaxSteps);
	TestEqual(TEXT("A long hitch is clamped to MaxStepsPerUpdate"), HitchSteps, MaxSteps);
	TestTrue(TEXT("The backlog is dropped instead of kept"), Accumulator.PendingTime < StepTime);

	// The frame right after the hitch resumes the normal schedule.
	const int32 NextSteps = Accumulator.Accumulate(StepTime, StepTime, MaxSteps);
	TestEqual(TEXT("The frame after a hitch runs one normal step"), NextSteps, 1);

	// A zero delta keeps the pending fraction and simulates nothing.
	FFluid2DFixedStepAccumulator IdleAccumulator;
	IdleAccumulator.PendingTime = StepTime * 0.5f;
	TestEqual(TEXT("A zero delta simulates no step"), IdleAccumulator.Accumulate(0.0f, StepTime, MaxSteps), 0);
	TestEqual(TEXT("A zero delta keeps the pending fraction"), IdleAccumulator.PendingTime, StepTime * 0.5f);

	// Negative deltas are treated as zero rather than rewinding the clock.
	TestEqual(TEXT("A negative delta simulates no step"), IdleAccumulator.Accumulate(-1.0f, StepTime, MaxSteps), 0);
	TestEqual(TEXT("A negative delta keeps the pending fraction"), IdleAccumulator.PendingTime, StepTime * 0.5f);

	IdleAccumulator.Reset();
	TestEqual(TEXT("Reset clears the pending fraction"), IdleAccumulator.PendingTime, 0.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
