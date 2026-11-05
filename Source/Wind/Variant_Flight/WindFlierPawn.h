// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "WindFlierPawn.generated.h"

class UCameraComponent;
class UInputAction;
struct FInputActionValue;

/**
 * 无实体滑翔的"风之精灵"Pawn。
 *
 * 飞行：输入馈入加速度并累加到速度上，空气阻力每帧将速度衰减一些，所以
 * 松开输入后 Pawn 会继续滑行而不是立即停止。Pawn 不带任何碰撞形状，
 * 会笔直穿过场景，自身也无法推动任何物体。
 *
 * 瞄准：输入增量驱动一个目标旋转，实际旋转以略带延迟地追上它，同时根据
 * 横向移动倾斜滚转。因此转向永远稍稍滞后于准心。
 *
 * 输入：如果 Pawn 指定了可选的 Enhanced Input Actions，就优先使用它们；
 * 否则会退回到 Config/DefaultInput.ini 中的 "WindFlier_*" 旧式映射，
 * 所以即便项目里没有任何输入资产，Pawn 也能工作。
 */
UCLASS(Blueprintable, meta = (DisplayName = "Wind Flier Pawn"))
class AWindFlierPawn : public APawn
{
	GENERATED_BODY()

public:

	AWindFlierPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	UCameraComponent* GetViewCamera() const { return ViewCamera; }

	// --- 输入入口，蓝图或 UMG 也可以直接调用 ---

	/** 推力输入，各分量在 -1..1 范围内。 */
	UFUNCTION(BlueprintCallable, Category = "Wind Flier|Input")
	void DoMove(float Right, float Forward, float Up);

	/** 瞄准增量，以 look 单位计（LookSensitivity 将其换算为角度）。 */
	UFUNCTION(BlueprintCallable, Category = "Wind Flier|Input")
	void DoLook(float Yaw, float Pitch);

	// --- 状态 ---

	/** 当前滑翔速度（cm/s）。 */
	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	FVector GetFlyVelocity() const { return FlyVelocity; }

	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	float GetFlySpeed() const { return FlyVelocity.Size(); }

	/** Pawn 实际使用的旋转，包含侧倾。 */
	UFUNCTION(BlueprintPure, Category = "Wind Flier")
	FRotator GetAimRotation() const { return FRotator(CurrentPitch, CurrentYaw, CurrentBank); }

protected:

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// --- 组件 ---

	/** 无碰撞的根节点：Pawn 没有身体，无法与任何物体碰撞。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ViewCamera;

	// --- 飞行调参 ---

	/** 全推力下，阻力使速度最终稳定到的滑翔速度（cm/s）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (ClampMin = "1"))
	float MaxFlySpeed = 1800.0f;

	/** 全推力时的推进力（cm/s^2）。阻力会使速度稳定在 FlyAcceleration / AirDrag，
	 *  因此让它接近 AirDrag * MaxFlySpeed。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (ClampMin = "0"))
	float FlyAcceleration = 4700.0f;

	/** 垂直推力倍率。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (ClampMin = "0"))
	float VerticalThrustScale = 1.0f;

	/** 空气阻力（1/s）。值越小滑翔越久。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (ClampMin = "0"))
	float AirDrag = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight")
	bool bApplyGravity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Flight", meta = (EditCondition = "bApplyGravity"))
	float GravityScale = 0.15f;

	// --- 瞄准调参 ---

	/** 每单位 look 输入对应的旋转角度。2.5 匹配引擎的旧式默认值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim", meta = (ClampMin = "0"))
	float LookSensitivity = 2.5f;

	/** 瞄准追上输入目标的速度（1/s）。值越小越拖拽。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim", meta = (ClampMin = "0"))
	float AimInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim")
	float MinPitch = -89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim")
	float MaxPitch = 89.0f;

	/** 根据横向速度叠加的侧倾滚转角（度）。0 表示关闭。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim", meta = (ClampMin = "0", ClampMax = "60"))
	float BankAngleMax = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Flier|Aim", meta = (ClampMin = "0"))
	float BankInterpSpeed = 4.0f;

	// --- 可选的 Enhanced Input Actions（未指定时使用旧式映射） ---

	UPROPERTY(EditAnywhere, Category = "Wind Flier|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Wind Flier|Input")
	TObjectPtr<UInputAction> LookAction;

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

	/** 从任意活跃输入路径收集的推力输入，由 UpdateFlight 消费。 */
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
