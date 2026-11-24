// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BambooSword.generated.h"

class ABambooStalk;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * 一把用条形 Box 当刀身的剑。
 *
 * 剑不判断"点击"：任何让它动起来的东西（骨骼动画、蓝图时间轴、自带的 Swing）都会让刀身在相邻
 * 两帧之间扫过一段体积。扫到竹子时，按碰撞点的绝对高度砍断，倒向取刀身在该点上的运动方向。
 *
 * Actor 原点就是握把，刀身沿本地 +X 伸出，所以直接挂到手部 socket 即可。
 */
UCLASS(meta = (DisplayName = "Bamboo Sword"))
class ABambooSword : public AActor
{
	GENERATED_BODY()

public:

	ABambooSword();

	// AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 挥一下：绕挂点的本地 Z 轴转 SwingAngle 度再回到原位，供没有动画时试玩。 */
	UFUNCTION(BlueprintCallable, Category = "Sword")
	void Swing();

	UFUNCTION(BlueprintPure, Category = "Sword")
	bool IsSwinging() const { return SwingElapsed >= 0.0f; }

	/** 刀身的判定体积（同时用于每帧扫掠与可视化）。 */
	UFUNCTION(BlueprintPure, Category = "Sword")
	UBoxComponent* GetBladeBox() const { return BladeBox; }

protected:

	/** 沿刀身扫一段体积，命中竹子就砍。 */
	void SweepBlade(float DeltaSeconds);

	/** 在碰撞点按"刀身方向 + 刀身在该点的运动方向"张成的平面砍断竹子，砍击速度转成断竹的冲量。 */
	void CutBamboo(ABambooStalk* Stalk, const FVector& ImpactPoint, const FVector& MotionVelocity);
	/** 按刀身尺寸刷新网格与 Box。 */
	void UpdateBladeShape();

	void UpdateSwing(float DeltaSeconds);

	/** 握把：Actor 原点，挂在手部 socket 上。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Grip;

	/** 刀身外观：引擎自带 Cube 拉成一条，不依赖任何美术资产。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BladeMesh;

	/** 刀身判定体积，尺寸与 BladeMesh 一致。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BladeBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sword", meta = (ClampMin = "10"))
	float BladeLength = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sword", meta = (ClampMin = "0.5"))
	float BladeWidth = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sword", meta = (ClampMin = "0.5"))
	float BladeThickness = 1.2f;

	/** 刀梢速度低于这个值就不判定，避免把剑靠在竹子上也算砍。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sword", meta = (ClampMin = "0"))
	float MinCutSpeed = 120.0f;

	/** Swing() 的挥动角度（度）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sword")
	float SwingAngle = 120.0f;

	/** Swing() 一次挥动的时长（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sword", meta = (ClampMin = "0.05"))
	float SwingDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sword")
	bool bDrawDebug = false;

private:

	/** 上一帧的刀身变换，用来把这一帧的扫掠连成一段体积。 */
	FTransform PreviousBladeTransform = FTransform::Identity;
	bool bHasPreviousBladeTransform = false;

	/** 挥剑进度；< 0 表示没在挥。 */
	float SwingElapsed = -1.0f;
	FRotator SwingRestRotation = FRotator::ZeroRotator;
};
