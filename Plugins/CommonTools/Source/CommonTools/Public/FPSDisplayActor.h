// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSDisplayActor.generated.h"

class STextBlock;

/**
 * 在屏幕上显示实时帧率的 Actor。
 * 使用 Slate 直接添加到 GameViewport，因此在 Shipping 构建下同样有效
 * （不依赖 stat fps / AddOnScreenDebugMessage，这些接口在 Shipping 中会被裁掉）。
 *
 * 使用方式：
 *   1) 将本 Actor 放置到关卡中，或用任意方式在运行时 Spawn。
 *   2) 调整属性即可自定义显示位置、字号、颜色、更新频率。
 */
UCLASS(Blueprintable, ClassGroup = (Debug), meta = (DisplayName = "FPS Display Actor"))
class COMMONTOOLS_API AFPSDisplayActor : public AActor
{
	GENERATED_BODY()

public:
	AFPSDisplayActor();

	/** 是否显示 FPS 文本 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS Display")
	bool bShowFPS = true;

	/** 文本更新频率（Hz），过高会引起数字抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS Display", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float UpdateFrequency = 5.f;

	/** 字体大小 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS Display", meta = (ClampMin = "6", ClampMax = "128"))
	int32 FontSize = 18;

	/** 相对于视口左上角的像素偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS Display")
	FVector2D ScreenOffset = FVector2D(20.f, 20.f);

	/** 文字颜色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS Display")
	FLinearColor TextColor = FLinearColor(0.1f, 1.f, 0.1f, 1.f);

	/** 是否显示背景描边（提高在亮色背景下的可读性） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS Display")
	bool bDrawShadow = true;

	UFUNCTION(BlueprintCallable, Category = "FPS Display")
	void SetVisible(bool bVisible);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** 使用指数移动平均平滑 DeltaTime，避免帧率数值抖动 */
	float SmoothedDeltaTime = 0.f;

	/** 距离上次刷新文本经过的时间 */
	float TimeSinceLastUpdate = 0.f;

	/** Slate 文本控件 */
	TSharedPtr<STextBlock> FPSTextWidget;

	/** 将控件加入 Viewport，若已加入则忽略 */
	void AddWidgetToViewport();

	/** 从 Viewport 移除控件 */
	void RemoveWidgetFromViewport();

	/** 生成 "FPS: 60.0 (16.67 ms)" 之类的显示字符串 */
	FString BuildDisplayString(float SmoothedDT) const;
};
