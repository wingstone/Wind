// Copyright Epic Games, Inc. All Rights Reserved.

#include "BambooSword.h"

#include "BambooStalk.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 引擎自带 Cube 的边长（厘米），用来把刀身缩放到指定尺寸。 */
	constexpr float UnitCubeSize = 100.0f;
}

ABambooSword::ABambooSword()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Grip = CreateDefaultSubobject<USceneComponent>(TEXT("Grip"));
	SetRootComponent(Grip);

	BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
	BladeMesh->SetupAttachment(Grip);
	BladeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BladeMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BladeMesh->SetStaticMesh(CubeMesh.Object);
	}

	BladeBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BladeBox"));
	BladeBox->SetupAttachment(Grip);
	BladeBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BladeBox->SetGenerateOverlapEvents(false);
	BladeBox->ShapeColor = FColor(190, 70, 60);

	UpdateBladeShape();
}

void ABambooSword::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateBladeShape();
}

void ABambooSword::UpdateBladeShape()
{
	const FVector HalfExtent(BladeLength * 0.5f, BladeWidth * 0.5f, BladeThickness * 0.5f);

	BladeBox->SetBoxExtent(HalfExtent);
	BladeBox->SetRelativeLocation(FVector(HalfExtent.X, 0.0f, 0.0f));

	BladeMesh->SetRelativeScale3D(FVector(BladeLength / UnitCubeSize, BladeWidth / UnitCubeSize, BladeThickness / UnitCubeSize));
	BladeMesh->SetRelativeLocation(FVector(HalfExtent.X, 0.0f, 0.0f));
}

void ABambooSword::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateSwing(DeltaSeconds);
	SweepBlade(DeltaSeconds);
}

void ABambooSword::Swing()
{
	if (IsSwinging())
	{
		return;
	}

	SwingRestRotation = GetRootComponent() ? GetRootComponent()->GetRelativeRotation() : GetActorRotation();
	SwingElapsed = 0.0f;
}

void ABambooSword::UpdateSwing(float DeltaSeconds)
{
	if (SwingElapsed < 0.0f)
	{
		return;
	}

	SwingElapsed += DeltaSeconds;
	const float Alpha = SwingElapsed / FMath::Max(SwingDuration, KINDA_SMALL_NUMBER);
	if (Alpha >= 1.0f)
	{
		SetActorRelativeRotation(SwingRestRotation);
		SwingElapsed = -1.0f;
		return;
	}

	// 0→1→0 的正弦：挥出去再自己回到静止姿态
	const float Curve = FMath::Sin(UE_PI * Alpha);
	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, Curve, 2.0f);
	SetActorRelativeRotation(FMath::Lerp(SwingRestRotation, SwingRestRotation + FRotator(0.0f, SwingAngle, 0.0f), Eased));
}

void ABambooSword::SweepBlade(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !BladeBox || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const FTransform CurrentTransform = BladeBox->GetComponentTransform();
	if (!bHasPreviousBladeTransform)
	{
		// 第一帧（或刚被传送/挂到手上）没有上一帧可比，直接记录，避免拖出一条巨大的扫掠
		PreviousBladeTransform = CurrentTransform;
		bHasPreviousBladeTransform = true;
		return;
	}

	const FVector PreviousCenter = PreviousBladeTransform.GetLocation();
	const FVector CurrentCenter = CurrentTransform.GetLocation();
	const FVector PreviousTip = PreviousBladeTransform.TransformPosition(FVector(BladeLength, 0.0f, 0.0f));
	const FVector CurrentTip = CurrentTransform.TransformPosition(FVector(BladeLength, 0.0f, 0.0f));
	const float TipSpeed = static_cast<float>((CurrentTip - PreviousTip).Size()) / DeltaSeconds;

	if (TipSpeed >= MinCutSpeed)
	{
		FCollisionQueryParams QueryParams(TEXT("BambooSwordBlade"), false, this);
		QueryParams.AddIgnoredActor(this);
		if (IsValid(GetOwner()))
		{
			QueryParams.AddIgnoredActor(GetOwner());
		}

		const FCollisionShape BladeShape = FCollisionShape::MakeBox(BladeBox->GetScaledBoxExtent());

		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits, PreviousCenter, CurrentCenter, PreviousBladeTransform.GetRotation(),
			ECC_Visibility, BladeShape, QueryParams);

		for (const FHitResult& Hit : Hits)
		{
			ABambooStalk* Stalk = Cast<ABambooStalk>(Hit.GetActor());
			if (!IsValid(Stalk) || Stalk->IsCut())
			{
				continue;
			}

			// 命中点跟着刀身一起运动：取它在上一帧的位置，两点相减就是该处刀身的运动速度
			const FVector LocalImpact = CurrentTransform.InverseTransformPosition(Hit.ImpactPoint);
			const FVector PreviousImpact = PreviousBladeTransform.TransformPosition(LocalImpact);
			CutBamboo(Stalk, Hit.ImpactPoint, (Hit.ImpactPoint - PreviousImpact) / DeltaSeconds);
		}

		if (bDrawDebug)
		{
			DrawDebugBox(World, CurrentCenter, BladeBox->GetScaledBoxExtent(), CurrentTransform.GetRotation(), FColor::Red, false, 0.5f);
			DrawDebugLine(World, PreviousCenter, CurrentCenter, FColor::Yellow, false, 0.5f, 0, 1.0f);
		}
	}

	PreviousBladeTransform = CurrentTransform;
}

void ABambooSword::CutBamboo(ABambooStalk* Stalk, const FVector& ImpactPoint, const FVector& MotionVelocity)
{
	if (!IsValid(Stalk) || Stalk->IsCut() || !BladeBox)
	{
		return;
	}

	const float ImpactSpeed = static_cast<float>(MotionVelocity.Size());

	FVector FallDirection = MotionVelocity.GetSafeNormal2D();
	if (FallDirection.IsNearlyZero())
	{
		// 竖直劈砍时刀身几乎没有水平位移，退化成"从剑指向竹子"
		FallDirection = (Stalk->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	}

	if (FallDirection.IsNearlyZero())
	{
		FallDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	// 切面 = 刀身长轴与刀身在该点的运动方向张成的平面，切口因此会顺着挥砍方向倾斜
	const FTransform BladeTransform = BladeBox->GetComponentTransform();
	const FVector BladeAxis = BladeTransform.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();

	FVector PlaneNormal = FVector::CrossProduct(BladeAxis, MotionVelocity.GetSafeNormal()).GetSafeNormal();
	if (PlaneNormal.IsNearlyZero())
	{
		// 刀身正好沿着运动方向：退而用刀面自身的法线（刀身最薄的那根轴）
		PlaneNormal = BladeTransform.TransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	}

	Stalk->CutAlongPlane(ImpactPoint, PlaneNormal, FallDirection, ImpactSpeed);
}
