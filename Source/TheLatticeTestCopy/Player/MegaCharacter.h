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
class UAnimSequence;
class UMaterialInstanceDynamic;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FInputActionValue;

/** What the forearm weapon (the TOOL) is doing right now. */
UENUM(BlueprintType)
enum class EToolAction : uint8
{
	Idle,
	Firing,
	Reloading
};

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

	/** Loads the TOOL mesh/animations, positions it and starts the idle loop. Safe to call again after changing Tool settings. */
	UFUNCTION(BlueprintCallable, Category="Tool")
	void ApplyToolLayout();

	/** Plays the fire animation and glow flash. Returns false if blocked (reloading or still on cooldown). */
	UFUNCTION(BlueprintCallable, Category="Tool")
	bool ToolFire();

	/** Plays the reload animation. Returns false if already reloading. */
	UFUNCTION(BlueprintCallable, Category="Tool")
	bool ToolReload();

	UFUNCTION(BlueprintPure, Category="Tool")
	EToolAction GetToolAction() const { return ToolAction; }

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

	/** Pivot of the forearm weapon (the TOOL). Attached to the camera; move/rotate it with ToolOffset/ToolRotation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> ToolRoot;

	/** The TOOL itself: one skeletal mesh (front cuff, rear cuff, screen on their own bones). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> ToolMesh;

	//~ TOOL (forearm weapon) ------------------------------------------------------------------------
	// Tweak live: play, press F8 to eject, select the character in the Outliner, edit these.

	UPROPERTY(EditDefaultsOnly, Category="Tool|Assets") TSoftObjectPtr<USkeletalMesh> ToolMeshAsset;
	UPROPERTY(EditDefaultsOnly, Category="Tool|Assets") TSoftObjectPtr<UAnimSequence> ToolIdleAnim;
	UPROPERTY(EditDefaultsOnly, Category="Tool|Assets") TSoftObjectPtr<UAnimSequence> ToolFireAnim;
	UPROPERTY(EditDefaultsOnly, Category="Tool|Assets") TSoftObjectPtr<UAnimSequence> ToolReloadAnim;

	/** Where the TOOL's origin (the attach point in the rear cuff) sits relative to the camera: X forward, Y right, Z up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool", meta=(ForceUnits="cm"))
	FVector ToolOffset = FVector(40.f, 22.f, -23.f);

	/** Yaw -90 turns the barrel (modeled along the mesh's +Y) to face the camera's forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool")
	FRotator ToolRotation = FRotator(0.f, -90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool", meta=(ClampMin="0.01"))
	float ToolScale = 1.f;

	/**
	 * PS1-style stepped animation: how many times per second the TOOL's animation updates.
	 * The weapon still follows the camera smoothly; only its own motion steps. 0 = smooth.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool", meta=(ClampMin="0"))
	float ToolAnimStepRate = 15.f;

	/** Minimum time between shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool", meta=(ClampMin="0", ForceUnits="s"))
	float ToolFireCooldown = 0.2f;

	/** Material scalar parameter that controls the glow (must match the name in M_TOOL). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Glow")
	FName ToolGlowParameter = TEXT("GlowIntensity");

	/** On firing, the glow jumps to this multiple of its normal intensity, then fades back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Glow", meta=(ClampMin="1"))
	float ToolFireGlowBoost = 4.f;

	/** How fast the fire flash fades (full flash -> normal in 1/this seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Glow", meta=(ClampMin="0.1"))
	float ToolGlowFlashDecay = 6.f;

	//~ Input (optional: empty slots are filled by FMegaDefaultInput at runtime) -------------------

	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputMappingContext> DefaultMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> DashAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> SlideAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> GrappleAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> FireAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> ReloadAction;

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
	void Input_Fire(const FInputActionValue& Value);
	void Input_Reload(const FInputActionValue& Value);

	/** Plays Anim on the TOOL (single-node mode, no Anim Blueprint needed). Returns its length in seconds. */
	float PlayToolAnim(UAnimSequence* Anim, bool bLoop);
	void OnToolActionFinished();
	void UpdateToolGlow(float DeltaSeconds);

	UPROPERTY(Transient)
	TObjectPtr<UMegaCharacterMovementComponent> MegaMovement;

	bool bJumpBuffered = false;
	float JumpBufferedAtTime = 0.f;

	float SmoothedEyeHeightAboveFeet = 0.f;
	float CurrentRoll = 0.f;
	float LandingDipOffset = 0.f;
	float LastFallSpeed = 0.f;

	// TOOL runtime state. Loaded assets are kept in UPROPERTYs so the garbage collector doesn't free them.
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedIdleAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedFireAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedReloadAnim;

	/** Per-character copy of the glow material, so code can change its parameters at runtime. */
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ToolGlowMaterial;

	EToolAction ToolAction = EToolAction::Idle;
	FTimerHandle ToolActionTimer;
	float LastToolFireTime = -1000.f;
	float ToolGlowBase = 0.f;   // the material's own GlowIntensity, read at startup
	float ToolGlowFlash = 0.f;  // 1 right after firing, fades to 0
};
