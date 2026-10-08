// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WindGustComponent.generated.h"

class UPrimitiveComponent;

/** Where a gust currently is in its start-up / hold / die-down cycle. */
UENUM(BlueprintType)
enum class EWindGustPhase : uint8
{
	/** Below the blowing threshold: no wind at all. */
	Idle    UMETA(DisplayName = "Idle"),
	/** Charge is building and the wind is still climbing toward it (wind up). */
	Rising  UMETA(DisplayName = "Rising"),
	/** The wind has caught up with the charge and holds there. */
	Holding UMETA(DisplayName = "Holding"),
	/** Input is gone and the wind is bleeding off (die down). */
	Falling UMETA(DisplayName = "Falling"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWindGustChargeChanged, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWindGustPhaseChanged, EWindGustPhase, Phase);

/**
 * Player-driven wind emitter: a collisionless cone of air blown along the
 * component's forward vector.
 *
 * The feel is built in three layers, so that nothing about the wind is instant:
 *
 *  1. Charge   - holding builds a charge, releasing bleeds it off. This is the
 *                player's intent, not the wind itself.
 *  2. Envelope - the wind's own speed and direction are springs chasing that
 *                intent, so a gust ramps up, overshoots a little, settles, and
 *                keeps blowing for a while after the release.
 *  3. Response - rigid bodies inside the cone are dragged toward the local wind
 *                velocity rather than snapped to it, so they pick up speed over
 *                time and keep drifting once the wind has gone.
 *
 * The emitter itself has no collision and never shoves anything directly; it
 * only accelerates the rigid bodies it finds.
 */
UCLASS(ClassGroup = (Wind), meta = (BlueprintSpawnableComponent, DisplayName = "Wind Gust Source"))
class UWindGustComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UWindGustComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Input ---

	/** Hold to blow, release to let the wind die down. */
	UFUNCTION(BlueprintCallable, Category = "Wind Gust|Input")
	void SetCharging(bool bInCharging);

	UFUNCTION(BlueprintCallable, Category = "Wind Gust|Input")
	void StartCharging() { SetCharging(true); }

	UFUNCTION(BlueprintCallable, Category = "Wind Gust|Input")
	void StopCharging() { SetCharging(false); }

	UFUNCTION(BlueprintPure, Category = "Wind Gust|Input")
	bool IsCharging() const { return bCharging; }

	// --- State ---

	/** Player charge, 0..1. Takes ChargeTime to fill and DischargeTime to bleed off. */
	UFUNCTION(BlueprintPure, Category = "Wind Gust|State")
	float GetChargeAlpha() const { return ChargeAlpha; }

	/** Live wind speed as a fraction of MaxWindSpeed. Lags the charge on purpose. */
	UFUNCTION(BlueprintPure, Category = "Wind Gust|State")
	float GetWindStrengthAlpha() const;

	/** Live wind speed in cm/s. */
	UFUNCTION(BlueprintPure, Category = "Wind Gust|State")
	float GetWindSpeed() const { return WindSpeed; }

	/** Live wind velocity in world space. */
	UFUNCTION(BlueprintPure, Category = "Wind Gust|State")
	FVector GetWindVelocity() const { return WindDirection * WindSpeed; }

	UFUNCTION(BlueprintPure, Category = "Wind Gust|State")
	EWindGustPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Wind Gust|State")
	bool IsBlowing() const { return Phase != EWindGustPhase::Idle; }

	/** Charge changed by a meaningful step. */
	UPROPERTY(BlueprintAssignable, Category = "Wind Gust|Events")
	FWindGustChargeChanged OnChargeChanged;

	/** The gust entered a new phase. */
	UPROPERTY(BlueprintAssignable, Category = "Wind Gust|Events")
	FWindGustPhaseChanged OnPhaseChanged;

	// --- Charge tuning ---

	/** Seconds of holding needed to reach a full charge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Charge", meta = (ClampMin = "0.01"))
	float ChargeTime = 0.9f;

	/** Shaping curve applied to the charge before it drives the wind. 1 = linear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Charge", meta = (ClampMin = "0.1", UIMax = "4"))
	float ChargeExponent = 1.0f;

	/** Seconds for a released charge to bleed back to zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Charge", meta = (ClampMin = "0.01"))
	float DischargeTime = 0.45f;

	// --- Envelope tuning (the inertia of the wind itself) ---

	/** Spring stiffness of the wind speed while it is climbing. Higher = snappier wind up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Envelope", meta = (ClampMin = "0.01"))
	float RiseStiffness = 6.0f;

	/** Damping ratio while climbing. Below 1 overshoots slightly, which reads as a gust. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Envelope", meta = (ClampMin = "0.05", UIMax = "2"))
	float RiseDamping = 0.8f;

	/** Spring stiffness of the wind speed while it is dying down. Lower = longer tail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Envelope", meta = (ClampMin = "0.01"))
	float FallStiffness = 2.4f;

	/** Damping ratio while dying down. 1 = no overshoot, so the tail just fades out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Envelope", meta = (ClampMin = "0.05", UIMax = "2"))
	float FallDamping = 1.0f;

	/** Spring stiffness of the wind direction. Keeps the aim from teleporting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Envelope", meta = (ClampMin = "0.01"))
	float DirectionStiffness = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Envelope", meta = (ClampMin = "0.05", UIMax = "2"))
	float DirectionDamping = 1.0f;

	// --- Output ---

	/** Wind speed at a full charge (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Output", meta = (ClampMin = "0"))
	float MaxWindSpeed = 1800.0f;

	/** Share of MaxWindSpeed below which the gust counts as idle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Output", meta = (ClampMin = "0", UIMax = "0.5"))
	float IdleThreshold = 0.04f;

	// --- Affected volume ---

	/** Half angle of the cone, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Cone", meta = (ClampMin = "1", ClampMax = "89"))
	float ConeHalfAngle = 30.0f;

	/** Reach of the cone, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Cone", meta = (ClampMin = "1"))
	float ConeLength = 2500.0f;

	/** Falloff curve over the cone. 1 = linear from the apex, higher = tighter core. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Cone", meta = (ClampMin = "0.01", UIMax = "6"))
	float FalloffExponent = 1.0f;

	/** Hard cap on how many bodies a single gust may touch per frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Cone", meta = (ClampMin = "1"))
	int32 MaxAffectedBodies = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Cone")
	bool bAffectPhysicsBodies = true;

	// --- Body response tuning ---

	/** How fast a body at the reference mass is dragged toward the wind (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Response", meta = (ClampMin = "0.01"))
	float WindResponseRate = 3.0f;

	/**
	 * How strongly body mass slows that drag down. 0 ignores mass entirely,
	 * 1 makes the response time exactly proportional to mass. Values around 0.3
	 * keep heavy props sluggish without ever making them feel stuck.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Response", meta = (ClampMin = "0", ClampMax = "1"))
	float MassInfluence = 0.35f;

	/** Mass the response rate is tuned for, in kg (a 1 m cube at default density). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Response", meta = (ClampMin = "1"))
	float ReferenceMass = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Response", meta = (ClampMin = "0.05", UIMax = "10"))
	float MinMassResponse = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Response", meta = (ClampMin = "0.05", UIMax = "10"))
	float MaxMassResponse = 3.0f;

	// --- Debug ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Debug")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Gust|Debug")
	bool bLogWind = false;

private:

	void UpdateCharge(float DeltaTime);
	void UpdateEnvelope(float DeltaTime);
	void ApplyWind(float DeltaTime);
	void ApplyWindToBody(UPrimitiveComponent* Body, const FVector& WindVelocity, float Falloff, float DeltaTime);

	float GetMassResponseScale(const UPrimitiveComponent* Body) const;

	/** Spring integrator, used for the wind's own speed and direction. */
	static float SpringScalar(float& InOutValue, float& InOutVelocity, float Target, float Stiffness, float DampingRatio, float DeltaTime);
	static FVector SpringVector(FVector& InOutValue, FVector& InOutVelocity, const FVector& Target, float Stiffness, float DampingRatio, float DeltaTime);

	/** Player intent, 0..1. */
	float ChargeAlpha = 0.0f;

	/** Wind speed the player is asking for, after the charge curve. */
	float TargetWindSpeed = 0.0f;

	/** The wind's own state, driven by springs so that it never snaps. */
	float WindSpeed = 0.0f;
	float WindSpeedVelocity = 0.0f;
	FVector WindDirection = FVector::ForwardVector;
	FVector WindDirectionVelocity = FVector::ZeroVector;

	EWindGustPhase Phase = EWindGustPhase::Idle;
	bool bCharging = false;
	float LastBroadcastCharge = -1.0f;
};
