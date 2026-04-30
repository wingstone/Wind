// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldCapsuleInteractiveComponent.h"
#include "DrawDebugHelpers.h"

UWindFieldCapsuleInteractiveComponent::UWindFieldCapsuleInteractiveComponent()
{
}

void UWindFieldCapsuleInteractiveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableInteraction)
	{
		CachedLinearVelocity = FVector::ZeroVector;
		CachedAngularVelocity = FVector::ZeroVector;
		LastPosition = GetComponentLocation();
		LastRotation = GetComponentQuat();
		bHasLastTransform = true;
		return;
	}

	UpdateMotionCache(DeltaTime);
}

void UWindFieldCapsuleInteractiveComponent::UpdateMotionCache(float DeltaTime)
{
	const FVector CurrentPosition = GetComponentLocation();
	const FQuat CurrentRotation = GetComponentQuat();

	if (!bHasLastTransform || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		LastPosition = CurrentPosition;
		LastRotation = CurrentRotation;
		CachedLinearVelocity = FVector::ZeroVector;
		CachedAngularVelocity = FVector::ZeroVector;
		bHasLastTransform = true;
		return;
	}

	CachedLinearVelocity = (CurrentPosition - LastPosition) / DeltaTime;
	if (CachedLinearVelocity.SizeSquared() < FMath::Square(MinLinearSpeed))
	{
		CachedLinearVelocity = FVector::ZeroVector;
	}

	FQuat DeltaQuat = CurrentRotation * LastRotation.Inverse();
	DeltaQuat.Normalize();

	FVector Axis = FVector::ForwardVector;
	float Angle = 0.0f;
	DeltaQuat.ToAxisAndAngle(Axis, Angle);
	Angle = FMath::UnwindRadians(Angle);
	CachedAngularVelocity = Axis.GetSafeNormal() * (Angle / DeltaTime);
	if (CachedAngularVelocity.SizeSquared() < FMath::Square(MinAngularSpeed))
	{
		CachedAngularVelocity = FVector::ZeroVector;
	}

	LastPosition = CurrentPosition;
	LastRotation = CurrentRotation;
}

FGPUWindSourceData UWindFieldCapsuleInteractiveComponent::ToGPUData() const
{
	FGPUWindSourceData Data = {};

	const float SafeRadius = FMath::Max(Radius, 1.0f);
	const float SafeHalfHeight = FMath::Max(HalfHeight, SafeRadius);

	const FVector Linear = CachedLinearVelocity * LinearVelocityScale;
	const FVector Angular = CachedAngularVelocity * AngularVelocityScale;

	Data.Position = FVector3f(GetComponentLocation());
	Data.Direction = FVector3f(GetForwardVector().GetSafeNormal());
	Data.Radius = SafeRadius;
	Data.HalfHeight = SafeHalfHeight;
	Data.WindType = static_cast<uint32>(GetWindType());

	// Capsule interactive payload:
	// Strength/InnerRadius/FalloffExponent -> LinearVelocity xyz
	// EndRadius/Padding3/Padding4          -> AngularVelocity xyz
	// Padding                               -> InfluenceFalloffExponent
	Data.Strength = Linear.X;
	Data.InnerRadius = Linear.Y;
	Data.FalloffExponent = Linear.Z;
	Data.EndRadius = Angular.X;
	Data.Padding3 = Angular.Y;
	Data.Padding4 = Angular.Z;
	Data.Padding = FMath::Max(InfluenceFalloffExponent, 0.001f);

	return Data;
}

void UWindFieldCapsuleInteractiveComponent::DrawDebug(float Lifetime) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetComponentLocation();
	const FQuat Rotation = GetComponentQuat();
	const float SafeRadius = FMath::Max(Radius, 1.0f);
	const float SafeHalfHeight = FMath::Max(HalfHeight, SafeRadius);

	DrawDebugCapsule(World, Center, SafeHalfHeight, SafeRadius, Rotation, FColor::Orange, false, Lifetime, 0, 1.5f);

	if (!CachedLinearVelocity.IsNearlyZero())
	{
		const float ArrowLen = FMath::Clamp(CachedLinearVelocity.Size() * 0.05f, 30.0f, 250.0f);
		DrawDebugDirectionalArrow(World, Center, Center + CachedLinearVelocity.GetSafeNormal() * ArrowLen, 18.0f, FColor::Cyan, false, Lifetime, 0, 2.0f);
	}

	if (!CachedAngularVelocity.IsNearlyZero())
	{
		const FVector Axis = CachedAngularVelocity.GetSafeNormal();
		const float AxisLen = FMath::Clamp(CachedAngularVelocity.Size() * 25.0f, 30.0f, 180.0f);
		DrawDebugLine(World, Center - Axis * AxisLen, Center + Axis * AxisLen, FColor::Purple, false, Lifetime, 0, 1.5f);
	}
}
