// Copyright Epic Games, Inc. All Rights Reserved.

#include "BambooCutPiece.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
	/** 沿高度把顶点云切成的段数：每段一个凸包，既贴合竹筒又不会太贵。 */
	constexpr int32 ConvexBandCount = 8;

	/** 把点云按高度分成若干段，每段交给物理引擎求凸包（凸包是动态刚体唯一支持的简单碰撞）。 */
	TArray<TArray<FVector>> BuildConvexHulls(const TArray<FVector>& Points, int32 BandCount)
	{
		TArray<TArray<FVector>> Hulls;
		if (Points.Num() < 4)
		{
			return Hulls;
		}

		FBox Bounds(ForceInit);
		for (const FVector& Point : Points)
		{
			Bounds += Point;
		}

		const float Height = static_cast<float>(Bounds.Max.Z - Bounds.Min.Z);
		if (Height <= KINDA_SMALL_NUMBER)
		{
			return Hulls;
		}

		const int32 Bands = FMath::Clamp(BandCount, 1, 32);
		Hulls.SetNum(Bands);
		for (const FVector& Point : Points)
		{
			const float Alpha = static_cast<float>(Point.Z - Bounds.Min.Z) / Height;
			const int32 Band = FMath::Clamp(FMath::FloorToInt(Alpha * Bands), 0, Bands - 1);
			Hulls[Band].Add(Point);
		}

		Hulls.RemoveAll([](const TArray<FVector>& Hull) { return Hull.Num() < 4; });
		return Hulls;
	}

	/** 中空管壁的体积（立方厘米）：沿高度积分 π(R² - r²)，够准且不受内部隔膜影响。 */
	float ComputeWallVolume(const FBambooStalkParams& Params, float BottomHeight, float TopHeight)
	{
		const float Length = TopHeight - BottomHeight;
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		constexpr int32 Steps = 64;
		float Volume = 0.0f;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float Height = FMath::Lerp(BottomHeight, TopHeight, (static_cast<float>(Step) + 0.5f) / static_cast<float>(Steps));
			const float OuterRadius = FBambooGeometry::RadiusAtHeight(Params, Height);
			const float InnerRadius = FMath::Max(OuterRadius - Params.WallThickness, 0.0f);
			Volume += UE_PI * (OuterRadius * OuterRadius - InnerRadius * InnerRadius) * (Length / static_cast<float>(Steps));
		}

		return Volume;
	}
}

ABambooCutPiece::ABambooCutPiece()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	PieceMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PieceMesh"));
	SetRootComponent(PieceMesh);
	PieceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PieceMesh->SetCollisionObjectType(ECC_PhysicsBody);
	PieceMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void ABambooCutPiece::InitPiece(const FBambooStalkParams& InParams, const FBambooCutPlane& Plane, const FBambooCutImpact& Impact,
	const TArray<UMaterialInterface*>& SectionMaterials)
{
	// 本地原点取切口在轴心处的高度
	const float OriginHeight = Plane.AxisHeight();
	if (!ensureMsgf(OriginHeight >= 0.0f && OriginHeight < InParams.Height, TEXT("Bamboo cut piece needs a cut below the tip.")))
	{
		return;
	}

	TArray<FBambooMeshSection> Sections;
	FBambooGeometry::BuildSection(InParams, 0.0f, InParams.Height, Sections, OriginHeight,
		FBambooGeometry::ECutSide::Bottom, &Plane);
	if (Sections[FBambooGeometry::SectionOuterWall].Vertices.Num() == 0)
	{
		return;
	}

	FBambooGeometry::ApplyToComponent(PieceMesh, Sections);

	// 复用竹子本体的材质，断口与竹身才和原来一致
	for (int32 SectionIndex = 0; SectionIndex < SectionMaterials.Num(); ++SectionIndex)
	{
		if (UMaterialInterface* Material = SectionMaterials[SectionIndex])
		{
			PieceMesh->SetMaterial(SectionIndex, Material);
		}
	}

	SetupPhysics(InParams, Impact, &Plane);
	SetLifeSpan(LifeSpanSeconds);
}

bool ABambooCutPiece::InitPieceFromComponent(UProceduralMeshComponent* SourceMesh, const FBambooStalkParams& InParams,
	const FBambooCutPlane& Plane, const FBambooCutImpact& Impact)
{
	if (!IsValid(SourceMesh))
	{
		return false;
	}

	int32 CopiedSections = 0;
	for (int32 SectionIndex = 0; SectionIndex < SourceMesh->GetNumSections(); ++SectionIndex)
	{
		const FProcMeshSection* Section = SourceMesh->GetProcMeshSection(SectionIndex);
		if (!Section || Section->ProcVertexBuffer.Num() == 0 || Section->ProcIndexBuffer.Num() == 0)
		{
			continue;
		}

		PieceMesh->SetProcMeshSection(SectionIndex, *Section);
		PieceMesh->SetMaterial(SectionIndex, SourceMesh->GetMaterial(SectionIndex));
		++CopiedSections;
	}

	// 几何已经搬进本体网格，临时组件就不需要了
	SourceMesh->DestroyComponent();

	if (CopiedSections == 0)
	{
		return false;
	}

	SetupPhysics(InParams, Impact, &Plane);
	SetLifeSpan(LifeSpanSeconds);
	return true;
}

void ABambooCutPiece::SetupPhysics(const FBambooStalkParams& InParams, const FBambooCutImpact& Impact, const FBambooCutPlane* Plane)
{
	// 程序化网格默认是复杂碰撞（trimesh）：引擎不会把 ComplexAsSimple 的刚体当动态物体模拟，
	// SetSimulatePhysics 只会打一条警告然后什么也不做。所以换成凸包简单碰撞。
	PieceMesh->bUseComplexAsSimpleCollision = false;
	PieceMesh->SetCollisionConvexMeshes(BuildConvexHulls(CollectPiecePoints(), ConvexBandCount));

	PieceMesh->SetSimulatePhysics(true);

	// 凸包是实心的，引擎按凸包算质量会把中空竹段当成实心柱，这里用真实壁体积覆盖
	const float CutHeight = Plane ? FMath::Clamp(Plane->AxisHeight(), 0.0f, InParams.Height) : 0.0f;
	const float VolumeCm3 = ComputeWallVolume(InParams, CutHeight, InParams.Height);
	const float MassKg = FMath::Max(VolumeCm3 * DensityGramsPerCubicCm / 1000.0f, 0.1f);
	PieceMesh->SetMassOverrideInKg(NAME_None, MassKg, true);

	ApplyImpulse(Impact, Plane);
}

TArray<FVector> ABambooCutPiece::CollectPiecePoints()
{
	TArray<FVector> Points;
	if (!PieceMesh)
	{
		return Points;
	}

	for (int32 SectionIndex = 0; SectionIndex < PieceMesh->GetNumSections(); ++SectionIndex)
	{
		const FProcMeshSection* Section = PieceMesh->GetProcMeshSection(SectionIndex);
		if (!Section)
		{
			continue;
		}

		Points.Reserve(Points.Num() + Section->ProcVertexBuffer.Num());
		for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
		{
			Points.Add(Vertex.Position);
		}
	}

	return Points;
}

void ABambooCutPiece::ApplyImpulse(const FBambooCutImpact& Impact, const FBambooCutPlane* Plane)
{
	// 冲击力：按剑速换算初速度，冲量作用在切口处的命中点上，偏离质心自然带出翻滚
	const FVector Direction = Impact.Direction.GetSafeNormal();
	const float LaunchSpeed = FMath::Clamp(Impact.Speed * ImpactSpeedScale, MinLaunchSpeed, MaxLaunchSpeed);
	if (Direction.IsNearlyZero() || LaunchSpeed <= 0.0f)
	{
		return;
	}

	const FTransform& PieceTransform = GetActorTransform();

	// 命中点可能略微偏离切面，先投影到切面上，避免力臂长得离谱
	FVector LocalImpulsePoint = PieceTransform.InverseTransformPosition(Impact.Point);
	if (Plane)
	{
		const FVector PlaneNormal = Plane->Normal.GetSafeNormal();
		LocalImpulsePoint -= PlaneNormal * FVector::DotProduct(LocalImpulsePoint - Plane->Point, PlaneNormal);
	}
	const FVector ImpulsePoint = PieceTransform.TransformPosition(LocalImpulsePoint);

	const float Mass = FMath::Max(PieceMesh->GetMass(), 1.0f);
	PieceMesh->AddImpulseAtLocation(Direction * Mass * LaunchSpeed, ImpulsePoint);
}
