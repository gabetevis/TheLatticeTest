// Player movement: Ultrakill-style dash, slide, ground slam, wall jump, grapple, momentum carry.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MegaCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EMegaJumpType : uint8
{
	Ground,
	Coyote,
	Wall,
	Slide,
	Dash,
	Slam,
	Grapple
};

UENUM(BlueprintType)
enum class EMegaGrappleMode : uint8
{
	/** Continuous pull: velocity steers toward the anchor and accelerates up to MaxGrappleSpeed. */
	Accelerating,
	/** One-shot launch toward the anchor, then normal physics take over. */
	Burst
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaJumpedSignature, EMegaJumpType, JumpType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaDashedSignature, FVector, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaSlideChangedSignature, bool, bIsSliding);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMegaSlamLandedSignature, FVector, ImpactLocation, float, FallDistance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaGrappleChangedSignature, bool, bIsGrappling);

/**
 * Movement component for the player character.
 *
 * Design note: dash, slide, slam and grapple are *states layered on top of* MOVE_Walking /
 * MOVE_Falling rather than custom movement modes. We only override how velocity is calculated
 * (CalcVelocity), gravity (GetGravityZ) and impacts (HandleImpact), so the engine's floor
 * detection, step-up, ledge and slope handling keep working for free.
 *
 * Momentum: the component remembers the highest horizontal speed reached over the last
 * MomentumGraceTime seconds. Entering a slide uses that remembered speed, so dash -> slide,
 * grapple -> slide and air -> slide all carry their speed and then bleed it off.
 *
 * Single-player only: there is no FSavedMove client prediction. If multiplayer ever becomes a
 * goal, these states need to move into compressed flags / saved moves (see ARCHITECTURE.md).
 */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class THELATTICETESTCOPY_API UMegaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UMegaCharacterMovementComponent();

	//~ Actions (called by AMegaCharacter / UMegaGrappleComponent) -----------------------------

	/** Performs the best jump for the current situation. Returns false if no jump was possible, so the caller can buffer it. */
	bool TryJump();

	/** Dashes in the current input direction (or facing direction when there is no input). */
	bool TryDash();

	/**
	 * Crouch/slide/slam button state.
	 *  - In the air: ground slam.
	 *  - On the ground while moving (input or carried speed): slide.
	 *  - On the ground while still: crouch.
	 */
	void SetSlideInputHeld(bool bHeld);

	/** Starts pulling toward WorldPoint. If AnchorComponent is set, the point follows it (moving platforms). */
	void StartGrapple(const FVector& WorldPoint, USceneComponent* AnchorComponent);

	/** Releases the grapple. Velocity is kept, so momentum carries out of it. */
	void StopGrapple();

	/** Current world-space grapple anchor (valid while grappling). */
	FVector GetGrapplePoint() const;

	//~ State queries --------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category="Mega|Movement") bool IsDashing() const { return bIsDashing; }
	UFUNCTION(BlueprintPure, Category="Mega|Movement") bool IsSliding() const { return bIsSliding; }
	UFUNCTION(BlueprintPure, Category="Mega|Movement") bool IsSlamming() const { return bIsSlamming; }
	UFUNCTION(BlueprintPure, Category="Mega|Movement") bool IsCrouchHeld() const { return bIsCrouchHeld; }
	UFUNCTION(BlueprintPure, Category="Mega|Movement") bool IsGrappling() const { return bIsGrappling; }

	/** Dash stamina measured in charges, e.g. 2.4 out of MaxDashCharges. */
	UFUNCTION(BlueprintPure, Category="Mega|Movement") float GetDashStamina() const { return DashStamina; }
	UFUNCTION(BlueprintPure, Category="Mega|Movement") int32 GetWallJumpsRemaining() const { return WallJumpsRemaining; }

	//~ Events (hook up audio, camera shake, VFX, arm animations) ----------------------------

	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaJumpedSignature OnMegaJumped;
	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaDashedSignature OnMegaDashed;
	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaSlideChangedSignature OnMegaSlideChanged;
	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaSlamLandedSignature OnMegaSlamLanded;
	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaGrappleChangedSignature OnMegaGrappleChanged;

	//~ Tuning: Momentum ------------------------------------------------------------------------

	/** How long a burst of speed is remembered for carrying into the next state (e.g. dash -> slide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Momentum", meta=(ClampMin="0", ForceUnits="s"))
	float MomentumGraceTime = 0.25f;

	//~ Tuning: Dash ----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Dash", meta=(ClampMin="0", ForceUnits="cm/s"))
	float DashSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Dash", meta=(ClampMin="0.01", ForceUnits="s"))
	float DashDuration = 0.18f;

	/** Horizontal speed kept when a dash ends naturally. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Dash", meta=(ClampMin="0", ForceUnits="cm/s"))
	float DashExitSpeed = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Dash", meta=(ClampMin="1"))
	int32 MaxDashCharges = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Dash", meta=(ClampMin="0"))
	float DashChargesRegenPerSecond = 0.9f;

	/** Jumping during a grounded dash launches with this horizontal speed (the "dash jump"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Dash", meta=(ClampMin="0", ForceUnits="cm/s"))
	float DashJumpHorizontalSpeed = 2600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Dash", meta=(ClampMin="0"))
	float DashJumpVerticalScale = 0.75f;

	/** Extra stamina a dash jump costs. Without enough stamina it becomes a normal jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Dash", meta=(ClampMin="0"))
	float DashJumpStaminaCost = 1.f;

	//~ Tuning: Slide / crouch ------------------------------------------------------------------
	//
	// A slide is real physics on the floor it's on: it starts with a kick (SlideSpeed), loses
	// speed to friction, and gravity along the slope speeds it up downhill / slows it uphill.
	// Speed carried in from a dash or grapple sits on top as a boost that holds briefly, then fades.
	// When the slide runs out of speed it turns into a crouch (until the button is pressed again).

	/** Starting speed of a slide from a standstill or walk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlideSpeed = 1700.f;

	/** Constant slowdown while sliding. With the default gravity, slopes steeper than ~12 degrees overcome it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="cm/s^2"))
	float SlideFriction = 450.f;

	/** How strongly slopes speed up / slow down a slide. 1 = real gravity (incl. GravityScale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0"))
	float SlideGravityScale = 1.f;

	/** Slides can't get faster than this from slopes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlideMaxSpeed = 5000.f;

	/** Below this speed the slide ends and becomes a crouch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlideMinSpeed = 350.f;

	/** Cap on how much carried speed (dash, grapple, fall) a slide can start with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlideMaxEntrySpeed = 3200.f;

	/** Carried speed above SlideSpeed is held this long before it starts fading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="s"))
	float SlideMomentumHoldTime = 0.3f;

	/** How fast carried speed fades after the hold time (on top of friction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="cm/s^2"))
	float SlideMomentumDecay = 2000.f;

	/** Without movement input, the slide button only slides when moving faster than this; otherwise it crouches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlideMinSpeedWithoutInput = 1300.f;

	/** How fast input can bend the slide direction. 0 = locked direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="deg/s"))
	float SlideSteerDegreesPerSecond = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0"))
	float SlideJumpVerticalScale = 0.85f;

	//~ Tuning: Ground slam ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slam", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlamSpeed = 4500.f;

	/** Jumping within this time after a slam landing performs a slam jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slam", meta=(ClampMin="0", ForceUnits="s"))
	float SlamJumpWindow = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slam", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlamJumpBaseVelocity = 1400.f;

	/** Extra slam-jump velocity per cm fallen during the slam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slam", meta=(ClampMin="0"))
	float SlamJumpFallDistanceScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slam", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlamJumpMaxVelocity = 2800.f;

	//~ Tuning: Walls ---------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Wall", meta=(ClampMin="0"))
	int32 MaxWallJumps = 3;

	/** How far beyond the capsule radius walls are detected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Wall", meta=(ClampMin="0", ForceUnits="cm"))
	float WallCheckDistance = 30.f;

	/** Surfaces with |Normal.Z| above this are floors/ceilings, not walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Wall", meta=(ClampMin="0", ClampMax="1"))
	float WallMaxNormalZ = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Wall", meta=(ClampMin="0", ForceUnits="cm/s"))
	float WallJumpOutSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Wall", meta=(ClampMin="0", ForceUnits="cm/s"))
	float WallJumpUpSpeed = 1000.f;

	/** Extra velocity in the input direction on wall jump, so you can steer off walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Wall", meta=(ClampMin="0", ForceUnits="cm/s"))
	float WallJumpInputInfluence = 400.f;

	/** Max fall speed while pushing into a wall in the air (wall cling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Wall", meta=(ClampMin="0", ForceUnits="cm/s"))
	float WallSlideMaxFallSpeed = 350.f;

	//~ Tuning: Grapple -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple")
	EMegaGrappleMode GrappleMode = EMegaGrappleMode::Accelerating;

	/** Accelerating mode: how quickly velocity turns toward and speeds up along the rope. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0", ForceUnits="cm/s^2"))
	float GrappleAcceleration = 7000.f;

	/** Accelerating mode: top pull speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0", ForceUnits="cm/s"))
	float MaxGrappleSpeed = 3500.f;

	/** Accelerating mode: release automatically this close to the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0", ForceUnits="cm"))
	float GrappleReleaseDistance = 200.f;

	/** Burst mode: launch speed toward the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0", ForceUnits="cm/s"))
	float BurstGrappleSpeed = 2800.f;

	/** Burst mode: extra upward velocity so short pulls arc instead of skimming the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0", ForceUnits="cm/s"))
	float BurstGrappleUpBoost = 450.f;

	/** Jumping while grappling releases the rope with this much of a normal jump added on top. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0"))
	float GrappleJumpVerticalScale = 0.7f;

	//~ Tuning: Jump forgiveness ----------------------------------------------------------------

	/** Grace period after walking off a ledge during which a ground jump is still allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Jump", meta=(ClampMin="0", ForceUnits="s"))
	float CoyoteTime = 0.12f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual float GetGravityZ() const override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

private:
	void StartDash(const FVector& Direction);
	void EndDash(bool bApplyExitVelocity);
	void StartSlide();
	void EndSlide();
	void StartCrouchHold();
	void EndCrouchHold();
	void StartSlam();
	void HandleLanded();
	void LaunchJump(const FVector& NewVelocity, EMegaJumpType JumpType);
	void ApplyWallSlide();
	void UpdateGrapple(float DeltaTime);
	void UpdateMomentumMemory();

	void UpdateSlide(float DeltaTime);

	/** Slide if moving (input or carried speed), crouch otherwise. Ground only. */
	void BeginGroundSlideOrCrouch();
	bool WantsToSlide() const;

	/** Speed/direction to carry into a new state: the remembered burst if recent, else current. */
	float GetCarriedSpeed2D() const;
	FVector GetCarriedDirection2D() const;

	/** Finds the closest wall around the capsule. */
	bool FindWall(FHitResult& OutHit) const;

	/** Current movement input flattened to the horizontal plane (zero if none). */
	FVector GetMoveInputDirection2D() const;
	FVector GetFacingDirection2D() const;
	float GetWorldTime() const;

	bool bIsDashing = false;
	bool bIsSliding = false;
	bool bIsSlamming = false;
	bool bIsCrouchHeld = false;
	bool bIsGrappling = false;
	bool bSlideInputHeld = false;
	bool bJumpedSinceGrounded = false;

	float DashStamina = 0.f;
	float DashTimeRemaining = 0.f;
	FVector DashDirection = FVector::ForwardVector;

	/** Slide velocity from friction + slope physics (horizontal). Its direction is the slide direction. */
	FVector SlideBaseVelocity = FVector::ZeroVector;
	/** Carried speed on top of the base, fading after SlideMomentumHoldTime. */
	float SlideBoostSpeed = 0.f;
	FVector SlideDirection = FVector::ForwardVector;
	float CurrentSlideSpeed = 0.f;
	float SlideStartTime = 0.f;
	/** Set when a slide runs out of speed while held; blocks re-sliding until the button is released. */
	bool bSlideExhausted = false;

	float MomentumSpeed = 0.f;
	FVector MomentumDirection = FVector::ForwardVector;
	float MomentumTime = -1000.f;

	TWeakObjectPtr<USceneComponent> GrappleAnchor;
	FVector GrappleLocalPoint = FVector::ZeroVector;
	FVector GrappleWorldPoint = FVector::ZeroVector;

	int32 WallJumpsRemaining = 0;
	float LastGroundedTime = -1000.f;
	float SlamStartZ = 0.f;
	float SlamLandTime = -1000.f;
	float LastSlamFallDistance = 0.f;
};
