// Project-wide VTT tuning, editable in Project Settings > Game > VTT (no Blueprint needed).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "VTTSettings.generated.h"

class UFont;
class USoundBase;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="VTT (Virtual Text Translator)"))
class THELATTICETESTCOPY_API UVTTSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVTTSettings();

	static const UVTTSettings* Get() { return GetDefault<UVTTSettings>(); }

	//~ Behaviour ------------------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category="Behaviour", meta=(ClampMin="1"))
	int32 MaxConcurrentCallouts = 2;

	/** Random pop-ups when the player looks at VTT-tagged objects. */
	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Ambient")
	bool bAmbientEnabled = true;

	/** How often an ambient pop-up is considered. */
	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Ambient", meta=(ClampMin="0.1", ForceUnits="s"))
	float AmbientCheckInterval = 4.f;

	/** Random +/- added to each interval so pop-ups don't feel metronomic. */
	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Ambient", meta=(ClampMin="0", ForceUnits="s"))
	float AmbientCheckJitter = 2.f;

	/** Chance (0-1) that a check actually shows something, when a candidate is visible. */
	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Ambient", meta=(ClampMin="0", ClampMax="1"))
	float AmbientChance = 0.5f;

	/** Minimum time between two ambient pop-ups anywhere. */
	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Ambient", meta=(ClampMin="0", ForceUnits="s"))
	float AmbientGlobalCooldown = 10.f;

	/** Used by VTT components whose MaxDistance is 0. */
	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Visibility", meta=(ClampMin="0", ForceUnits="cm"))
	float DefaultMaxDistance = 5000.f;

	/** Fraction of the screen at each edge where targets don't count as "in view". */
	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Visibility", meta=(ClampMin="0", ClampMax="0.45"))
	float ScreenEdgeMargin = 0.08f;

	/** Seconds a target may be lost (occluded/off-screen) before its callout dismisses. */
	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Visibility", meta=(ClampMin="0", ForceUnits="s"))
	float LoseTargetGraceTime = 0.75f;

	UPROPERTY(Config, EditAnywhere, Category="Behaviour|Visibility")
	TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;

	//~ Timing ---------------------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0", ForceUnits="s"))
	float BoxIntroDuration = 0.12f;

	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0", ForceUnits="s"))
	float LineIntroDuration = 0.1f;

	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="1"))
	float CharactersPerSecond = 60.f;

	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0", ForceUnits="s"))
	float HoldBaseDuration = 2.5f;

	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0", ForceUnits="s"))
	float HoldPerCharacter = 0.035f;

	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0", ForceUnits="s"))
	float HoldMaxDuration = 10.f;

	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0.01", ForceUnits="s"))
	float OutroDuration = 0.3f;

	//~ Style ----------------------------------------------------------------------------------

	/** Color of the box, line and text. Individual VTT components can override it. */
	UPROPERTY(Config, EditAnywhere, Category="Style")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(Config, EditAnywhere, Category="Style|Box", meta=(ClampMin="0.5"))
	float BoxThickness = 4.f;

	/** Space between the object's outline and the box. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Box", meta=(ClampMin="0"))
	float BoxPadding = 10.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Box", meta=(ClampMin="0"))
	float MinBoxSize = 40.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Line", meta=(ClampMin="0.5"))
	float LineThickness = 4.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Line", meta=(ClampMin="0"))
	float LineLength = 140.f;

	/**
	 * Preferred direction of the line: 0 = right, 90 = up, 180 = left, -90 = down.
	 * With Follow Open Space on, the line leaves this direction when there isn't room or the
	 * object is near a screen edge, and glides back when there is.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Style|Line", meta=(ClampMin="-180", ClampMax="180", ForceUnits="deg"))
	float LineAngleDegrees = 0.f;

	/** The line (and its text) moves around the box toward open screen space as the camera moves. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Line")
	bool bFollowOpenSpace = true;

	/** How quickly the line glides around the box to its new direction. Higher = snappier. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Line", meta=(ClampMin="0.1", EditCondition="bFollowOpenSpace"))
	float LineTurnSpeed = 5.f;

	/**
	 * How strongly the line points toward the screen center when its object is near an edge
	 * (0 = only avoid running off screen, 1 = always swing toward the center).
	 */
	UPROPERTY(Config, EditAnywhere, Category="Style|Line", meta=(ClampMin="0", ClampMax="1", EditCondition="bFollowOpenSpace"))
	float PullTowardScreenCenter = 0.7f;

	/** Gap between the end of the line and the text. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="0"))
	float TextGap = 12.f;

	/** Text wraps beyond this width. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="50"))
	float TextMaxWidth = 560.f;

	/** Font asset. Empty = engine default (Roboto Bold). */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(AllowedClasses="/Script/Engine.Font"))
	TSoftObjectPtr<UFont> Font;

	/** Typeface inside the font asset (e.g. "Bold", "Black"). Ignored if the font doesn't have it. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text")
	FName Typeface = TEXT("Bold");

	/** Main text size. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="4"))
	int32 FontSize = 30;

	/** Size of the optional Label line shown above the main text. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="4"))
	int32 LabelFontSize = 16;

	/** Extra spacing between letters, in thousandths of an em. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text")
	int32 LetterSpacing = 0;

	/** Dark outline around letters for legibility over bright areas. 0 = off. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="0", ClampMax="8"))
	int32 TextOutlineSize = 0;

	UPROPERTY(Config, EditAnywhere, Category="Style|Text")
	FLinearColor TextOutlineColor = FLinearColor::Black;

	/** Type the text out letter by letter. Off = text appears all at once. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text")
	bool bTypewriter = true;

	//~ Audio ----------------------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundBase> AppearSound;

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundBase> DismissSound;
};
