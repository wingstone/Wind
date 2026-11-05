// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFlierPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"

AWindFlierPawn::AWindFlierPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Pawn 完全掌管自身旋转。只有当下面任意一个为 true 时，
	// APawn::FaceRotation 才会去修改 Actor 的旋转，所以保持关闭
	// 可以避免 Controller 和瞄准互相打架。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 刻意只用一个裸的 SceneComponent：风没有身体，所以这里的任何东西
	// 都不会碰撞、不会阻挡、不会推动其他物体。
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SceneRoot);
	ViewCamera->bUsePawnControlRotation = false;
	ViewCamera->FieldOfView = 90.0f;
}

void AWindFlierPawn::BeginPlay()
{
	Super::BeginPlay();

	// 从 Pawn 被放置的位置继承瞄准，这样关卡里摆放的 Flier
	// 一开始就看向 PlayerStart 指向的方向。
	const FRotator StartRotation = GetActorRotation();
	CurrentYaw = TargetYaw = StartRotation.Yaw;
	CurrentPitch = TargetPitch = FMath::Clamp(StartRotation.Pitch, MinPitch, MaxPitch);
	CurrentBank = 0.0f;

	SetActorRotation(FRotator(CurrentPitch, CurrentYaw, CurrentBank));
}

void AWindFlierPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (APlayerController* PlayerController = Cast<APlayerController>(NewController))
	{
		PlayerController->bShowMouseCursor = false;
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void AWindFlierPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateAim(DeltaSeconds);
	UpdateFlight(DeltaSeconds);

	SetActorRotation(FRotator(CurrentPitch, CurrentYaw, CurrentBank));
}

void AWindFlierPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent)
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (EnhancedInput && MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWindFlierPawn::Input_MoveAction);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &AWindFlierPawn::Input_MoveActionStopped);
	}
	else
	{
		PlayerInputComponent->BindAxis(TEXT("WindFlier_MoveForward"), this, &AWindFlierPawn::Input_MoveForward);
		PlayerInputComponent->BindAxis(TEXT("WindFlier_MoveRight"), this, &AWindFlierPawn::Input_MoveRight);
		PlayerInputComponent->BindAxis(TEXT("WindFlier_MoveUp"), this, &AWindFlierPawn::Input_MoveUp);
	}

	if (EnhancedInput && LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWindFlierPawn::Input_LookAction);
	}
	else
	{
		PlayerInputComponent->BindAxis(TEXT("WindFlier_LookYaw"), this, &AWindFlierPawn::Input_LookYaw);
		PlayerInputComponent->BindAxis(TEXT("WindFlier_LookPitch"), this, &AWindFlierPawn::Input_LookPitch);
	}
}

void AWindFlierPawn::DoMove(float Right, float Forward, float Up)
{
	MoveInputRight = Right;
	MoveInputForward = Forward;
	MoveInputUp = Up;
}

void AWindFlierPawn::DoLook(float Yaw, float Pitch)
{
	TargetYaw += Yaw * LookSensitivity;
	TargetPitch = FMath::Clamp(TargetPitch + Pitch * LookSensitivity, MinPitch, MaxPitch);
}

void AWindFlierPawn::Input_MoveAction(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	MoveInputRight = Axis.X;
	MoveInputForward = Axis.Y;
}

void AWindFlierPawn::Input_MoveActionStopped(const FInputActionValue&)
{
	MoveInputRight = 0.0f;
	MoveInputForward = 0.0f;
}

void AWindFlierPawn::Input_LookAction(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	DoLook(Axis.X, Axis.Y);
}

void AWindFlierPawn::UpdateAim(float DeltaSeconds)
{
	// 偶尔收缩一下未展开的角度，但不改变它们之间的差值，
	// 这样长时间游玩也不会漂到浮点精度不够用的地步。
	if (FMath::Abs(TargetYaw) > 3600.0f)
	{
		const float Offset = FMath::GridSnap(TargetYaw, 360.0f);
		TargetYaw -= Offset;
		CurrentYaw -= Offset;
	}

	// 瞄准是追着目标走的，而不是直接等于目标。那段滞后就是所谓的"拖拽感"。
	CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, AimInterpSpeed);
	CurrentPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaSeconds, AimInterpSpeed);

	float TargetBank = 0.0f;
	if (BankAngleMax > 0.0f)
	{
		const FRotator AimRotation(CurrentPitch, CurrentYaw, 0.0f);
		const float LateralSpeed = FVector::DotProduct(FlyVelocity, FRotationMatrix(AimRotation).GetUnitAxis(EAxis::Y));

		// 负 roll 使 Pawn 向横向滑动的那一侧压翼。
		TargetBank = -FMath::Clamp(LateralSpeed / FMath::Max(MaxFlySpeed, 1.0f), -1.0f, 1.0f) * BankAngleMax;
	}

	CurrentBank = FMath::FInterpTo(CurrentBank, TargetBank, DeltaSeconds, BankInterpSpeed);
}

void AWindFlierPawn::UpdateFlight(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Roll 不参与飞行基向量：倾斜只影响视觉，不影响滑翔方向。
	const FMatrix AimBasis = FRotationMatrix(FRotator(CurrentPitch, CurrentYaw, 0.0f));

	FVector Throttle = AimBasis.GetUnitAxis(EAxis::X) * MoveInputForward + AimBasis.GetUnitAxis(EAxis::Y) * MoveInputRight;
	Throttle = Throttle.GetClampedToMaxSize(1.0f);

	FVector Acceleration = Throttle * FlyAcceleration;
	Acceleration += FVector::UpVector * (MoveInputUp * FlyAcceleration * VerticalThrustScale);

	if (bApplyGravity && GetWorld())
	{
		Acceleration += FVector::UpVector * (GetWorld()->GetGravityZ() * GravityScale);
	}

	FlyVelocity += Acceleration * DeltaSeconds;

	// 指数阻力与帧率无关，正是它让 Pawn 可以滑翔：
	// 松开油门后，速度只能缓慢下降，不会骤停。
	FlyVelocity *= FMath::Exp(-AirDrag * DeltaSeconds);

	const float MaxSpeed = FMath::Max(MaxFlySpeed, 1.0f);
	if (FlyVelocity.SizeSquared() > FMath::Square(MaxSpeed))
	{
		FlyVelocity = FlyVelocity.GetClampedToMaxSize(MaxSpeed);
	}

	if (!FlyVelocity.IsNearlyZero(0.1f))
	{
		// 不使用 sweep：Pawn 不应该被几何体阻挡。
		AddActorWorldOffset(FlyVelocity * DeltaSeconds, /*bSweep=*/false);
	}
}
