// Copyright 2025 Andrew Laptev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PostProcessComponent.h"
#include "DatamoshBlueprintLibrary.h"
#include "DatamoshControllerActor.generated.h"

UCLASS()
class DATAMOSHEFFECT_API ADatamoshControllerActor : public AActor
{
    GENERATED_BODY()

public:
    ADatamoshControllerActor();

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
    virtual void BeginPlay() override;

private:
    /*Proxy parameters. Since structs cannot be used in cinematics, we use separate variables and reassemble them into a structure.*/
    FDatamoshParameters Parameters;

public:
    // Current Datamosh mode.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh")
    TEnumAsByte<EDatamoshModeSetting::Type> Mode;

    // Screen Rendering mode only.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode == EDatamoshModeSetting::ScreenRendering"))
    bool bEnabled;

    //// Current Datamosh shader parameters. You can also use these parameters instead of modifying the parameters in the PPM_Datamosh material.
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    //FDatamoshParameters Parameters;

    /** Lerping value between current and previous texture. Default is 0.99 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    float DatamoshFactor = 0.99f;

    /** Trail factor for the datamosh effect. Default is 0.8 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    float DatamoshTrailFactor = 0.8f;

    /** Select built-in distortion pattern for the UV. Default is None */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    EDatamoshNoise DatamoshNoise = EDatamoshNoise::None;

    /** Intensity of datamosh noise. Default is 1.0 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    float DatamoshNoiseIntensity = 1.0f;

    /** Size of datamosh noise. Default is 32.0 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    float DatamoshNoiseSize = 32.0f;

    /** Motion (stretching) of the datamosh effect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    FVector2f DatamoshMotion = FVector2f(0.0f, 0.0f);

    /** Select built-in style preset of the datamosh effect. Default is Off */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    EDatamoshStyle DatamoshStyle = EDatamoshStyle::Off;

    /** Intensity of the datamosh style effect. Default is 0.9 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    float DatamoshStyleIntensity = 0.9f;

    /** Trail effect for datamosh. Default is Disabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    EDatamoshTrail DatamoshTrail = EDatamoshTrail::Disabled;

    /* Depth-based distance effect filtering. Disabled when 0.*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    float DatamoshDistance = 0.0f;

    UPROPERTY(VisibleAnywhere, Category = "Datamosh")
    UPostProcessComponent* PostProcessComponent;

    // The effect material used in Render Target mode.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Datamosh")
    UMaterialInterface* DatamoshMaterial;

    // Manually disable the Post-Process component built into the controller to use PPM_Datamosh independently in Render Target mode.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    bool bEnablePostProcessComponent = true;

    // Event updating the current effect parameters.
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Datamosh")
    void UpdateDatamoshParameters();
    
    // Reset effect parameters to default settings.
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Datamosh")
    void ResetDatamoshParameters();

    void UpdatePostProcess();
    void LoadDefaultMaterial();
};