// Copyright TADemo. All Rights Reserved.

#include "Data/WeatherDataAsset.h"

#include "Data/WeatherParamSchema.h"

void UWeatherDataAsset::PostLoad()
{
	Super::PostLoad();
	NormalizeFrames();
}

#if WITH_EDITOR
void UWeatherDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NormalizeFrames();
}
#endif

void UWeatherDataAsset::NormalizeFrames()
{
	Frames.Sort([](const FWeatherFrame& A, const FWeatherFrame& B) { return A.TimeOfDay < B.TimeOfDay; });
	for (FWeatherFrame& F : Frames)
	{
		F.Resolve(Schema);
	}
}

void UWeatherDataAsset::ResolveFrames()
{
	for (FWeatherFrame& F : Frames)
	{
		F.Resolve(Schema);
	}
}

void UWeatherDataAsset::BuildConstantValues(TArray<FWeatherParamValue>& Out) const
{
	Out.Reset();
	if (!Schema) return;
	const int32 N = Schema->Bindings.Num();
	Out.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const FWeatherParamBinding& B = Schema->Bindings[i];
		if (const FWeatherParamValue* V = Constants.Find(B.ParamName))
		{
			Out[i] = *V;
			Out[i].Type = B.Type;
		}
		else
		{
			Out[i] = B.MakeDefaultValue();
		}
	}
}
