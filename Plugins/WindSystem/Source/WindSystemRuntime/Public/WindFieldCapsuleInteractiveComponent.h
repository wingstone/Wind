// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WindFieldSourceComponent.h"
#include "WindFieldCapsuleInteractiveComponent.generated.h"

/**
 * Capsule interactive wind source driven by component motion.
 *
 * The component computes linear velocity from translation and angular velocity
 * from rotation every tick, then uploads both to GPU. Shader-side force
 * injection uses rigid-body style velocity:
 *   v(x) = v_linear + (omega x r)
 * where r is world-space offset from capsule center.
 */
UCLASS(ClassGroup = (Wind), meta = (BlueprintSpawnableComponent, DisplayName = "Wind Field Capsule Interactive Source"))
class WINDSYSTEMRUNTIME_API UWindFieldCapsuleInteractiveComponent : public UWindFieldSourceComponent
{
	GENERATED_BODY()

public:
	UWindFieldCapsuleInteractiveComponent();

	/** Enable/disable motion-driven interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Interactive")
	bool bEnableInteraction = true;

	/** Capsule radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Interactive|Shape", meta = (ClampMin = "1"))
	float Radius = 80.0f;

	/** Capsule half-height (cm), UE-style (including hemispherical caps). Must be >= Radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Interactive|Shape", meta = (ClampMin = "0"))
	float HalfHeight = 160.0f;

	/** Ignore tiny linear motion to reduce jitter noise (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Interactive|Motion", meta = (ClampMin = "0"))
	float MinLinearSpeed = 0.001f;

	/** Ignore tiny angular motion to reduce jitter noise (rad/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Interactive|Motion", meta = (ClampMin = "0"))
	float MinAngularSpeed = 0.001f;

	/** Scale applied to computed linear velocity before GPU upload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Interactive|Motion")
	float LinearVelocityScale = 1.0f;

	/** Scale applied to computed angular velocity before GPU upload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Interactive|Motion")
	float AngularVelocityScale = 1.0f;

	/** Radial falloff exponent inside the capsule (1=linear, 2=quadratic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Interactive|Shape", meta = (ClampMin = "0.001"))
	float InfluenceFalloffExponent = 1.0f;

	virtual EWindFieldSourceType GetWindType() const override { return EWindFieldSourceType::CapsuleInteractive; }
	virtual FGPUWindSourceData ToGPUData() const override;
	virtual void DrawDebug(float Lifetime = 0.0f) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FVector LastPosition = FVector::ZeroVector;
	FQuat LastRotation = FQuat::Identity;
	FVector CachedLinearVelocity = FVector::ZeroVector;
	FVector CachedAngularVelocity = FVector::ZeroVector;
	bool bHasLastTransform = false;

	void UpdateMotionCache(float DeltaTime);
};
