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
	float BoxIntroDuration = 0.25f;

	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0", ForceUnits="s"))
	float LineIntroDuration = 0.18f;

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

	UPROPERTY(Config, EditAnywhere, Category="Style|Color")
	FLinearColor FrameColor = FLinearColor(0.55f, 1.f, 0.85f, 1.f);

	UPROPERTY(Config, EditAnywhere, Category="Style|Color")
	FLinearColor LabelColor = FLinearColor(0.55f, 1.f, 0.85f, 1.f);

	UPROPERTY(Config, EditAnywhere, Category="Style|Color")
	FLinearColor BodyColor = FLinearColor(0.9f, 0.95f, 0.95f, 1.f);

	/** Color of not-yet-translated (scrambled) characters while typing. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Color")
	FLinearColor ScrambleColor = FLinearColor(0.55f, 1.f, 0.85f, 0.6f);

	/** Panel behind the text for legibility. Alpha 0 disables it. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Color")
	FLinearColor TextBackdropColor = FLinearColor(0.f, 0.f, 0.f, 0.45f);

	/** Opacity of the thin full rectangle drawn between the corner brackets. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Frame", meta=(ClampMin="0", ClampMax="1"))
	float FrameOutlineOpacity = 0.25f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Frame", meta=(ClampMin="0"))
	float FrameThickness = 1.5f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Frame", meta=(ClampMin="0"))
	float CornerLength = 14.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Frame", meta=(ClampMin="0"))
	float BoxPadding = 8.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Frame", meta=(ClampMin="0"))
	float MinBoxSize = 28.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Leader", meta=(ClampMin="0"))
	float LineThickness = 1.25f;

	/** Length of the diagonal segment leaving the box. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Leader", meta=(ClampMin="0"))
	float LeaderLength = 70.f;

	/** Horizontal gap between the elbow and the text block. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Leader", meta=(ClampMin="0"))
	float ShelfLength = 24.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="50"))
	float TextMaxWidth = 340.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="0"))
	float TextGap = 4.f;

	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="0"))
	float TextBackdropPadding = 6.f;

	/** Font asset. Empty = engine default (Roboto). */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(AllowedClasses="/Script/Engine.Font"))
	TSoftObjectPtr<UFont> Font;

	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="4"))
	int32 LabelFontSize = 11;

	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="4"))
	int32 BodyFontSize = 13;

	/** Extra spacing between letters, in thousandths of an em. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text")
	int32 LetterSpacing = 60;

	/** Number of scrambled characters running ahead of the typing cursor ("translation" effect). */
	UPROPERTY(Config, EditAnywhere, Category="Style|Text", meta=(ClampMin="0"))
	int32 ScrambleLeadCharacters = 6;

	/** Flicker and jitter while callouts appear and disappear. */
	UPROPERTY(Config, EditAnywhere, Category="Style|Effects")
	bool bGlitchEffects = true;

	//~ Audio ----------------------------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundBase> AppearSound;

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundBase> DismissSound;
};
