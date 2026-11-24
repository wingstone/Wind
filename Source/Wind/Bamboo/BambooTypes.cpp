// Copyright Epic Games, Inc. All Rights Reserved.

#include "BambooTypes.h"

#include "ProceduralMeshComponent.h"

namespace
{
	constexpr float CentimetresPerMetre = 100.0f;

	// 以下常数取自 Blender 参考脚本，长度单位已换算到厘米
	constexpr float SecondaryCollarStrength = 0.018f;
	constexpr float SecondaryCollarWidth = 3.8f;
	constexpr float OrganicPrimary = 0.012f;
	constexpr float OrganicSecondary = 0.006f;
	constexpr float WobblePrimary = 0.003f;
	constexpr float WobbleSecondary = 0.002f;
	constexpr float MinInnerRadius = 0.6f;

	/** 节环附近的加密采样偏移（相对节点高度）。 */
	constexpr float CollarSampleOffsets[] = { -5.5f, -3.5f, -1.8f, -0.8f, 0.0f, 0.8f, 1.8f, 3.5f, 5.5f };

	/** 用两侧差分求半径的纵向斜率，让法线贴合节环的隆起。 */
	constexpr float SlopeSampleStep = 0.05f;

	/** 采样去重的最小间距。 */
	constexpr float MinRingSpacing = 0.01f;

	float ToMetres(float Centimetres)
	{
		return Centimetres / CentimetresPerMetre;
	}

	/** 让机器做出的圆柱不那么完美：只由角度与绝对高度决定，砍断后依然接得上。 */
	float AngularWobble(float Angle, float Height)
	{
		const float HeightMetres = ToMetres(Height);
		return 1.0f
			+ WobblePrimary * FMath::Sin(3.0f * Angle + HeightMetres * 1.4f)
			+ WobbleSecondary * FMath::Sin(7.0f * Angle - HeightMetres * 0.8f);
	}

	float RadiusSlope(const FBambooStalkParams& Params, float Height)
	{
		const float Lower = FBambooGeometry::RadiusAtHeight(Params, Height - SlopeSampleStep);
		const float Upper = FBambooGeometry::RadiusAtHeight(Params, Height + SlopeSampleStep);
		return (Upper - Lower) / (2.0f * SlopeSampleStep);
	}

	float InnerRadiusAtHeight(const FBambooStalkParams& Params, float Height, float WallThickness)
	{
		return FMath::Max(MinInnerRadius, FBambooGeometry::RadiusAtHeight(Params, Height) - WallThickness);
	}

	/**
	 * 纵向采样高度，与参考脚本一致：整根竹子等距采样，再在节环附近加密。
	 * 采样只取决于绝对高度，所以相邻两段在切口处的边界能对上。
	 */
	void BuildProfileHeights(const FBambooStalkParams& Params, float BottomHeight, float TopHeight, TArray<float>& OutHeights)
	{
		OutHeights.Reset();
		OutHeights.Add(BottomHeight);
		OutHeights.Add(TopHeight);

		const int32 Bays = FMath::Max(Params.NodeCount, 1);
		const int32 StepsPerBay = FMath::Max(Params.StepsPerBay, 1);
		const int32 StepCount = Bays * StepsPerBay;

		for (int32 Step = 1; Step < StepCount; ++Step)
		{
			const float Height = Params.Height * static_cast<float>(Step) / static_cast<float>(StepCount);
			if (Height > BottomHeight && Height < TopHeight)
			{
				OutHeights.Add(Height);
			}
		}

		const float BayHeight = Params.Height / static_cast<float>(Bays);
		for (int32 NodeIndex = 1; NodeIndex < Bays; ++NodeIndex)
		{
			const float NodeHeight = BayHeight * static_cast<float>(NodeIndex);
			for (const float Offset : CollarSampleOffsets)
			{
				const float Height = NodeHeight + Offset;
				if (Height > BottomHeight && Height < TopHeight)
				{
					OutHeights.Add(Height);
				}
			}
		}

		OutHeights.Sort();

		// 去掉几乎重合的采样，避免出现退化三角形
		for (int32 Index = OutHeights.Num() - 1; Index > 0; --Index)
		{
			if (OutHeights[Index] - OutHeights[Index - 1] < MinRingSpacing)
			{
				OutHeights.RemoveAt(Index);
			}
		}
	}

	/**
	 * 斜切面与每根母线的交点高度。半径随高度变化，所以迭代几次求不动点。
	 * 外壁带角度抖动，内壁不带——与侧壁的生成方式保持一致。
	 */
	void ComputeCutHeights(const FBambooStalkParams& Params, const FBambooCutPlane& Plane, int32 Sides, float WallThickness,
		TArray<float>& OutOuterHeights, TArray<float>& OutInnerHeights)
	{
		OutOuterHeights.SetNum(Sides);
		OutInnerHeights.SetNum(Sides);

		const float AxisHeight = Plane.AxisHeight();
		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(SideIndex) / static_cast<float>(Sides);
			const float CosAngle = FMath::Cos(Angle);
			const float SinAngle = FMath::Sin(Angle);

			float OuterHeight = AxisHeight;
			float InnerHeight = AxisHeight;
			for (int32 Iteration = 0; Iteration < 3; ++Iteration)
			{
				const float OuterRadius = FBambooGeometry::RadiusAtHeight(Params, OuterHeight) * AngularWobble(Angle, OuterHeight);
				OuterHeight = Plane.HeightAt(OuterRadius * CosAngle, OuterRadius * SinAngle);

				const float InnerRadius = InnerRadiusAtHeight(Params, InnerHeight, WallThickness);
				InnerHeight = Plane.HeightAt(InnerRadius * CosAngle, InnerRadius * SinAngle);
			}

			OutOuterHeights[SideIndex] = OuterHeight;
			OutInnerHeights[SideIndex] = InnerHeight;
		}
	}

	int32 PushVertex(FBambooMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector& Tangent, const FVector2D& UV)
	{
		Section.Vertices.Add(Position);
		Section.Normals.Add(Normal);
		Section.TangentX.Add(Tangent);
		Section.UVs.Add(UV);
		return Section.Vertices.Num() - 1;
	}

	/** 按 UE 的正面约定（cross(V1-V0, V2-V0) = -Normal）展开一个四边形。 */
	void PushQuad(FBambooMeshSection& Section, int32 V0, int32 V1, int32 V2, int32 V3)
	{
		Section.Triangles.Add(V0);
		Section.Triangles.Add(V1);
		Section.Triangles.Add(V3);

		Section.Triangles.Add(V1);
		Section.Triangles.Add(V2);
		Section.Triangles.Add(V3);
	}
}

float FBambooGeometry::NodeCollar(const FBambooStalkParams& Params, float Height)
{
	const int32 Bays = FMath::Max(Params.NodeCount, 0);
	if (Bays <= 1 || Params.Height <= KINDA_SMALL_NUMBER || Params.CollarStrength <= 0.0f)
	{
		return 0.0f;
	}

	const float BayHeight = Params.Height / static_cast<float>(Bays);
	const float HeightMetres = ToMetres(Height);
	const float PrimaryWidth = FMath::Max(Params.CollarWidth, 0.1f) / CentimetresPerMetre;

	float Collar = 0.0f;
	// 只有内部节点才有节环：根部与顶梢是竹子的两端
	for (int32 NodeIndex = 1; NodeIndex < Bays; ++NodeIndex)
	{
		const float Distance = HeightMetres - ToMetres(BayHeight * static_cast<float>(NodeIndex));
		Collar += Params.CollarStrength * FMath::Exp(-FMath::Square(Distance / PrimaryWidth));
		Collar += SecondaryCollarStrength * FMath::Exp(-FMath::Square(Distance / (SecondaryCollarWidth / CentimetresPerMetre)));
	}

	return Collar;
}

float FBambooGeometry::RadiusAtHeight(const FBambooStalkParams& Params, float Height)
{
	if (Params.Height <= KINDA_SMALL_NUMBER)
	{
		return Params.BaseRadius;
	}

	const float Alpha = FMath::Clamp(Height / Params.Height, 0.0f, 1.0f);
	const float HeightMetres = ToMetres(Height);

	const float Taper = 1.0f - Params.TaperRatio * Alpha;
	const float Organic = 1.0f
		+ OrganicPrimary * FMath::Sin(HeightMetres * 2.7f)
		+ OrganicSecondary * FMath::Sin(HeightMetres * 7.1f);

	return Params.BaseRadius * Taper * Organic * (1.0f + NodeCollar(Params, Height));
}

void FBambooGeometry::GetCutHeightRange(const FBambooStalkParams& Params, const FBambooCutPlane& Plane,
	float& OutMinHeight, float& OutMaxHeight)
{
	const int32 Sides = FMath::Max(Params.SideCount, 3);
	TArray<float> OuterHeights;
	TArray<float> InnerHeights;
	ComputeCutHeights(Params, Plane, Sides, FMath::Max(Params.WallThickness, 0.0f), OuterHeights, InnerHeights);

	OutMinHeight = Plane.AxisHeight();
	OutMaxHeight = OutMinHeight;
	for (const float Height : OuterHeights)
	{
		OutMinHeight = FMath::Min(OutMinHeight, Height);
		OutMaxHeight = FMath::Max(OutMaxHeight, Height);
	}
	for (const float Height : InnerHeights)
	{
		OutMinHeight = FMath::Min(OutMinHeight, Height);
		OutMaxHeight = FMath::Max(OutMaxHeight, Height);
	}
}

void FBambooGeometry::BuildSection(const FBambooStalkParams& Params, float BottomHeight, float TopHeight,
	TArray<FBambooMeshSection>& OutSections, float MeshOriginHeight, ECutSide CutSide, const FBambooCutPlane* CutPlane)
{
	OutSections.Reset();
	OutSections.SetNum(SectionCount);
	OutSections[SectionOuterWall].Color = Params.OuterColor;
	OutSections[SectionInnerWall].Color = Params.InnerColor;
	OutSections[SectionCut].Color = Params.CutColor;

	if (TopHeight - BottomHeight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const int32 Sides = FMath::Max(Params.SideCount, 3);
	const float WallThickness = FMath::Max(Params.WallThickness, 0.0f);

	const bool bSlanted = CutSide != ECutSide::None && CutPlane != nullptr && CutPlane->IsValid();
	TArray<float> CutOuterHeights;
	TArray<float> CutInnerHeights;
	float MinCutHeight = TopHeight;
	float MaxCutHeight = BottomHeight;
	if (bSlanted)
	{
		ComputeCutHeights(Params, *CutPlane, Sides, WallThickness, CutOuterHeights, CutInnerHeights);

		for (const float Height : CutOuterHeights)
		{
			MinCutHeight = FMath::Min(MinCutHeight, Height);
			MaxCutHeight = FMath::Max(MaxCutHeight, Height);
		}
		for (const float Height : CutInnerHeights)
		{
			MinCutHeight = FMath::Min(MinCutHeight, Height);
			MaxCutHeight = FMath::Max(MaxCutHeight, Height);
		}
	}

	// 固定环：斜切时只保留切口内侧的那些
	TArray<float> FixedHeights;
	BuildProfileHeights(Params, BottomHeight, TopHeight, FixedHeights);
	for (int32 Index = FixedHeights.Num() - 1; Index >= 0; --Index)
	{
		const float Height = FixedHeights[Index];
		const bool bKeep = !bSlanted
			|| (CutSide == ECutSide::Top ? Height < MinCutHeight - 0.05f : Height > MaxCutHeight + 0.05f);
		if (!bKeep)
		{
			FixedHeights.RemoveAt(Index);
		}
	}

	if (FixedHeights.Num() == 0)
	{
		if (!bSlanted)
		{
			return;
		}

		// 切口几乎贴着端点：补一条固定环，侧壁才有东西可连
		const float Fallback = CutSide == ECutSide::Top
			? FMath::Max(BottomHeight, MinCutHeight - 0.5f)
			: FMath::Min(TopHeight, MaxCutHeight + 0.5f);
		FixedHeights.Add(Fallback);
		FixedHeights.Sort();
	}

	const int32 CutRingIndex = bSlanted ? (CutSide == ECutSide::Top ? FixedHeights.Num() : 0) : INDEX_NONE;
	const int32 FixedOffset = (CutRingIndex == 0) ? 1 : 0;
	const int32 RingCount = FixedHeights.Num() + (bSlanted ? 1 : 0);

	auto IsCutRing = [&](int32 RingIndex)
	{
		return bSlanted && RingIndex == CutRingIndex;
	};

	/** 环上某条母线的高度：固定环处处相同，斜切环逐边不同（外壁与内壁也不一定相同）。 */
	auto RingHeight = [&](int32 RingIndex, int32 SideIndex, bool bOuter) -> float
	{
		if (IsCutRing(RingIndex))
		{
			return bOuter ? CutOuterHeights[SideIndex] : CutInnerHeights[SideIndex];
		}
		return FixedHeights[RingIndex - FixedOffset];
	};

	FBambooMeshSection& OuterSection = OutSections[SectionOuterWall];
	FBambooMeshSection& InnerSection = OutSections[SectionInnerWall];
	FBambooMeshSection& CutSection = OutSections[SectionCut];

	// 外壁：竹青。带一点点角度抖动，避免机械感
	for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
	{
		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(SideIndex) / static_cast<float>(Sides);
			const float CosAngle = FMath::Cos(Angle);
			const float SinAngle = FMath::Sin(Angle);
			const float Height = RingHeight(RingIndex, SideIndex, true);
			const float Radius = RadiusAtHeight(Params, Height) * AngularWobble(Angle, Height);
			const float Slope = RadiusSlope(Params, Height);

			PushVertex(OuterSection,
				FVector(CosAngle * Radius, SinAngle * Radius, Height - MeshOriginHeight),
				FVector(CosAngle, SinAngle, -Slope).GetSafeNormal(),
				FVector(-SinAngle, CosAngle, 0.0f),
				FVector2D(static_cast<float>(SideIndex) / static_cast<float>(Sides), ToMetres(Height)));
		}
	}

	for (int32 RingIndex = 0; RingIndex + 1 < RingCount; ++RingIndex)
	{
		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const int32 NextSide = (SideIndex + 1) % Sides;
			PushQuad(OuterSection,
				RingIndex * Sides + SideIndex,
				(RingIndex + 1) * Sides + SideIndex,
				(RingIndex + 1) * Sides + NextSide,
				RingIndex * Sides + NextSide);
		}
	}

	// 内壁：竹黄，法线朝内
	for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
	{
		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(SideIndex) / static_cast<float>(Sides);
			const float CosAngle = FMath::Cos(Angle);
			const float SinAngle = FMath::Sin(Angle);
			const float Height = RingHeight(RingIndex, SideIndex, false);
			const float Radius = InnerRadiusAtHeight(Params, Height, WallThickness);
			const float Slope = RadiusSlope(Params, Height);

			PushVertex(InnerSection,
				FVector(CosAngle * Radius, SinAngle * Radius, Height - MeshOriginHeight),
				FVector(-CosAngle, -SinAngle, Slope).GetSafeNormal(),
				FVector(-SinAngle, CosAngle, 0.0f),
				FVector2D(static_cast<float>(SideIndex) / static_cast<float>(Sides), ToMetres(Height)));
		}
	}

	for (int32 RingIndex = 0; RingIndex + 1 < RingCount; ++RingIndex)
	{
		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const int32 NextSide = (SideIndex + 1) % Sides;
			PushQuad(InnerSection,
				RingIndex * Sides + SideIndex,
				RingIndex * Sides + NextSide,
				(RingIndex + 1) * Sides + NextSide,
				(RingIndex + 1) * Sides + SideIndex);
		}
	}

	// 上下环形切口：露出壁厚，而不是把竹筒封成实心；斜切时法线跟着切面走
	auto AddAnnulus = [&](int32 RingIndex, bool bTop)
	{
		FVector Normal(0.0f, 0.0f, bTop ? 1.0f : -1.0f);
		if (IsCutRing(RingIndex) && CutPlane)
		{
			const FVector PlaneNormal = CutPlane->Normal.GetSafeNormal();
			Normal = bTop ? PlaneNormal : -PlaneNormal;
		}

		const int32 BaseIndex = CutSection.Vertices.Num();

		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(SideIndex) / static_cast<float>(Sides);
			const float CosAngle = FMath::Cos(Angle);
			const float SinAngle = FMath::Sin(Angle);
			const FVector Tangent(-SinAngle, CosAngle, 0.0f);
			const float U = static_cast<float>(SideIndex) / static_cast<float>(Sides);

			const float OuterHeight = RingHeight(RingIndex, SideIndex, true);
			const float OuterRadius = RadiusAtHeight(Params, OuterHeight) * AngularWobble(Angle, OuterHeight);
			const float InnerHeight = RingHeight(RingIndex, SideIndex, false);
			const float InnerRadius = InnerRadiusAtHeight(Params, InnerHeight, WallThickness);

			PushVertex(CutSection, FVector(CosAngle * OuterRadius, SinAngle * OuterRadius, OuterHeight - MeshOriginHeight), Normal, Tangent, FVector2D(U, 1.0f));
			PushVertex(CutSection, FVector(CosAngle * InnerRadius, SinAngle * InnerRadius, InnerHeight - MeshOriginHeight), Normal, Tangent, FVector2D(U, 0.0f));
		}

		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const int32 NextSide = (SideIndex + 1) % Sides;
			const int32 OuterCurrent = BaseIndex + SideIndex * 2;
			const int32 InnerCurrent = OuterCurrent + 1;
			const int32 OuterNext = BaseIndex + NextSide * 2;
			const int32 InnerNext = OuterNext + 1;

			if (bTop)
			{
				PushQuad(CutSection, OuterCurrent, InnerCurrent, InnerNext, OuterNext);
			}
			else
			{
				PushQuad(CutSection, OuterCurrent, OuterNext, InnerNext, InnerCurrent);
			}
		}
	};

	AddAnnulus(0, false);
	if (RingCount > 1)
	{
		AddAnnulus(RingCount - 1, true);
	}

	// 节内隔膜：贴在节高度的内壁环上的一块薄片
	const int32 Bays = FMath::Max(Params.NodeCount, 1);
	const float BayHeight = Params.Height / static_cast<float>(Bays);
	for (int32 NodeIndex = 1; NodeIndex < Bays; ++NodeIndex)
	{
		const float NodeHeight = BayHeight * static_cast<float>(NodeIndex);
		if (NodeHeight <= BottomHeight || NodeHeight >= TopHeight)
		{
			continue;
		}

		// 斜切面之外的那部分已经没了，不再生成隔膜
		if (bSlanted && (CutSide == ECutSide::Top ? NodeHeight >= MinCutHeight : NodeHeight <= MaxCutHeight))
		{
			continue;
		}

		int32 FixedIndex = INDEX_NONE;
		for (int32 Index = 0; Index < FixedHeights.Num(); ++Index)
		{
			if (FMath::IsNearlyEqual(FixedHeights[Index], NodeHeight, 0.01f))
			{
				FixedIndex = Index;
				break;
			}
		}

		if (FixedIndex == INDEX_NONE)
		{
			continue;
		}

		const FVector Normal(0.0f, 0.0f, 1.0f);
		const float LocalNodeHeight = NodeHeight - MeshOriginHeight;
		const int32 CenterIndex = PushVertex(CutSection, FVector(0.0f, 0.0f, LocalNodeHeight), Normal, FVector(1.0f, 0.0f, 0.0f), FVector2D(0.5f, 0.5f));
		const int32 RingBase = CutSection.Vertices.Num();

		const float Radius = InnerRadiusAtHeight(Params, NodeHeight, WallThickness);
		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(SideIndex) / static_cast<float>(Sides);
			const float CosAngle = FMath::Cos(Angle);
			const float SinAngle = FMath::Sin(Angle);

			PushVertex(CutSection,
				FVector(CosAngle * Radius, SinAngle * Radius, LocalNodeHeight),
				Normal,
				FVector(-SinAngle, CosAngle, 0.0f),
				FVector2D(0.5f + 0.5f * CosAngle, 0.5f + 0.5f * SinAngle));
		}

		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const int32 NextSide = (SideIndex + 1) % Sides;

			CutSection.Triangles.Add(CenterIndex);
			CutSection.Triangles.Add(RingBase + NextSide);
			CutSection.Triangles.Add(RingBase + SideIndex);
		}
	}
}

void FBambooGeometry::ApplyToComponent(UProceduralMeshComponent* Component, const TArray<FBambooMeshSection>& Sections)
{
	if (!Component)
	{
		return;
	}

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const FBambooMeshSection& Section = Sections[SectionIndex];
		if (Section.Vertices.Num() == 0 || Section.Triangles.Num() == 0)
		{
			Component->ClearMeshSection(SectionIndex);
			continue;
		}

		TArray<FProcMeshTangent> Tangents;
		Tangents.Reserve(Section.TangentX.Num());
		for (const FVector& Tangent : Section.TangentX)
		{
			Tangents.Emplace(Tangent, false);
		}

		TArray<FLinearColor> Colors;
		Colors.Init(Section.Color, Section.Vertices.Num());

		Component->CreateMeshSection_LinearColor(
			SectionIndex, Section.Vertices, Section.Triangles, Section.Normals, Section.UVs, Colors, Tangents, true);
	}
}
