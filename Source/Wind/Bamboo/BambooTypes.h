// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BambooTypes.generated.h"

class UProceduralMeshComponent;

/**
 * 一根竹子的外形参数。高度与半径都使用 Actor 本地空间，单位厘米，Z 轴向上。
 *
 * 比例参考常见的 5 m 竹材：高 500、直径 10、壁厚 6 mm、8 个节。
 */
USTRUCT(BlueprintType)
struct FBambooStalkParams
{
	GENERATED_BODY()

	/** 整根竹子的高度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "50"))
	float Height = 500.0f;

	/** 根部半径（直径的一半）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "1"))
	float BaseRadius = 5.0f;

	/** 壁厚。竹子是中空的，内壁半径 = 外壁半径 - 壁厚。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "0"))
	float WallThickness = 0.6f;

	/** 整根竹子上的节数（不是节间的数量）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "0", ClampMax = "32"))
	int32 NodeCount = 8;

	/** 横截面边数，越大越圆。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "3", ClampMax = "64"))
	int32 SideCount = 48;

	/** 每个节间的纵向采样数，决定竹杆本身的光滑度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "1", ClampMax = "32"))
	int32 StepsPerBay = 10;

	/** 从根到梢的收缩比例：梢部半径 = 根部半径 × (1 - TaperRatio)。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "0", ClampMax = "0.9"))
	float TaperRatio = 0.1f;

	/** 节环的隆起高度（半径放大比例）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "0"))
	float CollarStrength = 0.075f;

	/** 节环主隆起的宽度，越大节看起来越"缓"。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo", meta = (ClampMin = "0.1"))
	float CollarWidth = 1.8f;

	/** 竹青（外壁）颜色，写进顶点色。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo")
	FLinearColor OuterColor = FLinearColor(0.105f, 0.235f, 0.075f);

	/** 内壁颜色，写进顶点色。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo")
	FLinearColor InnerColor = FLinearColor(0.48f, 0.36f, 0.19f);

	/** 切口截面（上下环带与节内隔膜）颜色，写进顶点色。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bamboo")
	FLinearColor CutColor = FLinearColor(0.66f, 0.53f, 0.31f);
};

/** 斜切口所在的平面（Actor 本地空间）。Normal 指向被切掉的那一半。 */
struct FBambooCutPlane
{
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;

	/** 法线必须大致朝上，切口才能把竹子分成上下两段。 */
	bool IsValid() const
	{
		return Normal.Z > KINDA_SMALL_NUMBER && Normal.SizeSquared() > KINDA_SMALL_NUMBER;
	}

	/** 平面上某个水平位置的高度。 */
	float HeightAt(float X, float Y) const
	{
		return static_cast<float>(Point.Z)
			- (static_cast<float>(Normal.X) * (X - static_cast<float>(Point.X))
				+ static_cast<float>(Normal.Y) * (Y - static_cast<float>(Point.Y))) / static_cast<float>(Normal.Z);
	}

	/** 平面在竹子轴心（本地 X = Y = 0）处的高度。 */
	float AxisHeight() const { return HeightAt(0.0f, 0.0f); }
};

/** 一刀砍下去带给断竹的冲击参数（世界空间）。 */
struct FBambooCutImpact
{
	/** 命中点：冲量作用在这里，断竹自然会翻滚。 */
	FVector Point = FVector::ZeroVector;

	/** 倒下方向，通常取刀身运动方向的水平分量。 */
	FVector Direction = FVector::ForwardVector;

	/** 砍击速度（厘米/秒），决定断竹被甩出去多快。 */
	float Speed = 0.0f;
};

/** 一段网格。竹子的每个材质槽对应一段：0 = 竹青外壁，1 = 内壁，2 = 切口截面。 */struct FBambooMeshSection
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FVector> TangentX;
	FLinearColor Color = FLinearColor::White;
};

/**
 * 竹筒的几何生成，纯数据、不依赖组件，方便单独测试。
 *
 * 造型参考 Blender 的 "Cuttable Bamboo Generator"：中空管壁（外壁 + 内壁 + 上下环状切口）、
 * 节环处的高斯隆起、节内隔膜、轻微的自然收缩与不规则。
 *
 * 切口可以是斜的：所有形状只由绝对高度决定，斜切面按平面与每根母线求交，所以树桩与掉落的
 * 那一段在切口处严丝合缝地共享同一条边界。
 */
struct FBambooGeometry
{
	/** 三个材质槽在 FBambooMeshSection 数组中的下标。 */
	static constexpr int32 SectionOuterWall = 0;
	static constexpr int32 SectionInnerWall = 1;
	static constexpr int32 SectionCut = 2;
	static constexpr int32 SectionCount = 3;

	/** 倾斜的那一端。 */
	enum class ECutSide : uint8
	{
		/** 两端都是水平截面。 */
		None,
		/** 顶端是斜面：只保留平面下方，也就是树桩。 */
		Top,
		/** 底端是斜面：只保留平面上方，也就是倒下的那一段。 */
		Bottom,
	};

	/** 节环在指定高度上带来的半径放大比例。 */
	static float NodeCollar(const FBambooStalkParams& Params, float Height);

	/** 指定高度处的**外壁**半径，含收缩、自然不规则与节环隆起。 */
	static float RadiusAtHeight(const FBambooStalkParams& Params, float Height);

	/** 斜切面在这根竹子上的最低 / 最高切口高度（含内外壁）。 */
	static void GetCutHeightRange(const FBambooStalkParams& Params, const FBambooCutPlane& Plane,
		float& OutMinHeight, float& OutMaxHeight);

	/**
	 * 生成 [BottomHeight, TopHeight] 区间内的一段中空竹筒。
	 * CutSide/CutPlane 非空时，对应那一端改成沿平面切出的斜口。
	 * 顶点位于 Actor 本地空间，Z = 绝对高度 - MeshOriginHeight；区间退化时每一段都是空的。
	 */
	static void BuildSection(
		const FBambooStalkParams& Params,
		float BottomHeight,
		float TopHeight,
		TArray<FBambooMeshSection>& OutSections,
		float MeshOriginHeight = 0.0f,
		ECutSide CutSide = ECutSide::None,
		const FBambooCutPlane* CutPlane = nullptr);

	/** 把生成好的分段写进组件，每段一个材质槽。 */
	static void ApplyToComponent(UProceduralMeshComponent* Component, const TArray<FBambooMeshSection>& Sections);
};
