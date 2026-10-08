// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "WindFlierPawn.generated.h"

class UCameraComponent;
class UInputAction;
class UWindGustComponent;
struct FInputActionValue;

/**
 * Bodyless gliding "wind spirit" pawn.
 *
 * Flight: input feeds an acceleration into a velocity that air drag bleeds off
 * every frame, so the pawn coasts after the input stops instead of halting dead.
 * The pawn carries no collision shape at all, so it drifts straight through the
 * world and can never push anything by itself.
 *
 * Aim: input deltas move a target rotation and the actual rotation chases it
 * with a short lag, banking into lateral motion on the way. Turning the gust
 * therefore always trails the crosshair slightly.
 *
 * Input: the optional Enhanced Input actions are used when they are assigned;
 * otherwise the legacy "WindFlier_*" mappings in Config/DefaultInput.ini are
 * bound, so the pawn also works in a project that has no input assets at all.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Wind Flier Pawn"))
class AWindFlierPawn : public APawn
{
	GENERATED_BODY()

public:

	AWindFlierPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Wind emitter fired by this pawn. */
	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	UWindGustComponent* GetWindGust() const { return WindGust; }

	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	UCameraComponent* GetViewCamera() const { return ViewCamera; }

	// --- Input entry points, also callable from Blueprint or UMG ---

	/** Throttle, each component in the range -1..1. */
	UFUNCTION(BlueprintCallable, Category = "Wind Flier|Input")
	void DoMove(float Right, float Forward, float Up);

	/** Aim delta, in look units (LookSensitivity turns them into degrees). */
	UFUNCTION(BlueprintCallable, Category = "Wind Flier|Input")
	void DoLook(float Yaw, float Pitch);

	/** Start holding the gust. */
	UFUNCTION(BlueprintCallable, Category = "Wind Flier|Input")
	void DoChargeStart();

	/** Release the gust. */
	UFUNCTION(BlueprintCallable, Category = "Wind Flier|Input")
	void DoChargeStop();

	// --- State ---

	/** Current glide velocity (cm/s). */
	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	FVector GetFlyVelocity() const { return FlyVelocity; }

	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	float GetFlySpeed() const { return FlyVelocity.Size(); }

	/** Rotation the pawn is actually using, including bank. */
	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	FRotator GetAimRotation() const { return FRotator(CurrentPitch, CurrentYaw, CurrentBank); }

protected:

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// --- Components ---

	/** Collisionless root: the pawn has no body to collide with anything. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ViewCamera;

	/** The gust emanates from the camera, so the wind always leaves through the crosshair. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWindGustComponent> WindGust;

	// --- Flight tuning ---

	/** Glide speed the drag settles at under full throttle (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (ClampMin = "1"))
	float MaxFlySpeed = 1800.0f;

	/** Thrust at full throttle (cm/s^2). Drag settles the speed at
	 *  FlyAcceleration / AirDrag, so keep this near AirDrag * MaxFlySpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (ClampMin = "0"))
	float FlyAcceleration = 4700.0f;

	/** Vertical thrust multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (ClampMin = "0"))
	float VerticalThrustScale = 1.0f;

	/** Air drag (1/s). Lower values glide for longer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (ClampMin = "0"))
	float AirDrag = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight")
	bool bApplyGravity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (EditCondition = "bApplyGravity"))
	float GravityScale = 0.15f;

	// --- Aim tuning ---

	/** Degrees of aim per unit of look input. 2.5 matches the engine legacy default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim", meta = (ClampMin = "0"))
	float LookSensitivity = 2.5f;

	/** How fast the aim catches up with the input target (1/s). Lower means more drag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim", meta = (ClampMin = "0"))
	float AimInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim")
	float MinPitch = -89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim")
	float MaxPitch = 89.0f;

	/** Roll banked into lateral motion while gliding (degrees). 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim", meta = (ClampMin = "0", ClampMax = "60"))
	float BankAngleMax = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim", meta = (ClampMin = "0"))
	float BankInterpSpeed = 4.0f;

	// --- Optional Enhanced Input actions (legacy mappings are used when unset) ---

	UPROPERTY(EditAnywhere, Category = "Wind Flier|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Wind Flier|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Wind Flier|Input")
	TObjectPtr<UInputAction> ChargeAction;

private:

	void Input_MoveForward(float Value) { MoveInputForward = Value; }
	void Input_MoveRight(float Value) { MoveInputRight = Value; }
	void Input_MoveUp(float Value) { MoveInputUp = Value; }
	void Input_LookYaw(float Value) { DoLook(Value, 0.0f); }
	void Input_LookPitch(float Value) { DoLook(0.0f, Value); }

	void Input_MoveAction(const FInputActionValue& Value);
	void Input_MoveActionStopped(const FInputActionValue& Value);
	void Input_LookAction(const FInputActionValue& Value);

	void UpdateFlight(float DeltaSeconds);
	void UpdateAim(float DeltaSeconds);

	/** Throttle gathered from whichever input path is active, consumed by UpdateFlight. */
	float MoveInputRight = 0.0f;
	float MoveInputForward = 0.0f;
	float MoveInputUp = 0.0f;

	FVector FlyVelocity = FVector::ZeroVector;

	float TargetYaw = 0.0f;
	float TargetPitch = 0.0f;
	float CurrentYaw = 0.0f;
	float CurrentPitch = 0.0f;
	float CurrentBank = 0.0f;
};
