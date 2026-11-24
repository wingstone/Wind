// Copyright Epic Games, Inc. All Rights Reserved.

#include "BambooStalk.h"

#include "BambooCutPiece.h"
#include "Engine/World.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Wind.h"

namespace
{
	/** 砍断后至少留下的树桩高度，避免在根部生成退化几何。 */
	constexpr float MinStumpHeight = 10.0f;

	/** 上半段短于这个长度就不再生成掉落 Actor，只留树桩。 */
	constexpr float MinPieceLength = 10.0f;

	/** 切面法线至少要有这么多"竖直"分量，否则两段分不开。 */
	constexpr float MinCutNormalZ = 0.35f;
}

ABambooStalk::ABambooStalk()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	StalkMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("StalkMesh"));
	SetRootComponent(StalkMesh);
	StalkMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StalkMesh->SetCollisionObjectType(ECC_WorldStatic);
	StalkMesh->SetCollisionResponseToAllChannels(ECR_Block);
	StalkMesh->bUseComplexAsSimpleCollision = true;
}

void ABambooStalk::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!bCut)
	{
		RemainingHeight = Params.Height;
		RebuildMesh(0.0f, Params.Height);
		return;
	}

	RebuildMesh(0.0f, Params.Height, FBambooGeometry::ECutSide::Top);
}

ABambooCutPiece* ABambooStalk::Cut(float CutHeight, FVector FallDirection, float ImpactSpeed)
{
	const FTransform& Transform = GetActorTransform();
	return CutAlongPlane(
		Transform.TransformPosition(FVector(0.0f, 0.0f, CutHeight)),
		Transform.TransformVectorNoScale(FVector::UpVector),
		FallDirection,
		ImpactSpeed);
}

ABambooCutPiece* ABambooStalk::CutAtLocation(FVector WorldLocation, FVector FallDirection, float ImpactSpeed)
{
	const FVector LocalLocation = GetActorTransform().InverseTransformPosition(WorldLocation);
	return Cut(static_cast<float>(LocalLocation.Z), FallDirection, ImpactSpeed);
}

ABambooCutPiece* ABambooStalk::CutAlongPlane(FVector WorldPoint, FVector WorldNormal, FVector FallDirection, float ImpactSpeed)
{
	if (bCut)
	{
		return nullptr;
	}

	if (!bUseCustomCutMesh)
	{
		return CutWithEngineSlice(WorldPoint, WorldNormal, FallDirection, ImpactSpeed);
	}

	const FTransform& Transform = GetActorTransform();

	FBambooCutPlane Plane;
	Plane.Point = Transform.InverseTransformPosition(WorldPoint);
	Plane.Normal = Transform.InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();
	if (Plane.Normal.IsNearlyZero())
	{
		Plane.Normal = FVector::UpVector;
	}
	if (Plane.Normal.Z < 0.0)
	{
		Plane.Normal = -Plane.Normal;
	}

	// 切面太竖（例如几乎平行于竹子轴）就把法线掰回接近竖直，否则分不出上下两段
	Plane.Normal.Z = FMath::Max(Plane.Normal.Z, MinCutNormalZ);
	Plane.Normal.Normalize();

	// 切口必须落在竹子内部：太低抬一点，太高压一点
	float MinHeight = 0.0f;
	float MaxHeight = 0.0f;
	FBambooGeometry::GetCutHeightRange(Params, Plane, MinHeight, MaxHeight);

	if (MinHeight < MinStumpHeight)
	{
		Plane.Point.Z += MinStumpHeight - MinHeight;
		FBambooGeometry::GetCutHeightRange(Params, Plane, MinHeight, MaxHeight);
	}

	if (MaxHeight > Params.Height - MinPieceLength)
	{
		Plane.Point.Z -= MaxHeight - (Params.Height - MinPieceLength);
		FBambooGeometry::GetCutHeightRange(Params, Plane, MinHeight, MaxHeight);
	}

	if (MinHeight < 0.0f || MaxHeight > Params.Height)
	{
		// 倾斜得太夸张，退回水平切，保证一定能断开
		const float AxisHeight = FMath::Clamp(Plane.AxisHeight(), MinStumpHeight, Params.Height - MinPieceLength);
		Plane.Normal = FVector::UpVector;
		Plane.Point = FVector(0.0f, 0.0f, AxisHeight);
	}

	CutPlane = Plane;
	bCut = true;
	RemainingHeight = Plane.AxisHeight();
	RebuildMesh(0.0f, Params.Height, FBambooGeometry::ECutSide::Top);

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 上半段沿用原竹子的变换，只是整体抬到切口高度；切口本身就是它的网格原点
	const FTransform PieceTransform = Transform * FTransform(FVector(0.0f, 0.0f, RemainingHeight));

	ABambooCutPiece* Piece = World->SpawnActor<ABambooCutPiece>(
		ABambooCutPiece::StaticClass(), PieceTransform, SpawnParameters);
	if (!Piece)
	{
		UE_LOG(LogWind, Warning, TEXT("Bamboo '%s' failed to spawn its cut piece."), *GetName());
		return nullptr;
	}

	// 断竹复用本体每个材质槽的材质，断口看起来才和原来一致
	TArray<UMaterialInterface*> SectionMaterials;
	SectionMaterials.Reserve(FBambooGeometry::SectionCount);
	for (int32 SectionIndex = 0; SectionIndex < FBambooGeometry::SectionCount; ++SectionIndex)
	{
		SectionMaterials.Add(StalkMesh ? StalkMesh->GetMaterial(SectionIndex) : nullptr);
	}

	FBambooCutImpact Impact;
	Impact.Point = WorldPoint;
	Impact.Direction = FallDirection;
	Impact.Speed = FMath::Max(ImpactSpeed, 0.0f);

	Piece->InitPiece(Params, Plane, Impact, SectionMaterials);
	return Piece;
}

void ABambooStalk::RebuildMesh(float BottomHeight, float TopHeight, FBambooGeometry::ECutSide CutSide)
{
	if (!StalkMesh)
	{
		return;
	}

	const FBambooCutPlane* Plane = CutSide == FBambooGeometry::ECutSide::None ? nullptr : &CutPlane;

	TArray<FBambooMeshSection> Sections;
	FBambooGeometry::BuildSection(Params, BottomHeight, TopHeight, Sections, 0.0f, CutSide, Plane);
	FBambooGeometry::ApplyToComponent(StalkMesh, Sections);
}

ABambooCutPiece* ABambooStalk::CutWithEngineSlice(FVector WorldPoint, FVector WorldNormal, FVector FallDirection, float ImpactSpeed)
{
	UWorld* World = GetWorld();
	if (!World || !StalkMesh)
	{
		return nullptr;
	}

	const FTransform& Transform = GetActorTransform();

	// 与自定义切面共用同一套平面整理规则，两种模式的手感才一致
	FVector Normal = Transform.InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}
	if (Normal.Z < 0.0)
	{
		Normal = -Normal;
	}
	Normal.Z = FMath::Max(Normal.Z, MinCutNormalZ);
	Normal.Normalize();

	FBambooCutPlane Plane;
	Plane.Point = Transform.InverseTransformPosition(WorldPoint);
	Plane.Normal = Normal;

	// SliceProceduralMesh 把法线正侧的几何留在原组件里，所以传反向法线：竹子留下下半段
	const FVector SliceNormal = Transform.TransformVectorNoScale(-Normal).GetSafeNormal();

	UProceduralMeshComponent* OtherHalf = nullptr;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		StalkMesh,
		WorldPoint,
		SliceNormal,
		true,
		OtherHalf,
		EProcMeshSliceCapOption::CreateNewSectionForCap,
		StalkMesh->GetMaterial(FBambooGeometry::SectionCut));

	if (!OtherHalf)
	{
		return nullptr;
	}

	bCut = true;
	CutPlane = Plane;
	RemainingHeight = Plane.AxisHeight();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 切片出来的顶点仍然是竹子本地的坐标，所以掉落段的原点直接取竹子的变换
	ABambooCutPiece* Piece = World->SpawnActor<ABambooCutPiece>(
		ABambooCutPiece::StaticClass(), Transform, SpawnParameters);
	if (!Piece)
	{
		OtherHalf->DestroyComponent();
		UE_LOG(LogWind, Warning, TEXT("Bamboo '%s' failed to spawn its cut piece."), *GetName());
		return nullptr;
	}

	FBambooCutImpact Impact;
	Impact.Point = WorldPoint;
	Impact.Direction = FallDirection;
	Impact.Speed = FMath::Max(ImpactSpeed, 0.0f);

	if (!Piece->InitPieceFromComponent(OtherHalf, Params, Plane, Impact))
	{
		Piece->Destroy();
		return nullptr;
	}

	return Piece;
}
