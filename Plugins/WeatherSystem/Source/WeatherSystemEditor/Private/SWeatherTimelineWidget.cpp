// Copyright TADemo. All Rights Reserved.

#include "SWeatherTimelineWidget.h"

#include "Data/WeatherDataAsset.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "ScopedTransaction.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SWeatherTimelineWidget"

void SWeatherTimelineWidget::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	OnFrameSelected = InArgs._OnFrameSelected;
	OnFramesEdited  = InArgs._OnFramesEdited;
}

void SWeatherTimelineWidget::SetSelectedFrame(int32 FrameIndex)
{
	SelectedIndex = FrameIndex;
}

float SWeatherTimelineWidget::TimeToX(float T, float Width) const
{
	const float Usable = FMath::Max(1.0f, Width - LeftPadding - RightPadding);
	return LeftPadding + FMath::Clamp(T / 24.0f, 0.0f, 1.0f) * Usable;
}

float SWeatherTimelineWidget::XToTime(float X, float Width) const
{
	const float Usable = FMath::Max(1.0f, Width - LeftPadding - RightPadding);
	const float T = ((X - LeftPadding) / Usable) * 24.0f;
	return FMath::Clamp(T, 0.0f, 24.0f);
}

int32 SWeatherTimelineWidget::HitTestFrame(const FGeometry& Geo, FVector2D LocalPos) const
{
	UWeatherDataAsset* A = Asset.Get();
	if (!A) return INDEX_NONE;
	const float W = Geo.GetLocalSize().X;
	const float H = Geo.GetLocalSize().Y;
	const float TrackY = (TrackTopFrac + TrackBottomFrac) * 0.5f * H;
	const float Threshold = PipRadius + 3.0f;
	int32 BestIdx = INDEX_NONE;
	float BestDistSq = Threshold * Threshold;
	for (int32 i = 0; i < A->Frames.Num(); ++i)
	{
		const float PipX = TimeToX(A->Frames[i].TimeOfDay, W);
		const float dx = LocalPos.X - PipX;
		const float dy = LocalPos.Y - TrackY;
		const float DistSq = dx * dx + dy * dy;
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			BestIdx = i;
		}
	}
	return BestIdx;
}

int32 SWeatherTimelineWidget::OnPaint(const FPaintArgs& Args,
                                      const FGeometry& AllottedGeometry,
                                      const FSlateRect& MyCullingRect,
                                      FSlateWindowElementList& OutDrawElements,
                                      int32 LayerId,
                                      const FWidgetStyle& InWidgetStyle,
                                      bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float W = Size.X;
	const float H = Size.Y;

	// Background panel
	FSlateDrawElement::MakeBox(
		OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor(0.06f, 0.07f, 0.09f, 1.0f));

	// Baseline track
	const float TrackY0 = H * TrackTopFrac;
	const float TrackY1 = H * TrackBottomFrac;
	const float TrackYMid = 0.5f * (TrackY0 + TrackY1);
	TArray<FVector2D> Line;
	Line.Add(FVector2D(LeftPadding,        TrackYMid));
	Line.Add(FVector2D(W - RightPadding,   TrackYMid));
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(),
		Line, ESlateDrawEffect::None, FLinearColor(0.32f, 0.36f, 0.42f, 1.0f), true, 2.0f);

	const FSlateFontInfo TickFont = FCoreStyle::Get().GetFontStyle("SmallFont");
	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Hour ticks every 3h + label; minor ticks every 1h
	for (int32 Hour = 0; Hour <= 24; ++Hour)
	{
		const float X = TimeToX(static_cast<float>(Hour), W);
		const bool bMajor = (Hour % 3 == 0);
		const float TickLen = bMajor ? 12.0f : 5.0f;
		TArray<FVector2D> Tick;
		Tick.Add(FVector2D(X, TrackYMid - TickLen));
		Tick.Add(FVector2D(X, TrackYMid + TickLen));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(),
			Tick, ESlateDrawEffect::None,
			bMajor ? FLinearColor(0.70f, 0.75f, 0.82f, 1.0f) : FLinearColor(0.45f, 0.48f, 0.53f, 1.0f),
			true, bMajor ? 1.5f : 1.0f);

		if (bMajor)
		{
			const FString Label = FString::Printf(TEXT("%02d"), Hour);
			const FVector2D LabelSize = FontMeasure->Measure(Label, TickFont);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 3,
				AllottedGeometry.ToPaintGeometry(FVector2D(X - LabelSize.X * 0.5f, TrackYMid + TickLen + 3.0f), LabelSize),
				Label, TickFont, ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.87f, 0.90f, 1.0f));
		}
	}

	// Frame pips
	if (UWeatherDataAsset* A = Asset.Get())
	{
		for (int32 i = 0; i < A->Frames.Num(); ++i)
		{
			const float PipX = TimeToX(A->Frames[i].TimeOfDay, W);
			const bool bSelected = (i == SelectedIndex);
			const bool bDragging = (i == DraggingIndex);
			const FLinearColor FillColor = bSelected
				? FLinearColor(0.98f, 0.72f, 0.20f, 1.0f)    // amber for selected
				: FLinearColor(0.35f, 0.68f, 0.92f, 1.0f);    // blue otherwise
			const float R = PipRadius + (bDragging ? 2.0f : 0.0f);
			// Draw a filled circle-ish with a scaled box
			FSlateDrawElement::MakeBox(
				OutDrawElements, LayerId + 4,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(PipX - R, TrackYMid - R),
					FVector2D(R * 2.0f, R * 2.0f)),
				FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FillColor);
			// Time label above
			const FString T = FString::Printf(TEXT("%.2f"), A->Frames[i].TimeOfDay);
			const FVector2D LSize = FontMeasure->Measure(T, TickFont);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 5,
				AllottedGeometry.ToPaintGeometry(FVector2D(PipX - LSize.X * 0.5f, TrackYMid - R - LSize.Y - 2.0f), LSize),
				T, TickFont, ESlateDrawEffect::None,
				bSelected ? FLinearColor(1.0f, 0.85f, 0.55f, 1.0f) : FLinearColor(0.85f, 0.87f, 0.90f, 1.0f));
		}
	}

	return LayerId + 6;
}

FReply SWeatherTimelineWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const int32 Hit = HitTestFrame(MyGeometry, Local);

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		SelectedIndex = Hit;
		if (Hit != INDEX_NONE)
		{
			DraggingIndex = Hit;
			OnFrameSelected.ExecuteIfBound(Hit);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		OnFrameSelected.ExecuteIfBound(INDEX_NONE);
		return FReply::Handled();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Hit != INDEX_NONE)
	{
		if (UWeatherDataAsset* A = Asset.Get())
		{
			const FScopedTransaction Tx(LOCTEXT("DeleteFrame", "Delete Weather Frame"));
			A->Modify();
			A->Frames.RemoveAt(Hit);
			if (SelectedIndex == Hit) SelectedIndex = INDEX_NONE;
			else if (SelectedIndex > Hit) --SelectedIndex;
			NotifyEdited();
			OnFrameSelected.ExecuteIfBound(SelectedIndex);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SWeatherTimelineWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DraggingIndex != INDEX_NONE)
	{
		DraggingIndex = INDEX_NONE;
		NotifyEdited();  // trigger a resort + reselect by TOD via NotifyEdited handler in the toolkit
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SWeatherTimelineWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DraggingIndex != INDEX_NONE)
	{
		UWeatherDataAsset* A = Asset.Get();
		if (!A || !A->Frames.IsValidIndex(DraggingIndex)) return FReply::Handled();
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		const float NewT = XToTime(Local.X, MyGeometry.GetLocalSize().X);
		if (!FMath::IsNearlyEqual(A->Frames[DraggingIndex].TimeOfDay, NewT))
		{
			A->Modify();
			A->Frames[DraggingIndex].TimeOfDay = NewT;
			// Don't resort while dragging — resort on mouse-up so index stays stable
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SWeatherTimelineWidget::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	UWeatherDataAsset* A = Asset.Get();
	if (!A) return FReply::Unhandled();

	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (HitTestFrame(MyGeometry, Local) != INDEX_NONE) return FReply::Unhandled();

	const FScopedTransaction Tx(LOCTEXT("AddFrame", "Add Weather Frame"));
	A->Modify();
	FWeatherFrame NewFrame;
	NewFrame.TimeOfDay = XToTime(Local.X, MyGeometry.GetLocalSize().X);
	const int32 NewIdx = A->Frames.Add(NewFrame);
	SelectedIndex = NewIdx;
	NotifyEdited();
	OnFrameSelected.ExecuteIfBound(SelectedIndex);
	return FReply::Handled();
}

void SWeatherTimelineWidget::NotifyEdited()
{
	if (UWeatherDataAsset* A = Asset.Get())
	{
		// Sort ascending by TOD and re-track selection index by identity.
		int32 SelId = SelectedIndex;
		FWeatherFrame KeepFrame;
		bool bHaveKeep = false;
		if (A->Frames.IsValidIndex(SelId))
		{
			KeepFrame = A->Frames[SelId];
			bHaveKeep = true;
		}
		A->Frames.Sort([](const FWeatherFrame& X, const FWeatherFrame& Y) { return X.TimeOfDay < Y.TimeOfDay; });
		if (bHaveKeep)
		{
			for (int32 i = 0; i < A->Frames.Num(); ++i)
			{
				if (FMath::IsNearlyEqual(A->Frames[i].TimeOfDay, KeepFrame.TimeOfDay) &&
				    A->Frames[i].Overrides.Num() == KeepFrame.Overrides.Num())
				{
					SelectedIndex = i;
					break;
				}
			}
		}
		A->ResolveFrames();
		A->MarkPackageDirty();
	}
	OnFramesEdited.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
