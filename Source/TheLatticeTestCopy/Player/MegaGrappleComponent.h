// Grapple hook: aiming, rope visuals and cooldown. The pull itself lives in UMegaCharacterMovementComponent.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "MegaGrappleComponent.generated.h"

class UMaterialInterface;
class UMegaCharacterMovementComponent;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMegaGrappleFiredSignature, FVector, AnchorLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMegaGrappleMissedSignature);

/**
 * Ported from the old BP_FirstPersonCharacter grapple (F key, trace, rope, cooldown,
 * Burst / Accelerating pull, release on hit).
 *
 * Attach to the first-person camera: this component's location is where the rope starts
 * (the "hand"). Pull physics and tuning (mode, speed, acceleration) are on the movement
 * component under Mega|Grapple, so all movement numbers live in one place.
 *
 * The rope is a straight mesh (a cylinder by default) stretched from the hand to the anchor
 * every frame after movement, so it always lines up exactly. It shoots out when fired and
 * snaps back when released; that animation is visual only, the pull starts immediately.
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

	//~ Rope look -------------------------------------------------------------------------------

	/** Mesh stretched along the rope. Must be 100 units long on its Z axis with the pivot at its center (like the engine Cylinder). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple|Rope")
	TObjectPtr<UStaticMesh> RopeMesh;

	/** Empty = the mesh's own material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple|Rope")
	TObjectPtr<UMaterialInterface> RopeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple|Rope", meta=(ClampMin="0.1", ForceUnits="cm"))
	float RopeWidth = 3.f;

	/** How fast the rope tip travels out when fired and back when released. Visual only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple|Rope", meta=(ClampMin="1", ForceUnits="cm/s"))
	float RopeShootSpeed = 25000.f;

	/** Burst mode: how long the rope stays attached (visually) after the launch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mega|Grapple|Rope", meta=(ClampMin="0", ForceUnits="s"))
	float BurstRopeVisibleTime = 0.2f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleGrappleChanged(bool bIsGrappling);

	bool TraceForAnchor(FHitResult& OutHit) const;
	void UpdateRope(float DeltaTime);
	float GetWorldTime() const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Rope;

	UPROPERTY(Transient)
	TObjectPtr<UMegaCharacterMovementComponent> Movement;

	/** Where the rope is heading (live anchor while grappling, last anchor while retracting). */
	FVector RopeTarget = FVector::ZeroVector;
	/** Current rope length, animated toward the full distance (out) or zero (back). */
	float RopeExtension = 0.f;
	bool bRopeAttached = false;
	float BurstRopeReleaseTime = -1.f;
	float CooldownEndTime = -1.f;
};
