// Brain of the VTT system: tracks VTT objects, decides what appears, and runs callout lifetimes.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VTT/VTTTypes.h"
#include "VTTSubsystem.generated.h"

class APlayerController;
class USoundBase;
class UVTTComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVTTCalloutShownSignature, UVTTComponent*, Component, FName, EntryId);

/**
 * One per game world. Responsibilities:
 *  - registry of every UVTTComponent in the world
 *  - ambient (random) pop-ups for objects the player is looking at
 *  - scripted requests, optionally waiting until the target is visible
 *  - lifetime of on-screen callouts (intro, typing, hold, dismiss)
 *
 * Drawing lives in UVTTOverlayWidget, which reads GetActiveCallouts() every frame.
 *
 * Console:  vtt.Debug 1        draws every registered VTT box (green = perceivable)
 *           vtt.ForceAmbient   shows an ambient pop-up now, ignoring chance and cooldowns
 */
UCLASS()
class THELATTICETESTCOPY_API UVTTSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UVTTSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterComponent(UVTTComponent* Component);
	void UnregisterComponent(UVTTComponent* Component);

	/**
	 * Show text from a specific object. EntryId None = first unseen entry.
	 * Returns false if the request was rejected outright (no entries, bad id, disabled).
	 */
	UFUNCTION(BlueprintCallable, Category="VTT")
	bool RequestCallout(UVTTComponent* Component, FName EntryId, const FVTTRequestParams& Params);

	UFUNCTION(BlueprintCallable, Category="VTT")
	void DismissAll(bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category="VTT")
	void SetAmbientEnabled(bool bEnabled) { bAmbientEnabled = bEnabled; }

	/** Shows an ambient pop-up right now if any VTT object is perceivable. */
	UFUNCTION(BlueprintCallable, Category="VTT")
	bool ForceAmbientCallout();

	/** True if the object is on screen, in range and (if required) not occluded. */
	UFUNCTION(BlueprintPure, Category="VTT")
	bool IsPerceivable(const UVTTComponent* Component) const;

	const TArray<FVTTActiveCallout>& GetActiveCallouts() const { return ActiveCallouts; }

	UPROPERTY(BlueprintAssignable, Category="VTT")
	FVTTCalloutShownSignature OnCalloutShown;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FPendingRequest
	{
		TWeakObjectPtr<UVTTComponent> Component;
		int32 EntryIndex = INDEX_NONE;
		FVTTRequestParams Params;
		float Age = 0.f;
	};

	bool GetView(APlayerController*& OutController, FVector& OutViewLocation) const;
	bool IsPerceivableFrom(const UVTTComponent* Component, const APlayerController* Controller, const FVector& ViewLocation) const;

	void UpdateActiveCallouts(float DeltaTime, const APlayerController* Controller, const FVector& ViewLocation);
	void UpdatePendingRequests(float DeltaTime, const APlayerController* Controller, const FVector& ViewLocation);
	void UpdateAmbient(float DeltaTime);
	bool TryShowAmbient(bool bForce);

	void ShowCallout(UVTTComponent* Component, int32 EntryIndex);
	void BeginDismiss(FVTTActiveCallout& Callout);
	void DismissOldest();

	int32 NumOccupiedSlots() const;
	bool IsShowing(const UVTTComponent* Component) const;
	void ResetAmbientTimer();
	void PlaySound(USoundBase* Sound) const;
	void DrawDebug(const APlayerController* Controller, const FVector& ViewLocation) const;

	TArray<TWeakObjectPtr<UVTTComponent>> Registered;
	TArray<FVTTActiveCallout> ActiveCallouts;
	TArray<FPendingRequest> PendingRequests;

	UPROPERTY(Transient) TObjectPtr<USoundBase> AppearSound;
	UPROPERTY(Transient) TObjectPtr<USoundBase> DismissSound;

	float AmbientTimer = 0.f;
	float LastAmbientTime = -1.e9f;
	bool bAmbientEnabled = true;
	int32 NextCalloutId = 1;
};
