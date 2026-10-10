// First-person player character: input, camera feel, and the custom movement component.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MegaCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMegaCharacterMovementComponent;
class USkeletalMeshComponent;
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

protected:
	virtual void BeginPlay() override;

	//~ Components -------------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Arms/weapon mesh, visible only to the owning player. Assign the mesh on a child Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	//~ Input (optional: empty slots are filled by FMegaDefaultInput at runtime) -------------------

	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputMappingContext> DefaultMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> DashAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> SlideAction;

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

	UPROPERTY(Transient)
	TObjectPtr<UMegaCharacterMovementComponent> MegaMovement;

	bool bJumpBuffered = false;
	float JumpBufferedAtTime = 0.f;

	float SmoothedEyeHeightAboveFeet = 0.f;
	float CurrentRoll = 0.f;
	float LandingDipOffset = 0.f;
	float LastFallSpeed = 0.f;
};
