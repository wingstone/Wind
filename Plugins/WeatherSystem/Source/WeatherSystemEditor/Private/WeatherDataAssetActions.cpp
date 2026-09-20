// Copyright TADemo. All Rights Reserved.

#include "WeatherDataAssetActions.h"

#include "Data/WeatherDataAsset.h"
#include "WeatherDataAssetToolkit.h"

UClass* FWeatherDataAssetActions::GetSupportedClass() const
{
	return UWeatherDataAsset::StaticClass();
}

FText FWeatherDataAssetActions::GetName() const
{
	return INVTEXT("Weather Data Asset");
}

FColor FWeatherDataAssetActions::GetTypeColor() const
{
	return FColor(90, 160, 220);
}

uint32 FWeatherDataAssetActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FWeatherDataAssetActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	MakeShared<FWeatherDataAssetToolkit>()->InitEditor(InObjects);
}
