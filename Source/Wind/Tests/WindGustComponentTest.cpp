// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "WindGustComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindGustComponentDefaultsTest,
	"Wind.Flight.GustComponent.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWindGustComponentDefaultsTest::RunTest(const FString& Parameters)
{
	UWindGustComponent* Gust = NewObject<UWindGustComponent>();

	TestFalse(TEXT("A fresh component is idle"), Gust->IsBlowing());
	TestFalse(TEXT("A fresh component is not charging"), Gust->IsCharging());
	TestEqual(TEXT("Charge starts empty"), Gust->GetChargeAlpha(), 0.0f);
	TestEqual(TEXT("Wind starts still"), Gust->GetWindSpeed(), 0.0f);
	TestEqual(TEXT("Wind velocity is zero while still"), Gust->GetWindVelocity(), FVector::ZeroVector);
	TestEqual(TEXT("Strength is zero while the wind is still"), Gust->GetWindStrengthAlpha(), 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindGustComponentChargeIntentTest,
	"Wind.Flight.GustComponent.ChargeIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWindGustComponentChargeIntentTest::RunTest(const FString& Parameters)
{
	UWindGustComponent* Gust = NewObject<UWindGustComponent>();

	Gust->StartCharging();
	TestTrue(TEXT("StartCharging raises player intent"), Gust->IsCharging());

	Gust->StopCharging();
	TestFalse(TEXT("StopCharging clears player intent"), Gust->IsCharging());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindGustComponentStrengthGuardTest,
	"Wind.Flight.GustComponent.StrengthGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWindGustComponentStrengthGuardTest::RunTest(const FString& Parameters)
{
	UWindGustComponent* Gust = NewObject<UWindGustComponent>();

	// GetWindStrengthAlpha divides by MaxWindSpeed, so a zero maximum must hit the guard
	// and report zero strength rather than producing a NaN.
	Gust->MaxWindSpeed = 0.0f;
	TestEqual(TEXT("Zero MaxWindSpeed yields zero strength instead of a NaN"), Gust->GetWindStrengthAlpha(), 0.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
