// Copyright 2025 Andrew Laptev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatamoshParameters.generated.h"

/** Enum for selecting Datamosh mode */
UENUM(BlueprintType)
namespace EDatamoshModeSetting
{
	enum Type
	{
		Off UMETA(DisplayName = "Off"),
		RenderTarget UMETA(DisplayName = "Render Target"),
		ScreenRendering UMETA(DisplayName = "Screen Rendering")
	};
}

/**
 * Enum to define datamosh noise patterns.
 */
UENUM(BlueprintType)
enum class EDatamoshNoise : uint8
{
    None UMETA(DisplayName = "Disabled"),
    Cubic UMETA(DisplayName = "Pseudo Random Cubic"),
    Strip UMETA(DisplayName = "Horizontal Strip"),
    Jitter UMETA(DisplayName = "Jittering"),
    Simplex2D UMETA(DisplayName = "Simplex 2D"),
    Simplex2DCubic UMETA(DisplayName = "Simplex 2D Cubic")
};

/**
 * Enum to define datamosh styles.
 */
UENUM(BlueprintType)
enum class EDatamoshStyle : uint8
{
    Off UMETA(DisplayName = "Off"),
    ProcessedVelocityAlpha UMETA(DisplayName = "Frame Regeneration"),
};

/**
 * Enum to define datamosh trail effects.
 */
UENUM(BlueprintType)
enum class EDatamoshTrail : uint8
{
    Disabled UMETA(DisplayName = "Disabled"),
    EnabledWithoutMask UMETA(DisplayName = "Include the object"),
    EnabledWithMask UMETA(DisplayName = "Exclude the object itself")
};

/**
 * Struct to hold datamosh parameters for blueprint usage and shader integration.
 */
USTRUCT(BlueprintType)
struct FDatamoshParameters
{
    GENERATED_BODY()

    /** Lerping value between current and previous texture. Default is 0.99 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    float DatamoshFactor = 0.99f;

    /** Trail factor for the datamosh effect. Default is 0.8 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    float DatamoshTrailFactor = 0.8f;

    /** Select built-in distortion pattern for the UV. Default is None */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    EDatamoshNoise DatamoshNoise = EDatamoshNoise::None;

    /** Intensity of datamosh noise. Default is 1.0 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    float DatamoshNoiseIntensity = 1.0f;

    /** Size of datamosh noise. Default is 32.0 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    float DatamoshNoiseSize = 32.0f;

    /** Motion (stretching) of the datamosh effect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    FVector2f DatamoshMotion = FVector2f(0.0f, 0.0f);

    /** Select built-in style preset of the datamosh effect. Default is Off */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    EDatamoshStyle DatamoshStyle = EDatamoshStyle::Off;

    /** Intensity of the datamosh style effect. Default is 0.9 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    float DatamoshStyleIntensity = 0.9f;

    /** Trail effect for datamosh. Default is Disabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    EDatamoshTrail DatamoshTrail = EDatamoshTrail::Disabled;

    /* Depth-based distance effect filtering. Disabled when 0.*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    float DatamoshDistance = 0.0f;

    /** Datamoshed texture sharpness intensity */
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    float DatamoshSharpness = 0.0f;
};
