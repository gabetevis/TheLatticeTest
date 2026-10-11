// Grapple hook: aiming, cable visuals and cooldown. The pull itself lives in UMegaCharacterMovementComponent.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "MegaGrappleComponent.generated.h"

class UCableComponent;
class UMaterialInterface;
class UMegaCharacterMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaGrappleFiredSignature, FVector, AnchorLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMegaGrappleMissedSignature);

/**
 * Ported from the old BP_FirstPersonCharacter grapple (F key, trace, cable, cooldown,
 * Burst / Accelerating pull, release on hit).
 *
 * Attach to the first-person camera: this component's location is where the cable starts
 * (the "hand"). Pull physics and tuning (mode, speed, acceleration) are on the movement
 * component under Mega|Grapple, so all movement numbers live in one place.
 */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class THELATTICETESTCOPY_API UMegaGrappleComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UMegaGrappleComponent();

	/** Grapple button pressed: fire if ready, or release if already attached. */
	void OnGrapplePressed();

	/** Grapple button released: releases the rope when bHoldToGrapple is on. */
	void OnGrappleReleased();

	UFUNCTION(BlueprintPure, Category="Mega|Grapple")
	bool IsOnCooldown() const;

	UPROPERTY(BlueprintAssignable, Category="Mega|Grapple") FMegaGrappleFiredSignature OnGrappleFired;
	UPROPERTY(BlueprintAssignable, Category="Mega|Grapple") FMegaGrappleMissedSignature OnGrappleMissed;

	//~ Targeting -------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0", ForceUnits="cm"))
	float GrappleDistance = 4000.f;

	/** Sphere-trace radius: makes aiming forgiving. 0 = exact line trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0", ForceUnits="cm"))
	float AimAssistRadius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Seconds after release before the grapple can fire again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple", meta=(ClampMin="0", ForceUnits="s"))
	float GrappleCooldown = 0.4f;

	/** On: hold the button to stay attached. Off: press to attach, press again (or jump) to release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple")
	bool bHoldToGrapple = false;

	//~ Cable look ------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple|Cable")
	TObjectPtr<UMaterialInterface> CableMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple|Cable", meta=(ClampMin="0.1", ForceUnits="cm"))
	float CableWidth = 3.f;

	/** Burst mode: how long the cable stays visible after the launch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple|Cable", meta=(ClampMin="0", ForceUnits="s"))
	float BurstCableVisibleTime = 0.2f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleGrappleChanged(bool bIsGrappling);

	bool TraceForAnchor(FHitResult& OutHit) const;
	void ShowCable(const FVector& WorldPoint);
	void HideCable();
	float GetWorldTime() const;

	UPROPERTY(Transient)
	TObjectPtr<UCableComponent> Cable;

	UPROPERTY(Transient)
	TObjectPtr<UMegaCharacterMovementComponent> Movement;

	FVector BurstCablePoint = FVector::ZeroVector;
	float BurstCableHideTime = -1.f;
	float CooldownEndTime = -1.f;
};
