#include "Player/MegaGrappleComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInterface.h"
#include "Player/MegaCharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

UMegaGrappleComponent::UMegaGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // after movement + camera, so the rope never lags a frame

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		RopeMesh = CylinderMesh.Object;
	}
}

float UMegaGrappleComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

void UMegaGrappleComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Movement = Cast<UMegaCharacterMovementComponent>(Character->GetCharacterMovement());
	}
	if (Movement)
	{
		Movement->OnMegaGrappleChanged.AddDynamic(this, &UMegaGrappleComponent::HandleGrappleChanged);
	}

	// Created at runtime so it never shows up as a stray component in the editor. It uses absolute
	// transforms: we place it in world space every frame.
	Rope = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("GrappleRope"));
	Rope->SetupAttachment(this);
	Rope->SetUsingAbsoluteLocation(true);
	Rope->SetUsingAbsoluteRotation(true);
	Rope->SetUsingAbsoluteScale(true);
	Rope->SetStaticMesh(RopeMesh);
	if (RopeMaterial)
	{
		Rope->SetMaterial(0, RopeMaterial);
	}
	Rope->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Rope->SetGenerateOverlapEvents(false);
	Rope->SetCastShadow(false);
	Rope->SetCanEverAffectNavigation(false);
	Rope->RegisterComponent();
	Rope->SetVisibility(false);
}

void UMegaGrappleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateRope(DeltaTime);
}

bool UMegaGrappleComponent::IsOnCooldown() const
{
	return GetWorldTime() < CooldownEndTime;
}

void UMegaGrappleComponent::OnGrapplePressed()
{
	if (!Movement)
	{
		return;
	}

	if (Movement->IsGrappling())
	{
		Movement->StopGrapple();
		return;
	}

	if (IsOnCooldown())
	{
		return;
	}

	FHitResult Hit;
	if (!TraceForAnchor(Hit))
	{
		OnGrappleMissed.Broadcast();
		return;
	}

	const FVector AnchorPoint = Hit.ImpactPoint;
	USceneComponent* AnchorComponent = Hit.GetComponent();
	// Only follow components that can actually move; static geometry just uses the world point.
	if (AnchorComponent && AnchorComponent->Mobility != EComponentMobility::Movable)
	{
		AnchorComponent = nullptr;
	}

	// Rope shoots out from the hand toward the anchor.
	RopeTarget = AnchorPoint;
	RopeExtension = 0.f;
	bRopeAttached = true;

	Movement->StartGrapple(AnchorPoint, AnchorComponent);
	OnGrappleFired.Broadcast(AnchorPoint);

	if (Movement->GrappleMode == EMegaGrappleMode::Burst)
	{
		BurstRopeReleaseTime = GetWorldTime() + BurstRopeVisibleTime;
		CooldownEndTime = GetWorldTime() + GrappleCooldown;
	}
}

void UMegaGrappleComponent::OnGrappleReleased()
{
	if (bHoldToGrapple && Movement && Movement->IsGrappling())
	{
		Movement->StopGrapple();
	}
}

void UMegaGrappleComponent::HandleGrappleChanged(bool bIsGrappling)
{
	if (!bIsGrappling)
	{
		bRopeAttached = false; // rope retracts in UpdateRope
		CooldownEndTime = GetWorldTime() + GrappleCooldown;
	}
}

void UMegaGrappleComponent::UpdateRope(float DeltaTime)
{
	if (!Rope)
	{
		return;
	}

	// Burst mode has no grapple state; release the rope after its visible time.
	if (bRopeAttached && BurstRopeReleaseTime > 0.f && GetWorldTime() >= BurstRopeReleaseTime)
	{
		bRopeAttached = false;
		BurstRopeReleaseTime = -1.f;
	}

	// While attached, follow the live anchor (it may be on a moving platform).
	if (bRopeAttached && Movement && Movement->IsGrappling())
	{
		RopeTarget = Movement->GetGrapplePoint();
	}

	const FVector Hand = GetComponentLocation();
	const FVector ToTarget = RopeTarget - Hand;
	const float FullLength = static_cast<float>(ToTarget.Size());

	const float Step = RopeShootSpeed * DeltaTime;
	RopeExtension = bRopeAttached
		? FMath::Min(RopeExtension + Step, FullLength)
		: FMath::Max(RopeExtension - Step, 0.f);

	if (RopeExtension <= 1.f || FullLength <= KINDA_SMALL_NUMBER)
	{
		if (Rope->IsVisible())
		{
			Rope->SetVisibility(false);
		}
		return;
	}

	// Stretch the 100-unit cylinder from the hand along the rope direction.
	const FVector Direction = ToTarget / FullLength;
	const float Length = FMath::Min(RopeExtension, FullLength);
	const float WidthScale = RopeWidth / 100.f;

	Rope->SetWorldLocationAndRotation(Hand + Direction * (Length * 0.5f), FRotationMatrix::MakeFromZ(Direction).Rotator());
	Rope->SetWorldScale3D(FVector(WidthScale, WidthScale, Length / 100.f));
	if (!Rope->IsVisible())
	{
		Rope->SetVisibility(true);
	}
}

bool UMegaGrappleComponent::TraceForAnchor(FHitResult& OutHit) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}

	// Aim from the eyes, not from the hand, so the grapple goes exactly where you're looking.
	FVector EyeLocation;
	FRotator EyeRotation;
	Controller->GetPlayerViewPoint(EyeLocation, EyeRotation);
	const FVector End = EyeLocation + EyeRotation.Vector() * GrappleDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MegaGrappleTrace), /*bTraceComplex*/ true, GetOwner());
	if (AimAssistRadius > 0.f)
	{
		return GetWorld()->SweepSingleByChannel(OutHit, EyeLocation, End, FQuat::Identity, TraceChannel,
			FCollisionShape::MakeSphere(AimAssistRadius), Params);
	}
	return GetWorld()->LineTraceSingleByChannel(OutHit, EyeLocation, End, TraceChannel, Params);
}
