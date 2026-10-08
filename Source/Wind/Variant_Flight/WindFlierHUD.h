// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WindFlierHUD.generated.h"

/**
 * Canvas HUD for the wind flier: two meters side by side, one for the charge the
 * player is holding and one for the wind that is actually blowing.
 *
 * Watching the second meter trail the first is what makes the wind-up / hold /
 * die-down behaviour readable, so it doubles as a tuning aid.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Wind Flier HUD"))
class AWindFlierHUD : public AHUD
{
	GENERATED_BODY()

public:

	AWindFlierHUD();

	virtual void DrawHUD() override;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|HUD")
	bool bShowWindMeters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|HUD")
	bool bShowPhaseText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|HUD")
	FVector2D MeterSize = FVector2D(360.0f, 16.0f);

	/** Distance from the bottom of the screen to the charge meter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|HUD")
	float BottomMargin = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|HUD")
	float MeterSpacing = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|HUD")
	FLinearColor ChargeColor = FLinearColor(0.35f, 0.75f, 1.0f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|HUD")
	FLinearColor WindColor = FLinearColor(1.0f, 0.85f, 0.3f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|HUD")
	FLinearColor MeterBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.4f);
};
