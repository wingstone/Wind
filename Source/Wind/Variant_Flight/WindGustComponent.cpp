// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindGustComponent.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Wind.h"

UWindGustComponent::UWindGustComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWindGustComponent::SetCharging(bool bInCharging)
{
	if (bCharging == bInCharging)
	{
		return;
	}

	bCharging = bInCharging;

	if (bLogWind)
	{
		UE_LOG(LogWind, Log, TEXT("Wind gust %s: %s"), *GetNameSafe(GetOwner()), bCharging ? TEXT("charging") : TEXT("released"));
	}
}

float UWindGustComponent::GetWindStrengthAlpha() const
{
	return MaxWindSpeed > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(WindSpeed / MaxWindSpeed, 0.0f, 1.0f) : 0.0f;
}

void UWindGustComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	UpdateCharge(DeltaTime);
	UpdateEnvelope(DeltaTime);

	const float IdleSpeed = MaxWindSpeed * IdleThreshold;

	EWindGustPhase NewPhase;
	if (WindSpeed <= IdleSpeed)
	{
		NewPhase = EWindGustPhase::Idle;
	}
	else if (!bCharging)
	{
		NewPhase = EWindGustPhase::Falling;
	}
	else if (WindSpeed < FMath::Max(TargetWindSpeed, IdleSpeed) * 0.98f)
	{
		NewPhase = EWindGustPhase::Rising;
	}
	else
	{
		NewPhase = EWindGustPhase::Holding;
	}

	if (NewPhase != Phase)
	{
		Phase = NewPhase;
		OnPhaseChanged.Broadcast(Phase);
	}

	if (FMath::Abs(ChargeAlpha - LastBroadcastCharge) >= 0.01f)
	{
		LastBroadcastCharge = ChargeAlpha;
		OnChargeChanged.Broadcast(ChargeAlpha);
	}

	if (bAffectPhysicsBodies && WindSpeed > IdleSpeed)
	{
		ApplyWind(DeltaTime);
	}

	if (bDrawDebug)
	{
		const FVector Apex = GetComponentLocation();
		const FLinearColor Tint = FLinearColor::LerpUsingHSV(
			FLinearColor(0.2f, 0.5f, 1.0f), FLinearColor(1.0f, 0.85f, 0.2f), GetWindStrengthAlpha());
		const FColor Color = Tint.ToFColor(true);
		const float HalfAngleRad = FMath::DegreesToRadians(ConeHalfAngle);

		DrawDebugCone(GetWorld(), Apex, WindDirection, ConeLength, HalfAngleRad, HalfAngleRad, 16, Color, false, -1.0f, 0, 2.0f);

		const float StreakLength = FMath::Lerp(200.0f, ConeLength, GetWindStrengthAlpha());
		DrawDebugLine(GetWorld(), Apex, Apex + WindDirection * StreakLength, Color, false, -1.0f, 0, 4.0f);
	}
}

void UWindGustComponent::UpdateCharge(float DeltaTime)
{
	if (bCharging)
	{
		ChargeAlpha = FMath::Min(ChargeAlpha + DeltaTime / FMath::Max(ChargeTime, UE_KINDA_SMALL_NUMBER), 1.0f);
		TargetWindSpeed = MaxWindSpeed * FMath::Pow(ChargeAlpha, ChargeExponent);
	}
	else
	{
		ChargeAlpha = FMath::Max(ChargeAlpha - DeltaTime / FMath::Max(DischargeTime, UE_KINDA_SMALL_NUMBER), 0.0f);

		// Releasing only drops the *request* to zero. UpdateEnvelope is what makes
		// the wind itself take its time to go away.
		TargetWindSpeed = 0.0f;
	}
}

void UWindGustComponent::UpdateEnvelope(float DeltaTime)
{
	const FVector TargetDirection = GetForwardVector().GetSafeNormal();

	// Direction lags the aim, so whipping the camera around bends the gust
	// instead of snapping it.
	WindDirection = SpringVector(WindDirection, WindDirectionVelocity, TargetDirection, DirectionStiffness, DirectionDamping, DeltaTime);
	if (!WindDirection.Normalize(UE_KINDA_SMALL_NUMBER))
	{
		WindDirection = TargetDirection;
		WindDirectionVelocity = FVector::ZeroVector;
	}

	const bool bClimbing = TargetWindSpeed > WindSpeed;
	WindSpeed = SpringScalar(WindSpeed, WindSpeedVelocity, TargetWindSpeed,
		bClimbing ? RiseStiffness : FallStiffness,
		bClimbing ? RiseDamping : FallDamping,
		DeltaTime);

	if (WindSpeed < 0.0f)
	{
		WindSpeed = 0.0f;
		WindSpeedVelocity = 0.0f;
	}
}

void UWindGustComponent::ApplyWind(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Apex = GetComponentLocation();
	const FVector Direction = WindDirection;
	const FVector WindVelocity = Direction * WindSpeed;

	const float Length = FMath::Max(ConeLength, 1.0f);
	const float HalfAngleRad = FMath::DegreesToRadians(FMath::Clamp(ConeHalfAngle, 1.0f, 89.0f));
	const float BaseRadius = Length * FMath::Tan(HalfAngleRad);

	// Smallest sphere containing the whole cone (apex plus base circle), where
	// r = (L^2 + R^2) / 2L. This keeps the broad phase from scanning more of the
	// world than the cone actually covers.
	const float SphereDistance = (Length * Length + BaseRadius * BaseRadius) / (2.0f * Length);
	const FVector SphereCenter = Apex + Direction * SphereDistance;

	const FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllDynamicObjects);
	const FCollisionQueryParams QueryParams(FName(TEXT("WindGust")), /*bTraceComplex=*/false, GetOwner());

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, SphereCenter, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(SphereDistance), QueryParams);

	if (Overlaps.Num() == 0)
	{
		return;
	}

	TSet<UPrimitiveComponent*> Handled;
	int32 Affected = 0;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (Affected >= MaxAffectedBodies)
		{
			break;
		}

		UPrimitiveComponent* Body = Overlap.GetComponent();
		if (!Body || Body->GetOwner() == GetOwner() || Handled.Contains(Body))
		{
			continue;
		}

		if (!Body->IsSimulatingPhysics())
		{
			continue;
		}

		const FVector ToBody = Body->GetCenterOfMass() - Apex;
		const float Distance = ToBody.Size();
		if (Distance > Length)
		{
			continue;
		}

		// Radial and angular falloff multiplied together: strongest straight ahead
		// and close by, fading toward both the cone edge and its far end.
		float AngleAlpha = 1.0f;
		if (Distance > UE_KINDA_SMALL_NUMBER)
		{
			const float CosAngle = FVector::DotProduct(ToBody / Distance, Direction);
			const float Angle = FMath::Acos(FMath::Clamp(CosAngle, -1.0f, 1.0f));
			if (Angle > HalfAngleRad)
			{
				continue;
			}

			AngleAlpha = 1.0f - (Angle / HalfAngleRad);
		}

		const float RadialAlpha = 1.0f - (Distance / Length);
		const float Falloff = FMath::Pow(FMath::Clamp(RadialAlpha * AngleAlpha, 0.0f, 1.0f), FalloffExponent);
		if (Falloff <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		Handled.Add(Body);
		ApplyWindToBody(Body, WindVelocity, Falloff, DeltaTime);
		++Affected;
	}
}

void UWindGustComponent::ApplyWindToBody(UPrimitiveComponent* Body, const FVector& WindVelocity, float Falloff, float DeltaTime)
{
	const FVector RelativeVelocity = WindVelocity - Body->GetPhysicsLinearVelocity();

	// Clamping to a full swap keeps a high response rate stable at low frame
	// rates instead of oscillating around the wind velocity.
	const float Rate = WindResponseRate * GetMassResponseScale(Body) * Falloff;
	const float Alpha = FMath::Clamp(Rate * DeltaTime, 0.0f, 1.0f);

	if (!Body->IsAnyRigidBodyAwake())
	{
		Body->WakeAllRigidBodies();
	}

	// A velocity change rather than a force: mass is already accounted for by
	// GetMassResponseScale, so props react predictably whatever they weigh.
	Body->AddImpulse(RelativeVelocity * Alpha, NAME_None, /*bVelChange=*/true);
}

float UWindGustComponent::GetMassResponseScale(const UPrimitiveComponent* Body) const
{
	if (MassInfluence <= 0.0f)
	{
		return 1.0f;
	}

	const float Mass = FMath::Max(Body->GetMass(), 1.0f);
	const float Scale = FMath::Pow(Mass / FMath::Max(ReferenceMass, 1.0f), -MassInfluence);

	return FMath::Clamp(Scale, MinMassResponse, MaxMassResponse);
}

float UWindGustComponent::SpringScalar(float& InOutValue, float& InOutVelocity, float Target, float Stiffness, float DampingRatio, float DeltaTime)
{
	const float K = FMath::Max(Stiffness, UE_KINDA_SMALL_NUMBER);
	const float C = 2.0f * FMath::Sqrt(K) * FMath::Max(DampingRatio, 0.0f);

	// Fixed sub-steps keep the integrator stable no matter how stiff the spring is.
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / 0.008f), 1, 16);
	const float Step = DeltaTime / Steps;

	for (int32 Index = 0; Index < Steps; ++Index)
	{
		InOutVelocity += (K * (Target - InOutValue) - C * InOutVelocity) * Step;
		InOutValue += InOutVelocity * Step;
	}

	return InOutValue;
}

FVector UWindGustComponent::SpringVector(FVector& InOutValue, FVector& InOutVelocity, const FVector& Target, float Stiffness, float DampingRatio, float DeltaTime)
{
	const float K = FMath::Max(Stiffness, UE_KINDA_SMALL_NUMBER);
	const float C = 2.0f * FMath::Sqrt(K) * FMath::Max(DampingRatio, 0.0f);

	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / 0.008f), 1, 16);
	const float Step = DeltaTime / Steps;

	for (int32 Index = 0; Index < Steps; ++Index)
	{
		InOutVelocity += (K * (Target - InOutValue) - C * InOutVelocity) * Step;
		InOutValue += InOutVelocity * Step;
	}

	return InOutValue;
}
