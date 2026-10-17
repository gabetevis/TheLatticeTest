#include "Player/MegaCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Player/MegaCharacterMovementComponent.h"
#include "Player/MegaDefaultInput.h"
#include "Player/MegaGrappleComponent.h"

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

	Grapple = CreateDefaultSubobject<UMegaGrappleComponent>(TEXT("Grapple"));
	Grapple->SetupAttachment(FirstPersonCamera);
	Grapple->SetRelativeLocation(FVector(30.f, 22.f, -22.f)); // lower right, like a hand

	// The TOOL: the forearm weapon. ToolRoot is its pivot on the camera; ToolMesh is the rigged mesh.
	ToolRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ToolRoot"));
	ToolRoot->SetupAttachment(FirstPersonCamera);

	ToolMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ToolMesh"));
	ToolMesh->SetupAttachment(ToolRoot);
	ToolMesh->SetOnlyOwnerSee(true);            // viewmodel: only the player sees it
	ToolMesh->SetCastShadow(false);             // a floating arm-weapon shadow would look wrong
	ToolMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ToolMesh->SetGenerateOverlapEvents(false);
	// Keep animating even if the engine thinks the mesh is off screen (it's always in front of the camera).
	ToolMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Default asset paths (right-click an asset in the Content Browser > Copy Reference to get these).
	ToolMeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Tool/SK_TOOL.SK_TOOL")));
	ToolIdleAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Tool/SK_TOOL_Anim_Armature_TOOL_Idle.SK_TOOL_Anim_Armature_TOOL_Idle")));
	ToolFireAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Tool/SK_TOOL_Anim_Armature_TOOL_Fire.SK_TOOL_Anim_Armature_TOOL_Fire")));
	ToolReloadAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Tool/SK_TOOL_Anim_Armature_TOOL_Reload.SK_TOOL_Anim_Armature_TOOL_Reload")));

	// The full-body mesh (if any) is for shadows/mirrors only.
	GetMesh()->SetOwnerNoSee(true);
}

// ---------------------------------------------------------------------------------------------
// TOOL (forearm weapon)
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Loads a soft reference, logging (instead of crashing) if the asset is missing or renamed. */
	template <typename T>
	T* LoadToolAsset(const TSoftObjectPtr<T>& Ref)
	{
		if (Ref.IsNull())
		{
			return nullptr;
		}
		T* Asset = Ref.LoadSynchronous();
		if (!Asset)
		{
			UE_LOG(LogTemp, Warning, TEXT("TOOL: could not load %s (imported and saved?)"), *Ref.ToString());
		}
		return Asset;
	}
}

void AMegaCharacter::ApplyToolLayout()
{
	if (!ToolRoot || !ToolMesh)
	{
		return;
	}

	// 1) Placement: ToolRoot sits on the camera; the mesh's origin (the attach point) sits on ToolRoot.
	ToolRoot->SetRelativeLocationAndRotation(ToolOffset, ToolRotation);
	ToolRoot->SetRelativeScale3D(FVector(ToolScale));

	// 2) Assets.
	USkeletalMesh* ToolSkeletalMesh = LoadToolAsset(ToolMeshAsset);
	ToolMesh->SetSkeletalMeshAsset(ToolSkeletalMesh);
	LoadedIdleAnim = LoadToolAsset(ToolIdleAnim);
	LoadedFireAnim = LoadToolAsset(ToolFireAnim);
	LoadedReloadAnim = LoadToolAsset(ToolReloadAnim);

	// 3) PS1-style stepping: the mesh (and so its animation) only updates ToolAnimStepRate times a second.
	ToolMesh->SetComponentTickInterval(ToolAnimStepRate > 0.f ? 1.f / ToolAnimStepRate : 0.f);

	// 4) Glow: find the material slot that has the glow parameter and give it a per-character dynamic copy.
	ToolGlowMaterial = nullptr;
	for (int32 Index = 0; Index < ToolMesh->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* Material = ToolMesh->GetMaterial(Index);
		float Value = 0.f;
		if (Material && Material->GetScalarParameterValue(FMaterialParameterInfo(ToolGlowParameter), Value))
		{
			ToolGlowMaterial = ToolMesh->CreateDynamicMaterialInstance(Index, Material);
			ToolGlowBase = Value;
			break;
		}
	}

	// 5) Start in idle.
	GetWorldTimerManager().ClearTimer(ToolActionTimer);
	ToolAction = EToolAction::Idle;
	ToolGlowFlash = 0.f;
	PlayToolAnim(LoadedIdleAnim, true);

	if (ToolSkeletalMesh)
	{
		UE_LOG(LogTemp, Log, TEXT("TOOL: %s loaded, size %s cm, glow parameter %s"),
			*ToolSkeletalMesh->GetName(), *ToolSkeletalMesh->GetBounds().GetBox().GetSize().ToCompactString(),
			ToolGlowMaterial ? TEXT("found") : TEXT("NOT found"));
	}
}

float AMegaCharacter::PlayToolAnim(UAnimSequence* Anim, bool bLoop)
{
	if (!ToolMesh || !Anim)
	{
		return 0.f;
	}
	// Single-node mode: the mesh plays one animation asset directly, no Animation Blueprint required.
	ToolMesh->PlayAnimation(Anim, bLoop);
	return Anim->GetPlayLength();
}

bool AMegaCharacter::ToolFire()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (ToolAction == EToolAction::Reloading || Now - LastToolFireTime < ToolFireCooldown)
	{
		return false;
	}
	LastToolFireTime = Now;

	// Restarting the fire animation on every shot is intentional: rapid clicks re-kick the recoil.
	ToolAction = EToolAction::Firing;
	const float Length = PlayToolAnim(LoadedFireAnim, false);
	GetWorldTimerManager().SetTimer(ToolActionTimer, this, &AMegaCharacter::OnToolActionFinished, FMath::Max(Length, 0.01f), false);

	ToolGlowFlash = 1.f;
	return true;
}

bool AMegaCharacter::ToolReload()
{
	if (ToolAction == EToolAction::Reloading)
	{
		return false;
	}

	ToolAction = EToolAction::Reloading;
	const float Length = PlayToolAnim(LoadedReloadAnim, false);
	GetWorldTimerManager().SetTimer(ToolActionTimer, this, &AMegaCharacter::OnToolActionFinished, FMath::Max(Length, 0.01f), false);
	return true;
}

void AMegaCharacter::OnToolActionFinished()
{
	// Fire/reload finished: go back to the idle loop.
	ToolAction = EToolAction::Idle;
	PlayToolAnim(LoadedIdleAnim, true);
}

void AMegaCharacter::UpdateToolGlow(float DeltaSeconds)
{
	if (!ToolGlowMaterial || ToolGlowFlash <= 0.f)
	{
		return;
	}

	// Flash fades linearly from 1 to 0; the glow goes from Boost x normal back to normal.
	ToolGlowFlash = FMath::Max(0.f, ToolGlowFlash - DeltaSeconds * ToolGlowFlashDecay);
	const float Multiplier = FMath::Lerp(1.f, ToolFireGlowBoost, ToolGlowFlash);
	ToolGlowMaterial->SetScalarParameterValue(ToolGlowParameter, ToolGlowBase * Multiplier);
}

#if WITH_EDITOR
void AMegaCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Live tuning during Play (F8): re-apply the TOOL whenever one of its settings is edited.
	if (HasActorBegunPlay() && PropertyChangedEvent.MemberProperty &&
		PropertyChangedEvent.MemberProperty->GetName().StartsWith(TEXT("Tool")))
	{
		ApplyToolLayout();
	}
}
#endif

void AMegaCharacter::BeginPlay()
{
	Super::BeginPlay();

	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	SmoothedEyeHeightAboveFeet = 2.f * HalfHeight - EyeDepthBelowCapsuleTop;
	FirstPersonCamera->SetFieldOfView(BaseFieldOfView);

	ApplyToolLayout();
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

void AMegaCharacter::EnsureInputAssets()
{
	if (DefaultMappingContext && MoveAction && LookAction && JumpAction && DashAction && SlideAction && GrappleAction &&
		FireAction && ReloadAction)
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
	if (!GrappleAction) { GrappleAction = Defaults.Grapple; }
	if (!FireAction) { FireAction = Defaults.Fire; }
	if (!ReloadAction) { ReloadAction = Defaults.Reload; }
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
	Input->BindAction(GrappleAction, ETriggerEvent::Started, this, &AMegaCharacter::Input_GrapplePressed);
	Input->BindAction(GrappleAction, ETriggerEvent::Completed, this, &AMegaCharacter::Input_GrappleReleased);
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &AMegaCharacter::Input_Fire);
	Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AMegaCharacter::Input_Reload);
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

void AMegaCharacter::Input_GrapplePressed(const FInputActionValue& Value)
{
	if (Grapple)
	{
		Grapple->OnGrapplePressed();
	}
}

void AMegaCharacter::Input_GrappleReleased(const FInputActionValue& Value)
{
	if (Grapple)
	{
		Grapple->OnGrappleReleased();
	}
}

void AMegaCharacter::Input_Fire(const FInputActionValue& Value)
{
	ToolFire();
}

void AMegaCharacter::Input_Reload(const FInputActionValue& Value)
{
	ToolReload();
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
	UpdateToolGlow(DeltaSeconds);
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
