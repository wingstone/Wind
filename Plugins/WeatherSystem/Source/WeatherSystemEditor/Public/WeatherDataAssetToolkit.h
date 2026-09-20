// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UWeatherDataAsset;

/**
 * Standalone editor for UWeatherDataAsset. Two docked tabs: a horizontal 0..24 timeline
 * of frames on the left, and a standard details panel on the right that reflects the
 * currently selected frame (or the asset when nothing is selected).
 */
class FWeatherDataAssetToolkit : public FAssetEditorToolkit
{
public:
	void InitEditor(const TArray<UObject*>& InObjects);

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	virtual FName GetToolkitFName() const override { return TEXT("WeatherDataAssetEditor"); }
	virtual FText GetBaseToolkitName() const override { return INVTEXT("Weather Data Asset Editor"); }
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("Weather "); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.2f, 0.5f, 0.8f, 1.0f); }

	/** Widget calls into this when a frame is picked so the details panel can retarget. */
	void OnFrameSelected(int32 FrameIndex);

private:
	TWeakObjectPtr<UWeatherDataAsset> Asset;
	TSharedPtr<class IDetailsView> DetailsView;
	TSharedPtr<class SWeatherTimelineWidget> Timeline;

	int32 SelectedFrameIndex = INDEX_NONE;

	void RefreshDetailsSelection();

	static const FName TimelineTabId;
	static const FName DetailsTabId;
};
