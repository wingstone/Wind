// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeatherSystemTypes.h"
#include "WeatherFrame.generated.h"

class UWeatherParamSchema;

/**
 * A snapshot of parameter values at a specific TimeOfDay (0..24).
 *
 * Authored data lives in Overrides (a name → value map). Not every schema channel
 * needs an entry: missing channels are filled from the binding's default at resolve
 * time. Resolve happens in PostLoad and on edits; the runtime path reads ResolvedValues,
 * which is a compact array parallel to the schema's Bindings.
 */
USTRUCT(BlueprintType)
struct WEATHERSYSTEM_API FWeatherFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float TimeOfDay = 0.0f;

	/** Sparse overrides keyed by binding ParamName. Absent entries use the binding default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TMap<FName, FWeatherParamValue> Overrides;

	/** Runtime-only, rebuilt from Overrides + schema defaults. One entry per schema binding. */
	UPROPERTY(Transient)
	TArray<FWeatherParamValue> ResolvedValues;

	/** Expand Overrides against a schema into ResolvedValues, filling defaults and clamping. */
	void Resolve(const UWeatherParamSchema* Schema);
};

USTRUCT(BlueprintType)
struct WEATHERSYSTEM_API FSeasonFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SeasonPhase = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TMap<FName, FWeatherParamValue> Overrides;

	UPROPERTY(Transient)
	TArray<FWeatherParamValue> ResolvedValues;

	/** Season assets carry an inline bindings array (no schema asset), so Resolve takes it directly. */
	void Resolve(const TArray<FWeatherParamBinding>& Bindings);
};

USTRUCT(BlueprintType)
struct WEATHERSYSTEM_API FAddonFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float TimeOfDay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Weight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TMap<FName, FWeatherParamValue> Overrides;

	UPROPERTY(Transient)
	TArray<FWeatherParamValue> ResolvedValues;

	void Resolve(const UWeatherParamSchema* Schema);
};

/**
 * Sampling helpers over a sorted-by-time frame array.
 *
 * Frames must be sorted ascending by the time member. If Frames is empty, samplers
 * return a default-constructed FrameT.
 */
namespace WeatherFrameSampling
{
	/** Blend two Values arrays elementwise into Out (which will be resized). */
	inline void BlendValueArrays(const TArray<FWeatherParamValue>& A, const TArray<FWeatherParamValue>& B,
	                             float Alpha, TArray<FWeatherParamValue>& Out)
	{
		const int32 N = FMath::Min(A.Num(), B.Num());
		Out.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			Out[i] = FWeatherParamValue::Lerp(A[i], B[i], Alpha);
		}
	}

	/** Binary-search the pair of frames straddling Time; returns lo index and alpha in [0,1]. */
	template <typename FrameT>
	void FindFrames(const TArray<FrameT>& Frames, float Time, float (FrameT::*TimeMember),
	                int32& OutLo, int32& OutHi, float& OutAlpha)
	{
		OutLo = OutHi = 0;
		OutAlpha = 0.0f;
		const int32 N = Frames.Num();
		if (N == 0) return;
		if (N == 1) { OutLo = OutHi = 0; return; }

		if (Time <= Frames[0].*TimeMember) { OutLo = OutHi = 0; return; }
		if (Time >= Frames[N - 1].*TimeMember) { OutLo = OutHi = N - 1; return; }

		int32 Lo = 0, Hi = N - 1;
		while (Lo + 1 < Hi)
		{
			const int32 Mid = (Lo + Hi) >> 1;
			if (Frames[Mid].*TimeMember <= Time) Lo = Mid; else Hi = Mid;
		}
		OutLo = Lo;
		OutHi = Lo + 1;
		const float T0 = Frames[Lo].*TimeMember;
		const float T1 = Frames[Lo + 1].*TimeMember;
		OutAlpha = (T1 > T0) ? ((Time - T0) / (T1 - T0)) : 0.0f;
	}
}
