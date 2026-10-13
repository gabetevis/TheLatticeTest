// Copyright 2025 Andrew Laptev. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "Engine/Console.h"
#include "HAL/IConsoleManager.h"
#include "DatamoshParameters.h"
#include "DatamoshDeveloperSettings.generated.h"

UCLASS(config = Plugins, perObjectConfig, meta = (DisplayName = "Datamosh Effect", Category = "Plugins"))
class DATAMOSHEFFECT_API UDatamoshDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	UDatamoshDeveloperSettings();

//#if WITH_EDITOR
//	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
//#endif

	virtual FName GetCategoryName() const override { return FName("Plugins"); }

	void OnDatamoshStencilChanged(IConsoleVariable* Var);
	void OnEyeAdaptationOverrideChanged(IConsoleVariable* Var);

	void OnVelocityCompensationChanged(IConsoleVariable* Var);

	void RegisterConsoleVariableCallbacks();

	/** Compensation value for the Velocity pass. Used to fix the rendering of edge pixels on objects that are constantly rendered for the Velocity pass (e.g., skeletal meshes).*/
	UPROPERTY(config, EditAnywhere, Category = "Datamosh", meta = (ConsoleVariable = "r.Datamosh.VelocityCompensation", ClampMin = "0.0", ClampMax = "0.49"))
	float DatamoshVelocityCompensation;

	/*Pre-exposure override. Use this if you experience brightness issues with the datamosh texture.*/
	UPROPERTY(config, EditAnywhere, Category = "Datamosh", meta = (ConsoleVariable = "r.EyeAdaptation.PreExposureOverride", ClampMin = "0", ClampMax = "1"))
	int EyeAdaptationPreExposureOverride;

	/*Stencil value used to create the mask texture. 0 - applies to any objects rendered in Custom Depth.*/
	UPROPERTY(config, EditAnywhere, Category = "Datamosh", meta = (ConsoleVariable = "r.Datamosh.MaskStencil"))
	int DatamoshStencilValue;
};