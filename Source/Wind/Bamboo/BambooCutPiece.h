// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BambooTypes.h"
#include "GameFramework/Actor.h"
#include "BambooCutPiece.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * 被砍下来的竹子上半段。
 *
 * 一段独立的程序化网格，本地原点就是切口：生成后立即开始物理模拟，并按砍击速度在命中点上
 * 施加冲量（所以砍得越快飞得越远、转得越厉害），落地一段时间后自动销毁。
 * 材质槽与竹子本体一一对应，断口看起来和原来完全一致。
 */
UCLASS(meta = (DisplayName = "Bamboo Cut Piece"))
class ABambooCutPiece : public AActor
{
	GENERATED_BODY()

public:

	ABambooCutPiece();

	/** 从切口平面起生成掉落的整段，复用竹子本体的材质，并按冲击参数给物理初速度。 */
	void InitPiece(const FBambooStalkParams& InParams, const FBambooCutPlane& Plane, const FBambooCutImpact& Impact,
		const TArray<UMaterialInterface*>& SectionMaterials);

	/**
	 * 接管另一个程序化网格组件的分段几何（SliceProceduralMesh 切出来的另一半）。
	 * 顶点沿用源组件的本地空间，所以本 Actor 必须摆在源组件的变换上。
	 * 成功接管时返回 true，源组件会被销毁。
	 */
	bool InitPieceFromComponent(UProceduralMeshComponent* SourceMesh, const FBambooStalkParams& InParams,
		const FBambooCutPlane& Plane, const FBambooCutImpact& Impact);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bamboo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> PieceMesh;

	/** 倒下后存活时间，到点自动销毁，避免场景里堆满断竹。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bamboo", meta = (ClampMin = "1"))
	float LifeSpanSeconds = 8.0f;

	/** 砍击速度换算成断竹初速度的比例。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bamboo", meta = (ClampMin = "0"))
	float ImpactSpeedScale = 0.4f;

	/** 再慢的一刀也会让断竹歪出去一点（厘米/秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bamboo", meta = (ClampMin = "0"))
	float MinLaunchSpeed = 60.0f;

	/** 初速度上限（厘米/秒），防止极快的挥砍把断竹甩飞。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bamboo", meta = (ClampMin = "0"))
	float MaxLaunchSpeed = 1200.0f;

	/** 竹材密度（克/立方厘米），用来按真实壁体积算质量；凸包质量会把中空竹段当成实心。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bamboo", meta = (ClampMin = "0.01"))
	float DensityGramsPerCubicCm = 0.7f;

private:

	/** 建好碰撞、开物理、按体积设质量，最后施加冲量。 */
	void SetupPhysics(const FBambooStalkParams& InParams, const FBambooCutImpact& Impact, const FBambooCutPlane* Plane);

	/** 按冲击参数给刚体一个初速度；Plane 非空时把作用点投影到切面上。 */
	void ApplyImpulse(const FBambooCutImpact& Impact, const FBambooCutPlane* Plane);

	/** 网格上所有顶点（本地空间），用来生成凸包。 */
	TArray<FVector> CollectPiecePoints();
};
