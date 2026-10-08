// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFlierPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "WindGustComponent.h"

AWindFlierPawn::AWindFlierPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// The pawn owns its rotation outright. APawn::FaceRotation only touches the
	// actor when one of these is set, so leaving them off keeps the controller
	// from fighting the aim.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Deliberately a bare scene component: the wind has no body, so nothing here
	// can collide, block, or push.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SceneRoot);
	ViewCamera->bUsePawnControlRotation = false;
	ViewCamera->FieldOfView = 90.0f;

	WindGust = CreateDefaultSubobject<UWindGustComponent>(TEXT("WindGust"));
	WindGust->SetupAttachment(ViewCamera);
}

void AWindFlierPawn::BeginPlay()
{
	Super::BeginPlay();

	// Seed the aim from wherever the pawn was placed, so a level-placed flier
	// already looks where its PlayerStart points.
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

	if (EnhancedInput && ChargeAction)
	{
		EnhancedInput->BindAction(ChargeAction, ETriggerEvent::Started, this, &AWindFlierPawn::DoChargeStart);
		EnhancedInput->BindAction(ChargeAction, ETriggerEvent::Completed, this, &AWindFlierPawn::DoChargeStop);
	}
	else
	{
		PlayerInputComponent->BindAction(TEXT("WindFlier_Charge"), IE_Pressed, this, &AWindFlierPawn::DoChargeStart);
		PlayerInputComponent->BindAction(TEXT("WindFlier_Charge"), IE_Released, this, &AWindFlierPawn::DoChargeStop);
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

void AWindFlierPawn::DoChargeStart()
{
	if (WindGust)
	{
		WindGust->StartCharging();
	}
}

void AWindFlierPawn::DoChargeStop()
{
	if (WindGust)
	{
		WindGust->StopCharging();
	}
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
	// Shrink the unwrapped angles now and then without changing the delta between
	// them, so a long session cannot drift into float precision trouble.
	if (FMath::Abs(TargetYaw) > 3600.0f)
	{
		const float Offset = FMath::GridSnap(TargetYaw, 360.0f);
		TargetYaw -= Offset;
		CurrentYaw -= Offset;
	}

	// The aim chases the target rather than matching it. That lag is the drag.
	CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, AimInterpSpeed);
	CurrentPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaSeconds, AimInterpSpeed);

	float TargetBank = 0.0f;
	if (BankAngleMax > 0.0f)
	{
		const FRotator AimRotation(CurrentPitch, CurrentYaw, 0.0f);
		const float LateralSpeed = FVector::DotProduct(FlyVelocity, FRotationMatrix(AimRotation).GetUnitAxis(EAxis::Y));

		// Negative roll drops the wing on the side the pawn is sliding toward.
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

	// Roll is excluded from the flight basis: banking tilts the view, it does not
	// steer the glide.
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

	// Exponential drag is frame-rate independent and is what makes the pawn glide:
	// letting go of the throttle can only ever slow it down gradually.
	FlyVelocity *= FMath::Exp(-AirDrag * DeltaSeconds);

	const float MaxSpeed = FMath::Max(MaxFlySpeed, 1.0f);
	if (FlyVelocity.SizeSquared() > FMath::Square(MaxSpeed))
	{
		FlyVelocity = FlyVelocity.GetClampedToMaxSize(MaxSpeed);
	}

	if (!FlyVelocity.IsNearlyZero(0.1f))
	{
		// Never swept: the pawn is not meant to be stopped by geometry.
		AddActorWorldOffset(FlyVelocity * DeltaSeconds, /*bSweep=*/false);
	}
}
