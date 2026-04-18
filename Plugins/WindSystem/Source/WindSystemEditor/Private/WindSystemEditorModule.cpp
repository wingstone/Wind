// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "WindFieldSourceVisualizer.h"
#include "WindFieldDirectionalComponent.h"
#include "WindFieldPointComponent.h"
#include "WindFieldVortexComponent.h"
#include "WindFieldCylinderComponent.h"

#define LOCTEXT_NAMESPACE "FWindSystemEditorModule"

class FWindSystemEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FWindFieldSourceVisualizer> Visualizer;
};

void FWindSystemEditorModule::StartupModule()
{
	if (GUnrealEd)
	{
		Visualizer = MakeShared<FWindFieldSourceVisualizer>();

		GUnrealEd->RegisterComponentVisualizer(UWindFieldDirectionalComponent::StaticClass()->GetFName(), Visualizer);
		GUnrealEd->RegisterComponentVisualizer(UWindFieldPointComponent::StaticClass()->GetFName(), Visualizer);
		GUnrealEd->RegisterComponentVisualizer(UWindFieldVortexComponent::StaticClass()->GetFName(), Visualizer);
		GUnrealEd->RegisterComponentVisualizer(UWindFieldCylinderComponent::StaticClass()->GetFName(), Visualizer);
	}
}

void FWindSystemEditorModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UWindFieldDirectionalComponent::StaticClass()->GetFName());
		GUnrealEd->UnregisterComponentVisualizer(UWindFieldPointComponent::StaticClass()->GetFName());
		GUnrealEd->UnregisterComponentVisualizer(UWindFieldVortexComponent::StaticClass()->GetFName());
		GUnrealEd->UnregisterComponentVisualizer(UWindFieldCylinderComponent::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWindSystemEditorModule, WindSystemEditor)
