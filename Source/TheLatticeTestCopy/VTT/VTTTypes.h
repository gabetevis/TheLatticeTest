// Shared data types for the VTT (Virtual Text Translator) system.

#pragma once

#include "CoreMinimal.h"
#include "VTTTypes.generated.h"

class UVTTComponent;

/** One piece of text a VTT-tagged object can display. */
USTRUCT(BlueprintType)
struct FVTTEntry
{
	GENERATED_BODY()

	/** Optional name used by scripted requests (trigger volumes, code). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	FName Id;

	/** Short header shown above the leader line, e.g. "SAFEGUARD // CLASS 4". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	FText Label;

	/** Main text, typed out below the line. Wraps automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT", meta=(MultiLine="true"))
	FText Body;

	/** May appear on its own (random ambient pop-ups) when the player looks at the object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	bool bAmbient = true;

	/** Ambient pop-ups show this entry only once per play session. Scripted requests ignore this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	bool bAmbientOnlyOnce = false;

	/** Relative chance of being picked among this object's ambient entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT", meta=(ClampMin="0"))
	float Weight = 1.f;

	/** Seconds to stay on screen after typing finishes. 0 = automatic, based on text length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT", meta=(ClampMin="0", ForceUnits="s"))
	float HoldDurationOverride = 0.f;
};

/** Options for scripted VTT requests. */
USTRUCT(BlueprintType)
struct FVTTRequestParams
{
	GENERATED_BODY()

	/** Wait until the object is actually on screen and visible before showing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	bool bWaitUntilVisible = true;

	/** Give up waiting after this many seconds. 0 = wait forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT", meta=(ClampMin="0", ForceUnits="s", EditCondition="bWaitUntilVisible"))
	float Timeout = 20.f;

	/** If the screen is full, push out the oldest callout instead of waiting for a slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	bool bInterruptActive = false;
};

/** A callout currently on screen. Owned by UVTTSubsystem, drawn by UVTTOverlayWidget. */
struct FVTTActiveCallout
{
	int32 Id = INDEX_NONE;
	TWeakObjectPtr<UVTTComponent> Source;
	int32 EntryIndex = INDEX_NONE;

	FText Label;
	FText Body;

	/** Last known world bounds; kept so the callout can animate out after the target is gone. */
	FBox LastWorldBounds = FBox(ForceInit);

	/** Seconds since the callout appeared. */
	float Age = 0.f;
	float TypeDuration = 0.f;
	float HoldDuration = 0.f;

	/** Seconds the target has been continuously off-screen, occluded or out of range. */
	float TimeUnperceived = 0.f;

	bool bDismissing = false;
	float DismissStartAge = 0.f;
};
