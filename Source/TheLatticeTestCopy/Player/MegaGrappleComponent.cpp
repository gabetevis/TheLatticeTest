#include "Player/MegaGrappleComponent.h"

#include "CableComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Player/MegaCharacterMovementComponent.h"

UMegaGrappleComponent::UMegaGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // after movement, so the cable doesn't lag a frame
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

	// Cable is created at runtime so it never shows up as a stray component in the editor.
	Cable = NewObject<UCableComponent>(GetOwner(), TEXT("GrappleCable"));
	Cable->SetupAttachment(this);
	Cable->NumSegments = 1;            // straight line: no sag simulation
	Cable->CableLength = 0.f;
	Cable->CableWidth = CableWidth;
	Cable->bAttachStart = true;
	Cable->bAttachEnd = true;          // end is EndLocation in the cable's local space (updated every tick)
	Cable->SetCastShadow(false);
	if (CableMaterial)
	{
		Cable->SetMaterial(0, CableMaterial);
	}
	Cable->RegisterComponent();
	HideCable();
}

void UMegaGrappleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Movement && Movement->IsGrappling())
	{
		ShowCable(Movement->GetGrapplePoint());
	}
	else if (BurstCableHideTime > 0.f)
	{
		if (GetWorldTime() < BurstCableHideTime)
		{
			ShowCable(BurstCablePoint);
		}
		else
		{
			BurstCableHideTime = -1.f;
			HideCable();
		}
	}
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

	Movement->StartGrapple(AnchorPoint, AnchorComponent);
	OnGrappleFired.Broadcast(AnchorPoint);

	if (Movement->GrappleMode == EMegaGrappleMode::Burst)
	{
		BurstCablePoint = AnchorPoint;
		BurstCableHideTime = GetWorldTime() + BurstCableVisibleTime;
		CooldownEndTime = GetWorldTime() + GrappleCooldown;
		ShowCable(AnchorPoint);
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
		HideCable();
		CooldownEndTime = GetWorldTime() + GrappleCooldown;
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

	// Aim from the eyes, not from the hand, so the grapple goes where the crosshair would be.
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

void UMegaGrappleComponent::ShowCable(const FVector& WorldPoint)
{
	if (!Cable)
	{
		return;
	}
	Cable->EndLocation = Cable->GetComponentTransform().InverseTransformPosition(WorldPoint);
	if (!Cable->IsVisible())
	{
		Cable->SetVisibility(true);
		Cable->SetComponentTickEnabled(true);
	}
}

void UMegaGrappleComponent::HideCable()
{
	if (Cable)
	{
		Cable->SetVisibility(false);
		Cable->SetComponentTickEnabled(false);
	}
}
