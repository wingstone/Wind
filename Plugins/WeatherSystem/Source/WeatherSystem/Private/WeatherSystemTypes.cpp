// Copyright TADemo. All Rights Reserved.

#include "WeatherSystemTypes.h"

FWeatherParamValue FWeatherParamValue::Lerp(const FWeatherParamValue& A, const FWeatherParamValue& B, float T)
{
	FWeatherParamValue R;
	R.Type = A.Type;
	switch (A.Type)
	{
	case EWeatherParamType::Float:
		R.FloatValue = FMath::Lerp(A.FloatValue, B.FloatValue, T);
		break;
	case EWeatherParamType::LinearColor:
		R.LinearColorValue = FMath::Lerp(A.LinearColorValue, B.LinearColorValue, T);
		break;
	case EWeatherParamType::Color:
	{
		// Interpolate in linear space then re-quantize, avoids the ugly sRGB midpoint.
		const FLinearColor La = FLinearColor(A.ColorValue);
		const FLinearColor Lb = FLinearColor(B.ColorValue);
		R.ColorValue = FMath::Lerp(La, Lb, T).ToFColor(true);
		break;
	}
	}
	return R;
}
