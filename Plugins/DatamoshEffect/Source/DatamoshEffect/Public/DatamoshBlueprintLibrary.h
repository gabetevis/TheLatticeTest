// Copyright 2025 Andrew Laptev. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "DatamoshParameters.h"
#include "DatamoshBlueprintLibrary.generated.h"

/**
 * Blueprint library for managing Datamosh effects and parameters.
 */
UCLASS()
class DATAMOSHEFFECT_API UDatamoshBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    // Setters

    UFUNCTION(BlueprintCallable, Category = "Datamosh", meta = (DisplayName = "Set Datamosh Mode", ToolTip = "Datamosh Effect Render Mode. 0 - Disabled, 1 - Render Target mode, 2 - Screen Rendering mode. See documentation."))
    static TEnumAsByte<EDatamoshModeSetting::Type> SetDatamoshMode(TEnumAsByte<EDatamoshModeSetting::Type> Mode);

    UFUNCTION(BlueprintCallable, Category = "Datamosh", meta = (DisplayName = "Enable Datamosh Effect (Screen Rendering only)", ToolTip = "Enable Datamosh Effect. Only when using Screen Rendering Mode."))
    static bool SetDatamoshActive(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Datamosh", meta = (DisplayName = "Reset Datamosh Render Target", ToolTip = "Resets the render targets and calls CreatePooledRenderTarget_RenderThread"))
    static void ResetRenderTarget();

    UFUNCTION(BlueprintCallable, Category = "Datamosh", meta = (DisplayName = "Set Datamosh Parameters", ToolTip = "Apply custom Datamosh parameters."))
    static void SetDatamoshParameters(FDatamoshParameters Parameters);

    // Getters

    UFUNCTION(BlueprintPure, Category = "Datamosh")
    static TEnumAsByte<EDatamoshModeSetting::Type> GetDatamoshMode();

    UFUNCTION(BlueprintPure, Category = "Datamosh", meta = (DisplayName = "Get Datamosh Active (Screen Rendering only)"))
    static bool GetDatamoshActive();

    UFUNCTION(BlueprintCallable, Category = "Datamosh", meta = (DisplayName = "Get Datamosh Parameters", ToolTip = "Retrieve the current Datamosh shader parameters."))
    static FDatamoshParameters GetDatamoshParameters();

    UFUNCTION(BlueprintCallable, Category = "Datamosh", meta = (DisplayName = "Reset Datamosh Parameters", ToolTip = "Resets the current effect parameters to default values"))
    static void ResetToDefaultParameters();

private:
    static FDatamoshParameters DefaultParameters;
};
