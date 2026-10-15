// First-person player character: input, camera feel, and the custom movement component.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MegaCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMegaCharacterMovementComponent;
class UMegaGrappleComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FInputActionValue;

UCLASS()
class THELATTICETESTCOPY_API AMegaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit AMegaCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category="Mega")
	UMegaCharacterMovementComponent* GetMegaMovement() const { return MegaMovement; }

	UFUNCTION(BlueprintPure, Category="Mega")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category="Mega")
	UMegaGrappleComponent* GetGrapple() const { return Grapple; }

	/** Loads the TOOL meshes and positions every part. Safe to call again after changing the Tool settings. */
	UFUNCTION(BlueprintCallable, Category="Tool")
	void ApplyToolLayout();

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ Components -------------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Arms/weapon mesh, visible only to the owning player. Assign the mesh on a child Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	/** Grapple hook. Its position is where the cable leaves the hand; move it in a child Blueprint's viewport. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UMegaGrappleComponent> Grapple;

	/** Pivot of the forearm weapon (the TOOL). Attached to the camera; all three TOOL parts hang off it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> ToolRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> ToolFrontCuff;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> ToolRearCuff;

	/** The flip-up screen. Its pivot is the hinge, so rotating it opens/closes the screen. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> ToolScreen;

	//~ TOOL (forearm weapon) ------------------------------------------------------------------------
	// Tweak live: play, press F8 to eject, select the character in the Outliner, edit these.

	UPROPERTY(EditDefaultsOnly, Category="Tool") TSoftObjectPtr<UStaticMesh> ToolFrontCuffMesh;
	UPROPERTY(EditDefaultsOnly, Category="Tool") TSoftObjectPtr<UStaticMesh> ToolRearCuffMesh;
	UPROPERTY(EditDefaultsOnly, Category="Tool") TSoftObjectPtr<UStaticMesh> ToolScreenMesh;

	/** Where the TOOL sits relative to the camera: X forward, Y right, Z up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool", meta=(ForceUnits="cm"))
	FVector ToolOffset = FVector(30.1f, 25.8f, -20.6f);

	/** Yaw -90 turns the barrel (modeled along the mesh's +Y) to face the camera's forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool")
	FRotator ToolRotation = FRotator(0.f, -90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool", meta=(ClampMin="0.01"))
	float ToolScale = 0.01f; // the current FBX imports 100x too large; set to 1 after re-exporting at the right scale

	/**
	 * Recenters the parts on the rear cuff's bounds, so it doesn't matter where the model sat in Blender.
	 * Turn off once the meshes are exported with their pivot at the attach point.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool")
	bool bCenterToolOnRearCuff = true;

	/** The screen's hinge position exactly as Blender showed it (N panel > Location), in cm. Converted to Unreal axes at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool", meta=(ForceUnits="cm"))
	FVector ToolScreenHingeBlender = FVector::ZeroVector; // current screen mesh shares the cuffs' origin

	/** Extra rotation around the hinge (e.g. Roll to flip the screen open/closed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool")
	FRotator ToolScreenRotation = FRotator::ZeroRotator;

	//~ Input (optional: empty slots are filled by FMegaDefaultInput at runtime) -------------------

	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputMappingContext> DefaultMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> DashAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> SlideAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> GrappleAction;

	/** Degrees of rotation per raw look-input unit (2.5 matches Unreal's legacy controller scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input", meta=(ClampMin="0.01"))
	float LookSensitivity = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	bool bInvertLookY = false;

	/** A jump pressed this long before it becomes possible (e.g. just before landing) still fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input", meta=(ClampMin="0", ForceUnits="s"))
	float JumpBufferTime = 0.12f;

	//~ Camera feel ------------------------------------------------------------------------------

	/** Eye position measured down from the top of the capsule. Follows the capsule when sliding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ForceUnits="cm"))
	float EyeDepthBelowCapsuleTop = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0"))
	float EyeHeightInterpSpeed = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="30", ClampMax="170", ForceUnits="deg"))
	float BaseFieldOfView = 100.f;

	/** FOV added at high speed to sell velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0", ForceUnits="deg"))
	float MaxSpeedFOVBonus = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ForceUnits="cm/s"))
	float FOVBonusStartSpeed = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ForceUnits="cm/s"))
	float FOVBonusFullSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0"))
	float FOVInterpSpeed = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ForceUnits="deg"))
	float StrafeRollDegrees = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ForceUnits="deg"))
	float SlideRollDegrees = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0"))
	float RollInterpSpeed = 10.f;

	/** Camera dip on landing: cm of dip per cm/s of fall speed, capped by LandingDipMax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0"))
	float LandingDipPerFallSpeed = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0", ForceUnits="cm"))
	float LandingDipMax = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0"))
	float LandingDipRecoverySpeed = 9.f;

private:
	void EnsureInputAssets();
	void UpdateCamera(float DeltaSeconds);

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump(const FInputActionValue& Value);
	void Input_Dash(const FInputActionValue& Value);
	void Input_SlidePressed(const FInputActionValue& Value);
	void Input_SlideReleased(const FInputActionValue& Value);
	void Input_GrapplePressed(const FInputActionValue& Value);
	void Input_GrappleReleased(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UMegaCharacterMovementComponent> MegaMovement;

	bool bJumpBuffered = false;
	float JumpBufferedAtTime = 0.f;

	float SmoothedEyeHeightAboveFeet = 0.f;
	float CurrentRoll = 0.f;
	float LandingDipOffset = 0.f;
	float LastFallSpeed = 0.f;
};
