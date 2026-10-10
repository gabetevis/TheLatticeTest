// Player movement: Ultrakill-style dash, slide, ground slam, wall jump, coyote time.

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
	Slam
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaJumpedSignature, EMegaJumpType, JumpType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaDashedSignature, FVector, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaSlideChangedSignature, bool, bIsSliding);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMegaSlamLandedSignature, FVector, ImpactLocation, float, FallDistance);

/**
 * Movement component for the player character.
 *
 * Design note: dash, slide and slam are *states layered on top of* MOVE_Walking / MOVE_Falling
 * rather than custom movement modes. We only override how velocity is calculated (CalcVelocity)
 * and gravity (GetGravityZ), so the engine's floor detection, step-up, ledge and slope handling
 * keep working for free.
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

	//~ Actions (called by AMegaCharacter from input) ------------------------------------------

	/** Performs the best jump for the current situation. Returns false if no jump was possible, so the caller can buffer it. */
	bool TryJump();

	/** Dashes in the current input direction (or facing direction when there is no input). */
	bool TryDash();

	/** Slide/slam button state. Pressed on the ground = slide, pressed in the air = ground slam. */
	void SetSlideInputHeld(bool bHeld);

	//~ State queries --------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category="Mega|Movement") bool IsDashing() const { return bIsDashing; }
	UFUNCTION(BlueprintPure, Category="Mega|Movement") bool IsSliding() const { return bIsSliding; }
	UFUNCTION(BlueprintPure, Category="Mega|Movement") bool IsSlamming() const { return bIsSlamming; }

	/** Dash stamina measured in charges, e.g. 2.4 out of MaxDashCharges. */
	UFUNCTION(BlueprintPure, Category="Mega|Movement") float GetDashStamina() const { return DashStamina; }
	UFUNCTION(BlueprintPure, Category="Mega|Movement") int32 GetWallJumpsRemaining() const { return WallJumpsRemaining; }

	//~ Events (hook up audio, camera shake, VFX, arm animations) ----------------------------

	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaJumpedSignature OnMegaJumped;
	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaDashedSignature OnMegaDashed;
	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaSlideChangedSignature OnMegaSlideChanged;
	UPROPERTY(BlueprintAssignable, Category="Mega|Movement") FMegaSlamLandedSignature OnMegaSlamLanded;

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

	//~ Tuning: Slide ---------------------------------------------------------------------------

	/** Slides run at constant speed and do not decelerate while held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SlideSpeed = 1700.f;

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

private:
	void StartDash(const FVector& Direction);
	void EndDash(bool bApplyExitVelocity);
	void StartSlide();
	void EndSlide();
	void StartSlam();
	void HandleLanded();
	void LaunchJump(const FVector& NewVelocity, EMegaJumpType JumpType);
	void ApplyWallSlide();

	/** Finds the closest wall around the capsule. */
	bool FindWall(FHitResult& OutHit) const;

	/** Current movement input flattened to the horizontal plane (zero if none). */
	FVector GetMoveInputDirection2D() const;
	FVector GetFacingDirection2D() const;
	float GetWorldTime() const;

	bool bIsDashing = false;
	bool bIsSliding = false;
	bool bIsSlamming = false;
	bool bSlideInputHeld = false;
	bool bJumpedSinceGrounded = false;

	float DashStamina = 0.f;
	float DashTimeRemaining = 0.f;
	FVector DashDirection = FVector::ForwardVector;
	FVector SlideDirection = FVector::ForwardVector;

	int32 WallJumpsRemaining = 0;
	float LastGroundedTime = -1000.f;
	float SlamStartZ = 0.f;
	float SlamLandTime = -1000.f;
	float LastSlamFallDistance = 0.f;
};
