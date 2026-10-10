#include "Player/MegaCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UMegaCharacterMovementComponent::UMegaCharacterMovementComponent()
{
	// Fast, snappy base movement. Everything here is a starting point for feel tuning.
	MaxWalkSpeed = 1150.f;
	MaxWalkSpeedCrouched = 450.f;
	MaxAcceleration = 9000.f;
	BrakingDecelerationWalking = 7000.f;
	GroundFriction = 10.f;

	GravityScale = 2.2f;
	JumpZVelocity = 950.f;
	AirControl = 0.65f;
	BrakingDecelerationFalling = 0.f; // keep air momentum (slide jumps, dash jumps)
	FallingLateralFriction = 0.f;

	// Sliding uses the engine crouch to shrink the capsule.
	GetNavAgentPropertiesRef().bCanCrouch = true;
	SetCrouchedHalfHeight(48.f);
	bCanWalkOffLedgesWhenCrouching = true;

	bOrientRotationToMovement = false;
}

void UMegaCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	DashStamina = static_cast<float>(MaxDashCharges);
	WallJumpsRemaining = MaxWallJumps;
}

float UMegaCharacterMovementComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void UMegaCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (CharacterOwner && UpdatedComponent)
	{
		// Dash timer / stamina regen.
		if (bIsDashing)
		{
			DashTimeRemaining -= DeltaTime;
			if (DashTimeRemaining <= 0.f)
			{
				EndDash(/*bApplyExitVelocity*/ true);
			}
		}
		else
		{
			DashStamina = FMath::Min(static_cast<float>(MaxDashCharges), DashStamina + DashChargesRegenPerSecond * DeltaTime);
		}

		// Slide: ends when leaving the ground, (re)starts whenever held on the ground.
		if (bIsSliding && !IsMovingOnGround())
		{
			EndSlide();
		}
		else if (bSlideInputHeld && !bIsSliding && !bIsDashing && IsMovingOnGround())
		{
			StartSlide();
		}

		// Slam: straight down, no horizontal drift.
		if (bIsSlamming)
		{
			if (IsFalling())
			{
				Velocity.X = 0.f;
				Velocity.Y = 0.f;
				Velocity.Z = FMath::Min(Velocity.Z, -SlamSpeed);
			}
			else if (!IsMovingOnGround())
			{
				bIsSlamming = false; // e.g. entered water
			}
		}

		ApplyWallSlide();
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsMovingOnGround())
	{
		LastGroundedTime = GetWorldTime();
	}
}

// ---------------------------------------------------------------------------------------------
// Engine overrides
// ---------------------------------------------------------------------------------------------

void UMegaCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (bIsDashing)
	{
		// Constant horizontal velocity. In the air, PhysFalling re-adds the old Z (which we zeroed
		// when the dash started) and gravity is 0 while dashing, so the dash stays flat.
		Velocity = DashDirection * DashSpeed;
		return;
	}

	if (bIsSliding && IsMovingOnGround())
	{
		const FVector InputDir = GetCurrentAcceleration().GetSafeNormal2D();
		if (!InputDir.IsNearlyZero() && SlideSteerDegreesPerSecond > 0.f)
		{
			const float Dot = FMath::Clamp(FVector::DotProduct(SlideDirection, InputDir), -1.f, 1.f);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));

			// Ignore input pointing mostly backwards; that should not U-turn a slide.
			if (AngleDeg > KINDA_SMALL_NUMBER && AngleDeg < 120.f)
			{
				const float Sign = FMath::Sign(FVector::CrossProduct(SlideDirection, InputDir).Z);
				const float Step = FMath::Min(AngleDeg, SlideSteerDegreesPerSecond * DeltaTime);
				SlideDirection = SlideDirection.RotateAngleAxis(Step * Sign, FVector::UpVector).GetSafeNormal2D();
			}
		}

		Velocity = SlideDirection * SlideSpeed;
		return;
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

float UMegaCharacterMovementComponent::GetGravityZ() const
{
	return bIsDashing ? 0.f : Super::GetGravityZ();
}

void UMegaCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (MovementMode == MOVE_Walking && PreviousMovementMode == MOVE_Falling)
	{
		HandleLanded();
	}
}

// ---------------------------------------------------------------------------------------------
// Jumping
// ---------------------------------------------------------------------------------------------

bool UMegaCharacterMovementComponent::TryJump()
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	const float Now = GetWorldTime();

	if (IsMovingOnGround())
	{
		if (Now - SlamLandTime <= SlamJumpWindow)
		{
			const float SlamJumpZ = FMath::Min(SlamJumpBaseVelocity + LastSlamFallDistance * SlamJumpFallDistanceScale, SlamJumpMaxVelocity);
			LaunchJump(FVector(Velocity.X, Velocity.Y, SlamJumpZ), EMegaJumpType::Slam);
			return true;
		}

		if (bIsDashing)
		{
			const bool bCanAfford = DashStamina >= DashJumpStaminaCost;
			if (bCanAfford)
			{
				DashStamina -= DashJumpStaminaCost;
			}
			const FVector Horizontal = DashDirection * (bCanAfford ? DashJumpHorizontalSpeed : DashExitSpeed);
			LaunchJump(Horizontal + FVector(0.f, 0.f, JumpZVelocity * DashJumpVerticalScale), EMegaJumpType::Dash);
			return true;
		}

		if (bIsSliding)
		{
			LaunchJump(SlideDirection * SlideSpeed + FVector(0.f, 0.f, JumpZVelocity * SlideJumpVerticalScale), EMegaJumpType::Slide);
			return true;
		}

		LaunchJump(FVector(Velocity.X, Velocity.Y, JumpZVelocity), EMegaJumpType::Ground);
		return true;
	}

	if (IsFalling())
	{
		if (!bJumpedSinceGrounded && !bIsSlamming && (Now - LastGroundedTime) <= CoyoteTime)
		{
			LaunchJump(FVector(Velocity.X, Velocity.Y, JumpZVelocity), EMegaJumpType::Coyote);
			return true;
		}

		if (WallJumpsRemaining > 0)
		{
			FHitResult WallHit;
			if (FindWall(WallHit))
			{
				--WallJumpsRemaining;
				const FVector WallNormal = WallHit.ImpactNormal.GetSafeNormal2D();
				const FVector NewVelocity =
					WallNormal * WallJumpOutSpeed +
					GetMoveInputDirection2D() * WallJumpInputInfluence +
					FVector::UpVector * WallJumpUpSpeed;
				LaunchJump(NewVelocity, EMegaJumpType::Wall);
				return true;
			}
		}
	}

	return false;
}

void UMegaCharacterMovementComponent::LaunchJump(const FVector& NewVelocity, EMegaJumpType JumpType)
{
	if (bIsDashing)
	{
		EndDash(/*bApplyExitVelocity*/ false);
	}
	if (bIsSliding)
	{
		EndSlide();
	}

	bIsSlamming = false;
	SlamLandTime = -1000.f;
	bJumpedSinceGrounded = true;

	Velocity = NewVelocity;
	SetMovementMode(MOVE_Falling);

	OnMegaJumped.Broadcast(JumpType);
}

// ---------------------------------------------------------------------------------------------
// Dash
// ---------------------------------------------------------------------------------------------

bool UMegaCharacterMovementComponent::TryDash()
{
	if (!CharacterOwner || !UpdatedComponent || bIsDashing || DashStamina < 1.f)
	{
		return false;
	}
	if (!IsMovingOnGround() && !IsFalling())
	{
		return false;
	}

	FVector Direction = GetMoveInputDirection2D();
	if (Direction.IsNearlyZero())
	{
		Direction = GetFacingDirection2D();
	}

	DashStamina -= 1.f;
	StartDash(Direction);
	return true;
}

void UMegaCharacterMovementComponent::StartDash(const FVector& Direction)
{
	if (bIsSliding)
	{
		EndSlide();
	}
	bIsSlamming = false;

	bIsDashing = true;
	DashTimeRemaining = DashDuration;
	DashDirection = Direction;
	Velocity = DashDirection * DashSpeed; // Z = 0: dashes are always horizontal

	OnMegaDashed.Broadcast(DashDirection);
}

void UMegaCharacterMovementComponent::EndDash(bool bApplyExitVelocity)
{
	bIsDashing = false;
	DashTimeRemaining = 0.f;

	if (bApplyExitVelocity)
	{
		Velocity = DashDirection * DashExitSpeed;
	}
}

// ---------------------------------------------------------------------------------------------
// Slide / slam
// ---------------------------------------------------------------------------------------------

void UMegaCharacterMovementComponent::SetSlideInputHeld(bool bHeld)
{
	bSlideInputHeld = bHeld;

	if (bHeld)
	{
		if (IsFalling() && !bIsSlamming)
		{
			StartSlam();
		}
		else if (IsMovingOnGround() && !bIsSliding)
		{
			if (bIsDashing)
			{
				EndDash(/*bApplyExitVelocity*/ false);
			}
			StartSlide();
		}
	}
	else if (bIsSliding)
	{
		EndSlide();
	}
}

void UMegaCharacterMovementComponent::StartSlide()
{
	// Prefer input direction, then current motion, then facing.
	FVector Direction = GetMoveInputDirection2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Velocity.Size2D() > 100.f ? Velocity.GetSafeNormal2D() : GetFacingDirection2D();
	}

	SlideDirection = Direction;
	bIsSliding = true;
	CharacterOwner->Crouch();
	OnMegaSlideChanged.Broadcast(true);
}

void UMegaCharacterMovementComponent::EndSlide()
{
	bIsSliding = false;
	if (CharacterOwner)
	{
		CharacterOwner->UnCrouch(); // stays crouched automatically if there's a ceiling
	}
	OnMegaSlideChanged.Broadcast(false);
}

void UMegaCharacterMovementComponent::StartSlam()
{
	if (bIsDashing)
	{
		EndDash(/*bApplyExitVelocity*/ false);
	}

	bIsSlamming = true;
	SlamStartZ = UpdatedComponent->GetComponentLocation().Z;
	Velocity = FVector(0.f, 0.f, -SlamSpeed);
}

void UMegaCharacterMovementComponent::HandleLanded()
{
	WallJumpsRemaining = MaxWallJumps;
	bJumpedSinceGrounded = false;

	if (bIsSlamming)
	{
		bIsSlamming = false;

		const FVector Location = UpdatedComponent->GetComponentLocation();
		LastSlamFallDistance = FMath::Max(0.f, SlamStartZ - Location.Z);
		SlamLandTime = GetWorldTime();
		Velocity = FVector::ZeroVector;

		const float HalfHeight = CharacterOwner ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
		OnMegaSlamLanded.Broadcast(Location - FVector(0.f, 0.f, HalfHeight), LastSlamFallDistance);
	}
}

// ---------------------------------------------------------------------------------------------
// Walls
// ---------------------------------------------------------------------------------------------

void UMegaCharacterMovementComponent::ApplyWallSlide()
{
	if (!IsFalling() || bIsSlamming || bIsDashing || Velocity.Z >= -WallSlideMaxFallSpeed)
	{
		return;
	}

	const FVector InputDir = GetMoveInputDirection2D();
	if (InputDir.IsNearlyZero())
	{
		return;
	}

	FHitResult WallHit;
	if (FindWall(WallHit) && FVector::DotProduct(InputDir, -WallHit.ImpactNormal.GetSafeNormal2D()) > 0.3f)
	{
		Velocity.Z = -WallSlideMaxFallSpeed;
	}
}

bool UMegaCharacterMovementComponent::FindWall(FHitResult& OutHit) const
{
	if (!CharacterOwner || !UpdatedPrimitive)
	{
		return false;
	}

	const FVector Center = UpdatedComponent->GetComponentLocation();
	const float Reach = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() + WallCheckDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MegaWallCheck), false, CharacterOwner);
	FCollisionResponseParams Response;
	UpdatedPrimitive->InitSweepCollisionParams(Params, Response);
	const ECollisionChannel Channel = UpdatedPrimitive->GetCollisionObjectType();

	constexpr int32 NumRays = 8;
	float BestDistance = TNumericLimits<float>::Max();
	bool bFound = false;

	for (int32 i = 0; i < NumRays; ++i)
	{
		const FVector Direction = FRotator(0.f, i * (360.f / NumRays), 0.f).Vector();
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Center, Center + Direction * Reach, Channel, Params, Response) &&
			FMath::Abs(Hit.ImpactNormal.Z) <= WallMaxNormalZ &&
			Hit.Distance < BestDistance)
		{
			BestDistance = Hit.Distance;
			OutHit = Hit;
			bFound = true;
		}
	}

	return bFound;
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

FVector UMegaCharacterMovementComponent::GetMoveInputDirection2D() const
{
	// Pending input is this frame's input (consumed inside Super::TickComponent). Fall back to
	// last frame's when called before movement input was processed this frame.
	FVector Input = GetPendingInputVector();
	if (Input.IsNearlyZero() && PawnOwner)
	{
		Input = PawnOwner->GetLastMovementInputVector();
	}
	return Input.GetSafeNormal2D();
}

FVector UMegaCharacterMovementComponent::GetFacingDirection2D() const
{
	return CharacterOwner ? CharacterOwner->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
}
