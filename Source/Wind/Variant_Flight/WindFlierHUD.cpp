// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFlierHUD.h"

#include "Engine/Canvas.h"
#include "WindFlierPawn.h"
#include "WindGustComponent.h"

namespace WindFlierHUDPrivate
{
	/** Keep in sync with EWindGustPhase. */
	static const TCHAR* GetPhaseName(EWindGustPhase Phase)
	{
		switch (Phase)
		{
		case EWindGustPhase::Rising:  return TEXT("Rising");
		case EWindGustPhase::Holding: return TEXT("Holding");
		case EWindGustPhase::Falling: return TEXT("Falling");
		default:                      return TEXT("Idle");
		}
	}
}

AWindFlierHUD::AWindFlierHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWindFlierHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !bShowWindMeters)
	{
		return;
	}

	const AWindFlierPawn* Flier = Cast<AWindFlierPawn>(GetOwningPawn());
	const UWindGustComponent* Gust = Flier ? Flier->GetWindGust() : nullptr;
	if (!Gust)
	{
		return;
	}

	const float BarWidth = FMath::Max(MeterSize.X, 1.0f);
	const float BarHeight = FMath::Max(MeterSize.Y, 1.0f);
	const float BarLeft = (Canvas->ClipX - BarWidth) * 0.5f;
	const float ChargeTop = Canvas->ClipY - BottomMargin;
	const float WindTop = ChargeTop + BarHeight + MeterSpacing;

	const float ChargeAlpha = Gust->GetChargeAlpha();
	const float WindAlpha = Gust->GetWindStrengthAlpha();

	// Lower bar: the wind that is actually blowing.
	if (WindAlpha > 0.0f)
	{
		DrawRect(WindColor, BarLeft, WindTop, BarWidth * WindAlpha, BarHeight);
	}

	// Upper bar: what the player is asking for right now.
	if (ChargeAlpha > 0.0f)
	{
		DrawRect(ChargeColor, BarLeft, ChargeTop, BarWidth * ChargeAlpha, BarHeight);
	}

	if (bShowPhaseText)
	{
		const FString Text = FString::Printf(TEXT("%s    wind %.0f cm/s"),
			WindFlierHUDPrivate::GetPhaseName(Gust->GetPhase()), Gust->GetWindSpeed());

		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(Text, TextWidth, TextHeight);

		DrawText(Text, FLinearColor::White, (Canvas->ClipX - TextWidth) * 0.5f, ChargeTop - TextHeight - 6.0f);
	}
}
