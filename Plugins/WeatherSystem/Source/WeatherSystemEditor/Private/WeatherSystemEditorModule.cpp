// Copyright TADemo. All Rights Reserved.

#include "WeatherSystemEditorModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "WeatherDataAssetActions.h"
#include "WeatherFrame.h"
#include "WeatherFrameCustomization.h"
#include "WeatherParamSchemaCustomization.h"

#include "Data/WeatherParamSchema.h"

#define LOCTEXT_NAMESPACE "FWeatherSystemEditorModule"

void FWeatherSystemEditorModule::StartupModule()
{
	WeatherDataAssetActions = MakeShared<FWeatherDataAssetActions>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(WeatherDataAssetActions.ToSharedRef());

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FWeatherFrame::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FWeatherFrameCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(
		UWeatherParamSchema::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FWeatherParamSchemaCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FWeatherSystemEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")) && WeatherDataAssetActions.IsValid())
	{
		FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(WeatherDataAssetActions.ToSharedRef());
	}
	WeatherDataAssetActions.Reset();

	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(FWeatherFrame::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UWeatherParamSchema::StaticClass()->GetFName());
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWeatherSystemEditorModule, WeatherSystemEditor)
