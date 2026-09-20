// Copyright TADemo. All Rights Reserved.

#include "WeatherDataAssetToolkit.h"

#include "Data/WeatherDataAsset.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SWeatherTimelineWidget.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "WeatherDataAssetToolkit"

const FName FWeatherDataAssetToolkit::TimelineTabId(TEXT("WeatherDataAssetEditor_Timeline"));
const FName FWeatherDataAssetToolkit::DetailsTabId (TEXT("WeatherDataAssetEditor_Details"));

void FWeatherDataAssetToolkit::InitEditor(const TArray<UObject*>& InObjects)
{
	Asset = InObjects.Num() > 0 ? Cast<UWeatherDataAsset>(InObjects[0]) : nullptr;

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("WeatherDataAssetEditor_Layout_v1")
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.25f)
			->AddTab(TimelineTabId, ETabState::OpenedTab)
			->SetHideTabWell(true)
		)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.75f)
			->AddTab(DetailsTabId, ETabState::OpenedTab)
			->SetHideTabWell(true)
		)
	);

	FAssetEditorToolkit::InitAssetEditor(EToolkitMode::Standalone, {}, TEXT("WeatherDataAssetEditor"),
		Layout, /*bCreateDefaultStandaloneMenu=*/true, /*bCreateDefaultToolbar=*/true, InObjects);
}

void FWeatherDataAssetToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(INVTEXT("Weather Data Asset"));

	InTabManager->RegisterTabSpawner(TimelineTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs&)
	{
		return SNew(SDockTab)
			.Label(INVTEXT("Timeline"))
			[
				SAssignNew(Timeline, SWeatherTimelineWidget)
				.Asset(Asset)
				.OnFrameSelected(FOnWeatherFrameSelected::CreateSP(this, &FWeatherDataAssetToolkit::OnFrameSelected))
				.OnFramesEdited(FOnWeatherFramesEdited::CreateLambda([this]()
				{
					if (DetailsView.IsValid())
					{
						DetailsView->ForceRefresh();
					}
				}))
			];
	}))
	.SetDisplayName(INVTEXT("Timeline"))
	.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bShowScrollBar   = true;
	DetailsView = PropertyEditorModule.CreateDetailView(Args);
	DetailsView->SetObjects(TArray<UObject*>{ Asset.Get() });

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs&)
	{
		return SNew(SDockTab)
			.Label(INVTEXT("Details"))
			[
				DetailsView.ToSharedRef()
			];
	}))
	.SetDisplayName(INVTEXT("Details"))
	.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FWeatherDataAssetToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(TimelineTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

void FWeatherDataAssetToolkit::OnFrameSelected(int32 FrameIndex)
{
	SelectedFrameIndex = FrameIndex;
	if (Timeline.IsValid()) Timeline->SetSelectedFrame(FrameIndex);
	RefreshDetailsSelection();
}

void FWeatherDataAssetToolkit::RefreshDetailsSelection()
{
	if (!DetailsView.IsValid()) return;
	// Simple strategy: the details view keeps showing the whole UWeatherDataAsset. It
	// already lets you expand Frames[]; selecting a pip is a visual affordance today.
	// A future revision can swap DetailsView.SetObjects to a proxy UObject that mirrors
	// just the selected FWeatherFrame if scrolling to the right entry becomes painful.
	DetailsView->ForceRefresh();
}

#undef LOCTEXT_NAMESPACE
