// Copyright TADemo. All Rights Reserved.

#include "WeatherFrame.h"

#include "Data/WeatherParamSchema.h"

namespace
{
	void ResolveAgainstBindings(const TArray<FWeatherParamBinding>& Bindings,
	                            const TMap<FName, FWeatherParamValue>& Overrides,
	                            TArray<FWeatherParamValue>& Out)
	{
		const int32 N = Bindings.Num();
		Out.Reset();
		Out.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			const FWeatherParamBinding& B = Bindings[i];
			if (const FWeatherParamValue* V = Overrides.Find(B.ParamName))
			{
				Out[i] = *V;
				Out[i].Type = B.Type; // authoritative from the binding
			}
			else
			{
				Out[i] = B.MakeDefaultValue();
			}
		}
	}
}

void FWeatherFrame::Resolve(const UWeatherParamSchema* Schema)
{
	if (!Schema) { ResolvedValues.Reset(); return; }
	ResolveAgainstBindings(Schema->Bindings, Overrides, ResolvedValues);
}

void FSeasonFrame::Resolve(const TArray<FWeatherParamBinding>& Bindings)
{
	ResolveAgainstBindings(Bindings, Overrides, ResolvedValues);
}

void FAddonFrame::Resolve(const UWeatherParamSchema* Schema)
{
	if (!Schema) { ResolvedValues.Reset(); return; }
	ResolveAgainstBindings(Schema->Bindings, Overrides, ResolvedValues);
}
