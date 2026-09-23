// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSDisplayActor.h"

#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SViewport.h"

AFPSDisplayActor::AFPSDisplayActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// 即便游戏暂停也允许 Tick，以便暂停时也能显示当前帧率
	PrimaryActorTick.bTickEvenWhenPaused = true;

	// 该 Actor 只用于绘制 UI，不需要任何网络复制
	bReplicates = false;
	SetCanBeDamaged(false);
}

void AFPSDisplayActor::BeginPlay()
{
	Super::BeginPlay();

	SmoothedDeltaTime = FApp::GetDeltaTime();
	TimeSinceLastUpdate = 0.f;

	if (bShowFPS)
	{
		AddWidgetToViewport();
	}
}

void AFPSDisplayActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveWidgetFromViewport();
	Super::EndPlay(EndPlayReason);
}

void AFPSDisplayActor::SetVisible(bool bVisible)
{
	bShowFPS = bVisible;
	if (bVisible)
	{
		AddWidgetToViewport();
	}
	else
	{
		RemoveWidgetFromViewport();
	}
}

void AFPSDisplayActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bShowFPS || !FPSTextWidget.IsValid())
	{
		return;
	}

	// 使用真实（未受 time dilation 影响）的 DeltaTime，避免 Slomo 时数值失真
	const float RealDelta = FApp::GetDeltaTime();

	// 指数移动平均平滑
	constexpr float SmoothingAlpha = 0.1f;
	SmoothedDeltaTime = FMath::Lerp(SmoothedDeltaTime, RealDelta, SmoothingAlpha);

	TimeSinceLastUpdate += RealDelta;
	const float UpdateInterval = 1.f / FMath::Max(UpdateFrequency, 1.f);
	if (TimeSinceLastUpdate >= UpdateInterval)
	{
		TimeSinceLastUpdate = 0.f;
		FPSTextWidget->SetText(FText::FromString(BuildDisplayString(SmoothedDeltaTime)));
	}
}

FString AFPSDisplayActor::BuildDisplayString(float SmoothedDT) const
{
	const float SafeDT = FMath::Max(SmoothedDT, KINDA_SMALL_NUMBER);
	const float FPS = 1.f / SafeDT;
	const float MS = SafeDT * 1000.f;
	return FString::Printf(TEXT("FPS: %.1f  (%.2f ms)"), FPS, MS);
}

void AFPSDisplayActor::AddWidgetToViewport()
{
	if (FPSTextWidget.IsValid())
	{
		return;
	}

	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!ViewportClient)
	{
		return;
	}

	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", FontSize);

	FPSTextWidget = SNew(STextBlock)
		.Font(FontInfo)
		.ColorAndOpacity(FSlateColor(TextColor))
		.ShadowOffset(bDrawShadow ? FVector2D(1.f, 1.f) : FVector2D::ZeroVector)
		.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
		.Text(FText::FromString(TEXT("FPS: --")));

	ViewportClient->AddViewportWidgetContent(
		SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(ScreenOffset.X, ScreenOffset.Y, 0.f, 0.f))
		[
			FPSTextWidget.ToSharedRef()
		],
		/*ZOrder=*/ 1000);
}

void AFPSDisplayActor::RemoveWidgetFromViewport()
{
	if (!FPSTextWidget.IsValid())
	{
		return;
	}

	if (UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		ViewportClient->RemoveViewportWidgetContent(FPSTextWidget.ToSharedRef());
	}
	FPSTextWidget.Reset();
}
