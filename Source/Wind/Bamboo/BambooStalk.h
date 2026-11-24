// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BambooTypes.h"
#include "GameFramework/Actor.h"
#include "BambooStalk.generated.h"

class ABambooCutPiece;
class UProceduralMeshComponent;

/**
 * 一根可以砍断的竹子。
 *
 * 本体由 UProceduralMeshComponent 在本地空间生成（Z 轴向上、根部在原点）。砍断时自身网格被
 * 重建成剩下的树桩，同时在切口处生成一个带物理模拟的 ABambooCutPiece 作为倒下的上半段。
 * 切口是一个平面，跟着砍它的东西（剑）的运动方向倾斜。
 */
UCLASS(meta = (DisplayName = "Bamboo Stalk"))
class ABambooStalk : public AActor
{
	GENERATED_BODY()

public:

	ABambooStalk();

	// AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 在本地空间高度 CutHeight 处水平砍断；ImpactSpeed 是砍击速度（厘米/秒），决定断竹被甩出的力度。 */
	UFUNCTION(BlueprintCallable, Category = "Bamboo")
	ABambooCutPiece* Cut(float CutHeight, FVector FallDirection = FVector::ForwardVector, float ImpactSpeed = 0.0f);

	/** 以世界空间命中点的高度作为水平切口。 */
	UFUNCTION(BlueprintCallable, Category = "Bamboo")
	ABambooCutPiece* CutAtLocation(FVector WorldLocation, FVector FallDirection = FVector::ForwardVector, float ImpactSpeed = 0.0f);

	/**
	 * 沿一个平面砍断：WorldPoint 是切口上的一点，WorldNormal 指向倒下的那一半。
	 * 法线会被夹到接近竖直，保证两段还能整段分离；切口越界时会沿 Z 推回竹子内部。
	 * ImpactSpeed 传给断竹作为物理冲量的大小。
	 */
	UFUNCTION(BlueprintCallable, Category = "Bamboo")
	ABambooCutPiece* CutAlongPlane(FVector WorldPoint, FVector WorldNormal, FVector FallDirection = FVector::ForwardVector, float ImpactSpeed = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Bamboo")
	bool IsCut() const { return bCut; }

	/** 还没被砍掉的部分的高度（轴心处）。 */
	UFUNCTION(BlueprintPure, Category = "Bamboo")
	float GetRemainingHeight() const { return RemainingHeight; }

	/** 竹子的外形参数，倒下的那一半会复用它，保证两段接得上。 */
	const FBambooStalkParams& GetParams() const { return Params; }

protected:

	/** 用 [BottomHeight, TopHeight] 重建自身网格；CutSide 非 None 时那一端是斜口。 */
	void RebuildMesh(float BottomHeight, float TopHeight, FBambooGeometry::ECutSide CutSide = FBambooGeometry::ECutSide::None);

	/** 关掉自定义切面时的那条路：直接调 UKismetProceduralMeshLibrary::SliceProceduralMesh。 */
	ABambooCutPiece* CutWithEngineSlice(FVector WorldPoint, FVector WorldNormal, FVector FallDirection, float ImpactSpeed);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bamboo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> StalkMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bamboo")
	FBambooStalkParams Params;

	/**
	 * 开启时用本项目自带的切面几何：斜切面精确、切口是环状（中空）、断开的两段严丝合缝。
	 * 关闭时直接用引擎的 UKismetProceduralMeshLibrary::SliceProceduralMesh，切口会被封成实心、
	 * 也没有节环/斜口的额外处理，可以拿来对照。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bamboo")
	bool bUseCustomCutMesh = true;

private:

	/** 砍断后剩下的高度；未砍时等于 Params.Height。 */
	UPROPERTY(VisibleInstanceOnly, Category = "Bamboo", meta = (AllowPrivateAccess = "true"))
	float RemainingHeight = 500.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Bamboo", meta = (AllowPrivateAccess = "true"))
	bool bCut = false;

	/** 实际切口所在的平面。 */
	FBambooCutPlane CutPlane;
};
