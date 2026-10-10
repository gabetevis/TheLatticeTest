#include "Player/MegaCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Player/MegaCharacterMovementComponent.h"
#include "Player/MegaDefaultInput.h"

AMegaCharacter::AMegaCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMegaCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);

	// Body yaw follows the controller; pitch/roll only affect the camera.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	MegaMovement = Cast<UMegaCharacterMovementComponent>(GetCharacterMovement());

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 90.f - EyeDepthBelowCapsuleTop));
	// Rotation is driven in UpdateCamera so roll (strafe/slide tilt) can be layered on top.
	FirstPersonCamera->bUsePawnControlRotation = false;
	FirstPersonCamera->SetFieldOfView(BaseFieldOfView);

	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(FirstPersonCamera);
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->CastShadow = false;
	FirstPersonArms->bCastDynamicShadow = false;
	FirstPersonArms->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	// The full-body mesh (if any) is for shadows/mirrors only.
	GetMesh()->SetOwnerNoSee(true);
}

void AMegaCharacter::BeginPlay()
{
	Super::BeginPlay();

	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	SmoothedEyeHeightAboveFeet = 2.f * HalfHeight - EyeDepthBelowCapsuleTop;
	FirstPersonCamera->SetFieldOfView(BaseFieldOfView);
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

void AMegaCharacter::EnsureInputAssets()
{
	if (DefaultMappingContext && MoveAction && LookAction && JumpAction && DashAction && SlideAction)
	{
		return;
	}

	// Only fill what's missing, so partially-authored assets still win.
	const FMegaDefaultInput Defaults = FMegaDefaultInput::Create(this);
	if (!DefaultMappingContext) { DefaultMappingContext = Defaults.MappingContext; }
	if (!MoveAction) { MoveAction = Defaults.Move; }
	if (!LookAction) { LookAction = Defaults.Look; }
	if (!JumpAction) { JumpAction = Defaults.Jump; }
	if (!DashAction) { DashAction = Defaults.Dash; }
	if (!SlideAction) { SlideAction = Defaults.Slide; }
}

void AMegaCharacter::PawnClientRestart()
{
	EnsureInputAssets();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Mapping context is added before Super so it is active as soon as the pawn is possessed.
	Super::PawnClientRestart();
}

void AMegaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	EnsureInputAssets();

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMegaCharacter::Input_Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMegaCharacter::Input_Look);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AMegaCharacter::Input_Jump);
	Input->BindAction(DashAction, ETriggerEvent::Started, this, &AMegaCharacter::Input_Dash);
	Input->BindAction(SlideAction, ETriggerEvent::Started, this, &AMegaCharacter::Input_SlidePressed);
	Input->BindAction(SlideAction, ETriggerEvent::Completed, this, &AMegaCharacter::Input_SlideReleased);
	Input->BindAction(SlideAction, ETriggerEvent::Canceled, this, &AMegaCharacter::Input_SlideReleased);
}

void AMegaCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AMegaCharacter::Input_Look(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	// Rotation applied directly (instead of AddControllerYaw/PitchInput) so look direction does
	// not depend on the project's legacy input-scale settings.
	const FVector2D Axis = Value.Get<FVector2D>();
	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw += Axis.X * LookSensitivity;
	Rotation.Pitch = FMath::Clamp(
		FRotator::NormalizeAxis(Rotation.Pitch + Axis.Y * LookSensitivity * (bInvertLookY ? -1.0 : 1.0)),
		-89.0, 89.0);
	Rotation.Roll = 0.f;
	Controller->SetControlRotation(Rotation);
}

void AMegaCharacter::Input_Jump(const FInputActionValue& Value)
{
	if (MegaMovement && MegaMovement->TryJump())
	{
		bJumpBuffered = false;
		return;
	}
	bJumpBuffered = true;
	JumpBufferedAtTime = GetWorld()->GetTimeSeconds();
}

void AMegaCharacter::Input_Dash(const FInputActionValue& Value)
{
	if (MegaMovement)
	{
		MegaMovement->TryDash();
	}
}

void AMegaCharacter::Input_SlidePressed(const FInputActionValue& Value)
{
	if (MegaMovement)
	{
		MegaMovement->SetSlideInputHeld(true);
	}
}

void AMegaCharacter::Input_SlideReleased(const FInputActionValue& Value)
{
	if (MegaMovement)
	{
		MegaMovement->SetSlideInputHeld(false);
	}
}

// ---------------------------------------------------------------------------------------------
// Tick / camera
// ---------------------------------------------------------------------------------------------

void AMegaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bJumpBuffered)
	{
		if (GetWorld()->GetTimeSeconds() - JumpBufferedAtTime > JumpBufferTime)
		{
			bJumpBuffered = false;
		}
		else if (MegaMovement && MegaMovement->TryJump())
		{
			bJumpBuffered = false;
		}
	}

	if (MegaMovement && MegaMovement->IsFalling())
	{
		LastFallSpeed = FMath::Max(0.f, static_cast<float>(-GetVelocity().Z));
	}

	UpdateCamera(DeltaSeconds);
}

void AMegaCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	LandingDipOffset = -FMath::Min(LastFallSpeed * LandingDipPerFallSpeed, LandingDipMax);
	LastFallSpeed = 0.f;
}

void AMegaCharacter::UpdateCamera(float DeltaSeconds)
{
	if (!FirstPersonCamera)
	{
		return;
	}

	// Eye height tracks the capsule top, smoothed so crouch/slide transitions don't pop.
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float TargetEyeAboveFeet = 2.f * HalfHeight - EyeDepthBelowCapsuleTop;
	SmoothedEyeHeightAboveFeet = FMath::FInterpTo(SmoothedEyeHeightAboveFeet, TargetEyeAboveFeet, DeltaSeconds, EyeHeightInterpSpeed);
	LandingDipOffset = FMath::FInterpTo(LandingDipOffset, 0.f, DeltaSeconds, LandingDipRecoverySpeed);
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, SmoothedEyeHeightAboveFeet - HalfHeight + LandingDipOffset));

	// FOV widens with horizontal speed.
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const float SpeedAlpha = FMath::Clamp((Speed - FOVBonusStartSpeed) / FMath::Max(1.f, FOVBonusFullSpeed - FOVBonusStartSpeed), 0.f, 1.f);
	const float TargetFOV = BaseFieldOfView + SpeedAlpha * MaxSpeedFOVBonus;
	FirstPersonCamera->SetFieldOfView(FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaSeconds, FOVInterpSpeed));

	// Roll: tilt into strafes, and harder while sliding.
	const float StrafeAmount = static_cast<float>(FVector::DotProduct(GetLastMovementInputVector(), GetActorRightVector()));
	float TargetRoll = StrafeAmount * StrafeRollDegrees;
	if (MegaMovement && MegaMovement->IsSliding())
	{
		TargetRoll += (StrafeAmount >= 0.f ? 1.f : -1.f) * SlideRollDegrees;
	}
	CurrentRoll = FMath::FInterpTo(CurrentRoll, TargetRoll, DeltaSeconds, RollInterpSpeed);

	FRotator ViewRotation = GetControlRotation();
	ViewRotation.Roll = static_cast<double>(CurrentRoll);
	FirstPersonCamera->SetWorldRotation(ViewRotation);
}
