// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UWeatherDataAsset;

DECLARE_DELEGATE_OneParam(FOnWeatherFrameSelected, int32 /*FrameIndex*/);
DECLARE_DELEGATE(FOnWeatherFramesEdited);

/**
 * Custom 0..24 timeline that shows every FWeatherFrame in a UWeatherDataAsset as a
 * draggable pip. Left-click selects; drag moves TimeOfDay; double-click on empty
 * space inserts a frame at that time; right-click on a pip deletes it.
 *
 * This widget is deliberately minimal — the real per-frame editing happens in the
 * details panel that the toolkit hooks up alongside.
 */
class SWeatherTimelineWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWeatherTimelineWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWeatherDataAsset>, Asset)
		SLATE_EVENT(FOnWeatherFrameSelected, OnFrameSelected)
		SLATE_EVENT(FOnWeatherFramesEdited, OnFramesEdited)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Programmatically clear or move the selection cursor. */
	void SetSelectedFrame(int32 FrameIndex);

	//~ SWidget
	virtual int32 OnPaint(const FPaintArgs& Args,
	                      const FGeometry& AllottedGeometry,
	                      const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements,
	                      int32 LayerId,
	                      const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(600.0f, 100.0f); }

private:
	TWeakObjectPtr<UWeatherDataAsset> Asset;
	FOnWeatherFrameSelected OnFrameSelected;
	FOnWeatherFramesEdited OnFramesEdited;

	int32 SelectedIndex = INDEX_NONE;
	int32 DraggingIndex = INDEX_NONE;

	/** Layout metrics (fractions of widget height). */
	static constexpr float TrackTopFrac    = 0.35f;
	static constexpr float TrackBottomFrac = 0.75f;
	static constexpr float PipRadius       = 7.0f;
	static constexpr float LeftPadding     = 24.0f;
	static constexpr float RightPadding    = 24.0f;

	/** Map a TOD in [0,24] to widget X. */
	float TimeToX(float T, float Width) const;
	/** Map a widget X to a TOD in [0,24]. */
	float XToTime(float X, float Width) const;

	/** Find the frame whose pip contains LocalPos; returns INDEX_NONE otherwise. */
	int32 HitTestFrame(const FGeometry& Geo, FVector2D LocalPos) const;

	void NotifyEdited();
};
