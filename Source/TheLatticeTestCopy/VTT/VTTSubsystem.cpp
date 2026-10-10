#include "VTT/VTTSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "VTT/VTTComponent.h"
#include "VTT/VTTSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogVTT, Log, All);

static TAutoConsoleVariable<bool> CVarVTTDebug(
	TEXT("vtt.Debug"),
	false,
	TEXT("Draw every registered VTT frame. Green = perceivable, red = not."));

static FAutoConsoleCommandWithWorld GVTTForceAmbientCommand(
	TEXT("vtt.ForceAmbient"),
	TEXT("Show a VTT ambient callout now, ignoring chance and cooldowns."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UVTTSubsystem* Subsystem = World ? World->GetSubsystem<UVTTSubsystem>() : nullptr)
		{
			Subsystem->ForceAmbientCallout();
		}
	}));

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

UVTTSubsystem* UVTTSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UVTTSubsystem>() : nullptr;
}

bool UVTTSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVTTSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UVTTSettings* Settings = UVTTSettings::Get();
	bAmbientEnabled = Settings->bAmbientEnabled;
	AppearSound = Settings->AppearSound.LoadSynchronous();
	DismissSound = Settings->DismissSound.LoadSynchronous();
	ResetAmbientTimer();
}

void UVTTSubsystem::Deinitialize()
{
	Registered.Reset();
	ActiveCallouts.Reset();
	PendingRequests.Reset();
	Super::Deinitialize();
}

TStatId UVTTSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVTTSubsystem, STATGROUP_Tickables);
}

void UVTTSubsystem::RegisterComponent(UVTTComponent* Component)
{
	if (Component)
	{
		Registered.AddUnique(Component);
	}
}

void UVTTSubsystem::UnregisterComponent(UVTTComponent* Component)
{
	Registered.RemoveAll([Component](const TWeakObjectPtr<UVTTComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Component;
	});
	// Active callouts keep a weak pointer and animate out at the last known position.
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void UVTTSubsystem::Tick(float DeltaTime)
{
	APlayerController* Controller = nullptr;
	FVector ViewLocation;
	if (!GetView(Controller, ViewLocation))
	{
		return;
	}

	UpdateActiveCallouts(DeltaTime, Controller, ViewLocation);
	UpdatePendingRequests(DeltaTime, Controller, ViewLocation);
	UpdateAmbient(DeltaTime);

	if (CVarVTTDebug.GetValueOnGameThread())
	{
		DrawDebug(Controller, ViewLocation);
	}
}

void UVTTSubsystem::UpdateActiveCallouts(float DeltaTime, const APlayerController* Controller, const FVector& ViewLocation)
{
	const UVTTSettings* Settings = UVTTSettings::Get();

	for (int32 i = ActiveCallouts.Num() - 1; i >= 0; --i)
	{
		FVTTActiveCallout& Callout = ActiveCallouts[i];
		Callout.Age += DeltaTime;

		if (Callout.bDismissing)
		{
			if (Callout.Age - Callout.DismissStartAge >= Settings->OutroDuration)
			{
				ActiveCallouts.RemoveAt(i);
			}
			continue;
		}

		const UVTTComponent* Component = Callout.Source.Get();
		if (Component)
		{
			const FBox Bounds = Component->GetVTTWorldBounds();
			if (Bounds.IsValid)
			{
				Callout.LastWorldBounds = Bounds;
			}
		}

		const bool bPerceived = Component && IsPerceivableFrom(Component, Controller, ViewLocation);
		Callout.TimeUnperceived = bPerceived ? 0.f : Callout.TimeUnperceived + DeltaTime;

		const float VisibleEnd = Settings->BoxIntroDuration + Settings->LineIntroDuration + Callout.TypeDuration + Callout.HoldDuration;
		if (!Component || Callout.TimeUnperceived > Settings->LoseTargetGraceTime || Callout.Age >= VisibleEnd)
		{
			BeginDismiss(Callout);
		}
	}
}

void UVTTSubsystem::UpdatePendingRequests(float DeltaTime, const APlayerController* Controller, const FVector& ViewLocation)
{
	const int32 MaxSlots = UVTTSettings::Get()->MaxConcurrentCallouts;

	// Front to back, so earlier requests get priority.
	for (int32 i = 0; i < PendingRequests.Num();)
	{
		FPendingRequest& Request = PendingRequests[i];
		Request.Age += DeltaTime;

		UVTTComponent* Component = Request.Component.Get();
		const bool bTimedOut = Request.Params.bWaitUntilVisible && Request.Params.Timeout > 0.f && Request.Age > Request.Params.Timeout;
		if (!Component || bTimedOut)
		{
			PendingRequests.RemoveAt(i);
			continue;
		}

		const bool bTargetReady = !IsShowing(Component) &&
			(!Request.Params.bWaitUntilVisible || IsPerceivableFrom(Component, Controller, ViewLocation));
		if (!bTargetReady)
		{
			++i;
			continue;
		}

		if (NumOccupiedSlots() >= MaxSlots)
		{
			if (!Request.Params.bInterruptActive)
			{
				++i;
				continue;
			}
			DismissOldest();
		}

		const int32 EntryIndex = Request.EntryIndex;
		PendingRequests.RemoveAt(i); // Request reference is invalid after this line.
		ShowCallout(Component, EntryIndex);
	}
}

void UVTTSubsystem::UpdateAmbient(float DeltaTime)
{
	if (!bAmbientEnabled)
	{
		return;
	}

	AmbientTimer -= DeltaTime;
	if (AmbientTimer > 0.f)
	{
		return;
	}

	ResetAmbientTimer();
	if (FMath::FRand() <= UVTTSettings::Get()->AmbientChance)
	{
		TryShowAmbient(/*bForce*/ false);
	}
}

// ---------------------------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------------------------

bool UVTTSubsystem::RequestCallout(UVTTComponent* Component, FName EntryId, const FVTTRequestParams& Params)
{
	if (!Component || !Component->bVTTEnabled || Component->Entries.Num() == 0)
	{
		return false;
	}

	const int32 EntryIndex = EntryId.IsNone() ? Component->PickEntry(/*bForAmbient*/ false) : Component->FindEntryIndex(EntryId);
	if (EntryIndex == INDEX_NONE)
	{
		UE_LOG(LogVTT, Warning, TEXT("VTT request on %s: no entry with Id '%s'."), *GetNameSafe(Component->GetOwner()), *EntryId.ToString());
		return false;
	}

	FPendingRequest& Request = PendingRequests.AddDefaulted_GetRef();
	Request.Component = Component;
	Request.EntryIndex = EntryIndex;
	Request.Params = Params;
	return true;
}

bool UVTTSubsystem::ForceAmbientCallout()
{
	return TryShowAmbient(/*bForce*/ true);
}

bool UVTTSubsystem::TryShowAmbient(bool bForce)
{
	APlayerController* Controller = nullptr;
	FVector ViewLocation;
	if (!GetView(Controller, ViewLocation))
	{
		return false;
	}

	const UVTTSettings* Settings = UVTTSettings::Get();
	const float Now = GetWorld()->GetTimeSeconds();

	if (!bForce)
	{
		if (NumOccupiedSlots() >= Settings->MaxConcurrentCallouts || Now - LastAmbientTime < Settings->AmbientGlobalCooldown)
		{
			return false;
		}
	}

	const FVector ViewDirection = Controller->PlayerCameraManager->GetCameraRotation().Vector();

	struct FCandidate
	{
		UVTTComponent* Component;
		int32 EntryIndex;
		float Weight;
	};
	TArray<FCandidate, TInlineAllocator<16>> Candidates;
	float TotalWeight = 0.f;

	for (const TWeakObjectPtr<UVTTComponent>& Weak : Registered)
	{
		UVTTComponent* Component = Weak.Get();
		if (!Component || !Component->bVTTEnabled || IsShowing(Component))
		{
			continue;
		}
		if (!bForce && Component->IsOnAmbientCooldown(Now))
		{
			continue;
		}

		const int32 EntryIndex = Component->PickEntry(/*bForAmbient*/ true);
		if (EntryIndex == INDEX_NONE || !IsPerceivableFrom(Component, Controller, ViewLocation))
		{
			continue;
		}

		// Favour what the player is actually looking at: weight rises sharply toward screen center.
		const FVector ToTarget = (Component->GetVTTWorldBounds().GetCenter() - ViewLocation).GetSafeNormal();
		const float Facing = static_cast<float>(FVector::DotProduct(ViewDirection, ToTarget));
		const float Centrality = FMath::Pow(FMath::Max(0.f, Facing), 8.f);
		const float Weight = FMath::Max(Component->AmbientWeight, 0.01f) * (0.1f + Centrality);

		Candidates.Add(FCandidate{ Component, EntryIndex, Weight });
		TotalWeight += Weight;
	}

	if (Candidates.Num() == 0)
	{
		return false;
	}

	float Roll = FMath::FRand() * TotalWeight;
	const FCandidate* Chosen = &Candidates.Last();
	for (const FCandidate& Candidate : Candidates)
	{
		Roll -= Candidate.Weight;
		if (Roll <= 0.f)
		{
			Chosen = &Candidate;
			break;
		}
	}

	if (NumOccupiedSlots() >= Settings->MaxConcurrentCallouts)
	{
		DismissOldest(); // only reachable when forced
	}

	LastAmbientTime = Now;
	ShowCallout(Chosen->Component, Chosen->EntryIndex);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Callout lifetime
// ---------------------------------------------------------------------------------------------

void UVTTSubsystem::ShowCallout(UVTTComponent* Component, int32 EntryIndex)
{
	if (!Component || !Component->Entries.IsValidIndex(EntryIndex))
	{
		return;
	}

	const UVTTSettings* Settings = UVTTSettings::Get();
	const FVTTEntry& Entry = Component->Entries[EntryIndex];
	const FName EntryId = Entry.Id;
	const int32 NumChars = Entry.Label.ToString().Len() + Entry.Body.ToString().Len();

	FVTTActiveCallout& Callout = ActiveCallouts.AddDefaulted_GetRef();
	Callout.Id = NextCalloutId++;
	Callout.Source = Component;
	Callout.EntryIndex = EntryIndex;
	Callout.Label = Entry.Label;
	Callout.Body = Entry.Body;
	Callout.LastWorldBounds = Component->GetVTTWorldBounds();
	Callout.TypeDuration = NumChars / FMath::Max(1.f, Settings->CharactersPerSecond);
	Callout.HoldDuration = Entry.HoldDurationOverride > 0.f
		? Entry.HoldDurationOverride
		: FMath::Min(Settings->HoldBaseDuration + NumChars * Settings->HoldPerCharacter, Settings->HoldMaxDuration);

	PlaySound(AppearSound);

	// Broadcast last: listeners may issue new requests.
	Component->NotifyEntryShown(EntryIndex, GetWorld()->GetTimeSeconds());
	OnCalloutShown.Broadcast(Component, EntryId);
}

void UVTTSubsystem::BeginDismiss(FVTTActiveCallout& Callout)
{
	if (!Callout.bDismissing)
	{
		Callout.bDismissing = true;
		Callout.DismissStartAge = Callout.Age;
		PlaySound(DismissSound);
	}
}

void UVTTSubsystem::DismissOldest()
{
	FVTTActiveCallout* Oldest = nullptr;
	for (FVTTActiveCallout& Callout : ActiveCallouts)
	{
		if (!Callout.bDismissing && (!Oldest || Callout.Age > Oldest->Age))
		{
			Oldest = &Callout;
		}
	}
	if (Oldest)
	{
		BeginDismiss(*Oldest);
	}
}

void UVTTSubsystem::DismissAll(bool bImmediate)
{
	PendingRequests.Reset();

	if (bImmediate)
	{
		ActiveCallouts.Reset();
		return;
	}
	for (FVTTActiveCallout& Callout : ActiveCallouts)
	{
		BeginDismiss(Callout);
	}
}

// ---------------------------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------------------------

bool UVTTSubsystem::GetView(APlayerController*& OutController, FVector& OutViewLocation) const
{
	UWorld* World = GetWorld();
	OutController = World ? World->GetFirstPlayerController() : nullptr;
	if (!OutController || !OutController->PlayerCameraManager)
	{
		return false;
	}
	OutViewLocation = OutController->PlayerCameraManager->GetCameraLocation();
	return true;
}

bool UVTTSubsystem::IsPerceivable(const UVTTComponent* Component) const
{
	APlayerController* Controller = nullptr;
	FVector ViewLocation;
	return GetView(Controller, ViewLocation) && IsPerceivableFrom(Component, Controller, ViewLocation);
}

bool UVTTSubsystem::IsPerceivableFrom(const UVTTComponent* Component, const APlayerController* Controller, const FVector& ViewLocation) const
{
	if (!Component || !Component->bVTTEnabled || !Controller)
	{
		return false;
	}

	const UVTTSettings* Settings = UVTTSettings::Get();
	const FBox Bounds = Component->GetVTTWorldBounds();
	if (!Bounds.IsValid)
	{
		return false;
	}

	// Distance to the nearest point of the box, so huge structures count when you're near any part.
	const float MaxDistance = Component->MaxDistance > 0.f ? Component->MaxDistance : Settings->DefaultMaxDistance;
	if (Bounds.ComputeSquaredDistanceToPoint(ViewLocation) > FMath::Square(MaxDistance))
	{
		return false;
	}

	const FVector Center = Bounds.GetCenter();
	FVector2D ScreenPosition;
	if (!Controller->ProjectWorldLocationToScreen(Center, ScreenPosition, false))
	{
		return false; // behind the camera
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	Controller->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return false;
	}

	const float MarginX = ViewportWidth * Settings->ScreenEdgeMargin;
	const float MarginY = ViewportHeight * Settings->ScreenEdgeMargin;
	if (ScreenPosition.X < MarginX || ScreenPosition.X > ViewportWidth - MarginX ||
		ScreenPosition.Y < MarginY || ScreenPosition.Y > ViewportHeight - MarginY)
	{
		return false;
	}

	if (Component->bRequireLineOfSight)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VTTLineOfSight), /*bTraceComplex*/ true);
		Params.AddIgnoredActor(Component->GetOwner());
		if (const APawn* Pawn = Controller->GetPawn())
		{
			Params.AddIgnoredActor(Pawn);
		}

		// Center, plus a point near the top (enemies behind waist-high cover still count).
		const FVector Targets[] = { Center, FMath::Lerp(Center, FVector(Center.X, Center.Y, Bounds.Max.Z), 0.75f) };
		bool bAnyVisible = false;
		for (const FVector& Target : Targets)
		{
			if (!GetWorld()->LineTraceTestByChannel(ViewLocation, Target, Settings->LineOfSightChannel, Params))
			{
				bAnyVisible = true;
				break;
			}
		}
		if (!bAnyVisible)
		{
			return false;
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

int32 UVTTSubsystem::NumOccupiedSlots() const
{
	int32 Count = 0;
	for (const FVTTActiveCallout& Callout : ActiveCallouts)
	{
		Count += Callout.bDismissing ? 0 : 1;
	}
	return Count;
}

bool UVTTSubsystem::IsShowing(const UVTTComponent* Component) const
{
	return ActiveCallouts.ContainsByPredicate([Component](const FVTTActiveCallout& Callout)
	{
		return Callout.Source.Get() == Component;
	});
}

void UVTTSubsystem::ResetAmbientTimer()
{
	const UVTTSettings* Settings = UVTTSettings::Get();
	AmbientTimer = FMath::Max(0.1f, Settings->AmbientCheckInterval + FMath::FRandRange(-Settings->AmbientCheckJitter, Settings->AmbientCheckJitter));
}

void UVTTSubsystem::PlaySound(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound);
	}
}

void UVTTSubsystem::DrawDebug(const APlayerController* Controller, const FVector& ViewLocation) const
{
#if ENABLE_DRAW_DEBUG
	for (const TWeakObjectPtr<UVTTComponent>& Weak : Registered)
	{
		if (const UVTTComponent* Component = Weak.Get())
		{
			const FBox Bounds = Component->GetVTTWorldBounds();
			if (Bounds.IsValid)
			{
				const FColor Color = IsPerceivableFrom(Component, Controller, ViewLocation) ? FColor::Green : FColor::Red;
				DrawDebugBox(GetWorld(), Bounds.GetCenter(), Bounds.GetExtent(), Color, false, -1.f, 0, 2.f);
			}
		}
	}
#endif
}
