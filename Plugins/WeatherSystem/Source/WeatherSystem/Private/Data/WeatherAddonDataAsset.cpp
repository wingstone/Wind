// Copyright TADemo. All Rights Reserved.

#include "Data/WeatherAddonDataAsset.h"

#include "Data/WeatherParamSchema.h"

void UWeatherAddonDataAsset::PostLoad()
{
	Super::PostLoad();
	NormalizeFrames();
}

#if WITH_EDITOR
void UWeatherAddonDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NormalizeFrames();
}
#endif

void UWeatherAddonDataAsset::NormalizeFrames()
{
	Frames.Sort([](const FAddonFrame& A, const FAddonFrame& B) { return A.TimeOfDay < B.TimeOfDay; });
	for (FAddonFrame& F : Frames)
	{
		F.Resolve(Schema);
	}
}

void UWeatherAddonDataAsset::ResolveFrames()
{
	for (FAddonFrame& F : Frames)
	{
		F.Resolve(Schema);
	}
}

void UWeatherAddonDataAsset::BuildConstantValues(TArray<FWeatherParamValue>& Out) const
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
