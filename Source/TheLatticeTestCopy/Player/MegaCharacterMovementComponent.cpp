#include "Player/MegaCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UMegaCharacterMovementComponent::UMegaCharacterMovementComponent()
{
	// Fast, snappy base movement. Everything here is a starting point for feel tuning.
	MaxWalkSpeed = 1150.f;
	MaxWalkSpeedCrouched = 450.f;
	MaxAcceleration = 12000.f;
	BrakingDecelerationWalking = 7000.f;
	GroundFriction = 10.f;

	GravityScale = 2.2f;
	JumpZVelocity = 950.f;
	AirControl = 0.65f;
	BrakingDecelerationFalling = 0.f; // keep air momentum (slide jumps, dash jumps, grapple)
	FallingLateralFriction = 0.f;

	// Crouch and slide both use the engine crouch to shrink the capsule.
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

		// Slide / crouch end when leaving the ground.
		if (bIsSliding && !IsMovingOnGround())
		{
			EndSlide();
		}
		if (bIsCrouchHeld && !IsMovingOnGround())
		{
			EndCrouchHold();
		}

		// Slide ran out of speed (friction / uphill): become a crouch until the button is pressed again.
		if (bIsSliding && IsMovingOnGround() && CurrentSlideSpeed < SlideMinSpeed && GetWorldTime() - SlideStartTime > 0.15f)
		{
			bSlideExhausted = true;
			bIsCrouchHeld = true; // set first so EndSlide keeps the capsule crouched
			EndSlide();
		}

		// Button held on the ground but not yet sliding/crouching (landing, or a dash just ended).
		if (bSlideInputHeld && IsMovingOnGround() && !bIsSliding && !bIsCrouchHeld && !bIsDashing && !bIsGrappling)
		{
			BeginGroundSlideOrCrouch();
		}

		// Slam: straight down, no horizontal drift.
		if (bIsSlamming)
		{
			if (IsFalling())
			{
				Velocity.X = 0.f;
				Velocity.Y = 0.f;
				Velocity.Z = FMath::Min(Velocity.Z, static_cast<double>(-SlamSpeed));
			}
			else if (!IsMovingOnGround())
			{
				bIsSlamming = false; // e.g. entered water
			}
		}

		if (bIsGrappling)
		{
			UpdateGrapple(DeltaTime);
		}

		ApplyWallSlide();
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsMovingOnGround())
	{
		LastGroundedTime = GetWorldTime();
	}
	UpdateMomentumMemory();
}

void UMegaCharacterMovementComponent::UpdateMomentumMemory()
{
	// Keep the fastest horizontal speed of the last MomentumGraceTime seconds.
	const float Now = GetWorldTime();
	const float Speed2D = static_cast<float>(Velocity.Size2D());
	if (Speed2D >= MomentumSpeed || Now - MomentumTime > MomentumGraceTime)
	{
		MomentumSpeed = Speed2D;
		MomentumTime = Now;
		if (Speed2D > KINDA_SMALL_NUMBER)
		{
			MomentumDirection = Velocity.GetSafeNormal2D();
		}
	}
}

float UMegaCharacterMovementComponent::GetCarriedSpeed2D() const
{
	const float Current = static_cast<float>(Velocity.Size2D());
	const bool bMemoryFresh = GetWorldTime() - MomentumTime <= MomentumGraceTime;
	return bMemoryFresh ? FMath::Max(Current, MomentumSpeed) : Current;
}

FVector UMegaCharacterMovementComponent::GetCarriedDirection2D() const
{
	const bool bMemoryFresh = GetWorldTime() - MomentumTime <= MomentumGraceTime;
	if (bMemoryFresh && MomentumSpeed > Velocity.Size2D())
	{
		return MomentumDirection;
	}
	return Velocity.Size2D() > 100.0 ? Velocity.GetSafeNormal2D() : FVector::ZeroVector;
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

	if (bIsGrappling)
	{
		// Velocity is driven entirely by UpdateGrapple; don't let friction or air control fight it.
		return;
	}

	if (bIsSliding && IsMovingOnGround())
	{
		UpdateSlide(DeltaTime);
		Velocity = SlideDirection * CurrentSlideSpeed;
		return;
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

float UMegaCharacterMovementComponent::GetGravityZ() const
{
	return (bIsDashing || bIsGrappling) ? 0.f : Super::GetGravityZ();
}

void UMegaCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (MovementMode == MOVE_Walking && PreviousMovementMode == MOVE_Falling)
	{
		HandleLanded();
	}
}

void UMegaCharacterMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);

	// Slamming into something head-on while grappling releases the rope (as the old Blueprint did).
	if (bIsGrappling && Hit.bBlockingHit)
	{
		const FVector PullDirection = (GetGrapplePoint() - UpdatedComponent->GetComponentLocation()).GetSafeNormal();
		if (FVector::DotProduct(Hit.ImpactNormal, -PullDirection) > 0.5)
		{
			StopGrapple();
		}
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

	if (bIsGrappling)
	{
		StopGrapple();
		LaunchJump(Velocity + FVector(0.f, 0.f, JumpZVelocity * GrappleJumpVerticalScale), EMegaJumpType::Grapple);
		return true;
	}

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
			// Uses the current (possibly boosted) slide speed, so momentum carries into the air.
			LaunchJump(SlideDirection * CurrentSlideSpeed + FVector(0.f, 0.f, JumpZVelocity * SlideJumpVerticalScale), EMegaJumpType::Slide);
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
	if (bIsCrouchHeld)
	{
		EndCrouchHold();
	}

	bIsSlamming = false;
	bSlideExhausted = false; // landing with the button held may slide again
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
	if (bIsGrappling)
	{
		StopGrapple();
	}
	if (bIsSliding)
	{
		EndSlide();
	}
	if (bIsCrouchHeld)
	{
		EndCrouchHold();
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
// Slide / crouch / slam
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
		else if (IsMovingOnGround() && !bIsSliding && !bIsCrouchHeld)
		{
			BeginGroundSlideOrCrouch();
		}
	}
	else
	{
		bSlideExhausted = false;
		if (bIsSliding)
		{
			EndSlide();
		}
		if (bIsCrouchHeld)
		{
			EndCrouchHold();
		}
	}
}

bool UMegaCharacterMovementComponent::WantsToSlide() const
{
	if (bSlideExhausted)
	{
		return false;
	}
	return !GetMoveInputDirection2D().IsNearlyZero() || GetCarriedSpeed2D() > SlideMinSpeedWithoutInput;
}

void UMegaCharacterMovementComponent::UpdateSlide(float DeltaTime)
{
	// 1. Steering: input bends the slide direction (never into a U-turn).
	const FVector InputDir = GetCurrentAcceleration().GetSafeNormal2D();
	if (!InputDir.IsNearlyZero() && SlideSteerDegreesPerSecond > 0.f)
	{
		const float Dot = FMath::Clamp(static_cast<float>(FVector::DotProduct(SlideDirection, InputDir)), -1.f, 1.f);
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
		if (AngleDeg > KINDA_SMALL_NUMBER && AngleDeg < 120.f)
		{
			const float Sign = FMath::Sign(static_cast<float>(FVector::CrossProduct(SlideDirection, InputDir).Z));
			const float Step = FMath::Min(AngleDeg, SlideSteerDegreesPerSecond * DeltaTime);
			SlideDirection = SlideDirection.RotateAngleAxis(Step * Sign, FVector::UpVector).GetSafeNormal2D();
		}
	}

	// 2. Slope: gravity along the floor speeds us up downhill and slows us uphill. On a slope of
	//    angle a its horizontal part is g*sin(a)*cos(a); on flat ground it is zero.
	FVector Base = SlideDirection * SlideBaseVelocity.Size2D();
	if (CurrentFloor.IsWalkableFloor())
	{
		const FVector AlongSlope = FVector::VectorPlaneProject(FVector(0.0, 0.0, GetGravityZ()), CurrentFloor.HitResult.ImpactNormal);
		Base += FVector(AlongSlope.X, AlongSlope.Y, 0.0) * (SlideGravityScale * DeltaTime);
	}

	// 3. Friction, and never slide backwards: if a slope turned us around, we've stopped.
	double BaseSpeed = Base.Size2D();
	if (BaseSpeed > KINDA_SMALL_NUMBER)
	{
		const FVector NewDirection = Base / BaseSpeed;
		if (FVector::DotProduct(NewDirection, SlideDirection) < 0.0)
		{
			BaseSpeed = 0.0;
		}
		else
		{
			SlideDirection = NewDirection.GetSafeNormal2D();
		}
	}
	BaseSpeed = FMath::Clamp(BaseSpeed - SlideFriction * DeltaTime, 0.0, static_cast<double>(SlideMaxSpeed));
	SlideBaseVelocity = SlideDirection * BaseSpeed;

	// 4. Carried boost: held briefly, then fades.
	if (GetWorldTime() - SlideStartTime > SlideMomentumHoldTime)
	{
		SlideBoostSpeed = FMath::Max(0.f, SlideBoostSpeed - SlideMomentumDecay * DeltaTime);
	}

	CurrentSlideSpeed = static_cast<float>(BaseSpeed) + SlideBoostSpeed;
}

void UMegaCharacterMovementComponent::BeginGroundSlideOrCrouch()
{
	if (WantsToSlide())
	{
		if (bIsDashing)
		{
			// Dash -> slide: the dash speed is in the momentum memory, so it carries into the slide.
			EndDash(/*bApplyExitVelocity*/ false);
		}
		StartSlide();
	}
	else if (!bIsDashing)
	{
		StartCrouchHold();
	}
}

void UMegaCharacterMovementComponent::StartSlide()
{
	const float CarriedSpeed = GetCarriedSpeed2D();
	const FVector CarriedDirection = GetCarriedDirection2D();
	const bool bCarryingMomentum = CarriedSpeed > SlideSpeed && !CarriedDirection.IsNearlyZero();

	// With momentum, slide where we were already going; otherwise input, then facing.
	FVector Direction = bCarryingMomentum ? CarriedDirection : GetMoveInputDirection2D();
	if (Direction.IsNearlyZero())
	{
		Direction = !CarriedDirection.IsNearlyZero() ? CarriedDirection : GetFacingDirection2D();
	}

	// Base physics speed starts at the slide kick; anything carried above it is a fading boost.
	const float EntrySpeed = FMath::Clamp(CarriedSpeed, SlideSpeed, FMath::Max(SlideSpeed, SlideMaxEntrySpeed));
	SlideDirection = Direction;
	SlideBaseVelocity = Direction * SlideSpeed;
	SlideBoostSpeed = EntrySpeed - SlideSpeed;
	CurrentSlideSpeed = EntrySpeed;
	SlideStartTime = GetWorldTime();

	if (bIsCrouchHeld)
	{
		bIsCrouchHeld = false; // keep the capsule crouched; the slide reuses it
	}
	bIsSliding = true;
	CharacterOwner->Crouch();
	OnMegaSlideChanged.Broadcast(true);
}

void UMegaCharacterMovementComponent::EndSlide()
{
	bIsSliding = false;
	if (CharacterOwner && !bIsCrouchHeld)
	{
		CharacterOwner->UnCrouch(); // stays crouched automatically if there's a ceiling
	}
	OnMegaSlideChanged.Broadcast(false);
}

void UMegaCharacterMovementComponent::StartCrouchHold()
{
	bIsCrouchHeld = true;
	CharacterOwner->Crouch();
}

void UMegaCharacterMovementComponent::EndCrouchHold()
{
	bIsCrouchHeld = false;
	if (CharacterOwner && !bIsSliding)
	{
		CharacterOwner->UnCrouch();
	}
}

void UMegaCharacterMovementComponent::StartSlam()
{
	if (bIsDashing)
	{
		EndDash(/*bApplyExitVelocity*/ false);
	}
	if (bIsGrappling)
	{
		StopGrapple();
	}

	bIsSlamming = true;
	SlamStartZ = static_cast<float>(UpdatedComponent->GetComponentLocation().Z);
	Velocity = FVector(0.f, 0.f, -SlamSpeed);

	// A slam is a deliberate stop: don't carry pre-slam speed into a slide after landing.
	MomentumSpeed = 0.f;
	MomentumTime = GetWorldTime();
}

void UMegaCharacterMovementComponent::HandleLanded()
{
	WallJumpsRemaining = MaxWallJumps;
	bJumpedSinceGrounded = false;

	if (bIsSlamming)
	{
		bIsSlamming = false;

		const FVector Location = UpdatedComponent->GetComponentLocation();
		LastSlamFallDistance = FMath::Max(0.f, static_cast<float>(SlamStartZ - Location.Z));
		SlamLandTime = GetWorldTime();
		Velocity = FVector::ZeroVector;

		const double HalfHeight = CharacterOwner ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0;
		OnMegaSlamLanded.Broadcast(Location - FVector(0.f, 0.f, HalfHeight), LastSlamFallDistance);
	}
}

// ---------------------------------------------------------------------------------------------
// Grapple
// ---------------------------------------------------------------------------------------------

void UMegaCharacterMovementComponent::StartGrapple(const FVector& WorldPoint, USceneComponent* AnchorComponent)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	if (bIsDashing)
	{
		EndDash(/*bApplyExitVelocity*/ false);
	}
	if (bIsSliding)
	{
		EndSlide();
	}
	if (bIsCrouchHeld)
	{
		EndCrouchHold();
	}
	bIsSlamming = false;

	const FVector ToPoint = WorldPoint - UpdatedComponent->GetComponentLocation();

	if (GrappleMode == EMegaGrappleMode::Burst)
	{
		// One-shot: launch and let normal physics (gravity, air control) take over.
		LaunchJump(ToPoint.GetSafeNormal() * BurstGrappleSpeed + FVector(0.f, 0.f, BurstGrappleUpBoost), EMegaJumpType::Grapple);
		return;
	}

	GrappleAnchor = AnchorComponent;
	GrappleWorldPoint = WorldPoint;
	GrappleLocalPoint = AnchorComponent ? AnchorComponent->GetComponentTransform().InverseTransformPosition(WorldPoint) : WorldPoint;
	bIsGrappling = true;

	if (IsMovingOnGround() && ToPoint.Z > 0.0)
	{
		SetMovementMode(MOVE_Falling);
	}

	OnMegaGrappleChanged.Broadcast(true);
}

void UMegaCharacterMovementComponent::StopGrapple()
{
	if (!bIsGrappling)
	{
		return;
	}
	bIsGrappling = false;
	GrappleAnchor.Reset();
	OnMegaGrappleChanged.Broadcast(false);
}

FVector UMegaCharacterMovementComponent::GetGrapplePoint() const
{
	if (const USceneComponent* Anchor = GrappleAnchor.Get())
	{
		return Anchor->GetComponentTransform().TransformPosition(GrappleLocalPoint);
	}
	return GrappleWorldPoint;
}

void UMegaCharacterMovementComponent::UpdateGrapple(float DeltaTime)
{
	const FVector ToPoint = GetGrapplePoint() - UpdatedComponent->GetComponentLocation();
	const double Distance = ToPoint.Size();
	if (Distance <= GrappleReleaseDistance)
	{
		StopGrapple(); // keep velocity: momentum carries past the anchor
		return;
	}

	const FVector Direction = ToPoint / Distance;
	if (IsMovingOnGround() && Direction.Z > 0.1)
	{
		SetMovementMode(MOVE_Falling);
	}

	// Steer and accelerate toward the anchor at a constant rate, capped at MaxGrappleSpeed.
	Velocity = FMath::VInterpConstantTo(Velocity, Direction * MaxGrappleSpeed, DeltaTime, GrappleAcceleration);
}

// ---------------------------------------------------------------------------------------------
// Walls
// ---------------------------------------------------------------------------------------------

void UMegaCharacterMovementComponent::ApplyWallSlide()
{
	if (!IsFalling() || bIsSlamming || bIsDashing || bIsGrappling || Velocity.Z >= -WallSlideMaxFallSpeed)
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
