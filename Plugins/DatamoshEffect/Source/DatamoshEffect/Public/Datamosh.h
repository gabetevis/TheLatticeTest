// Copyright 2025 Andrew Laptev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
//#include "Logging/LogMacros.h"
//#include "Stats/Stats2.h"

//DECLARE_LOG_CATEGORY_EXTERN(LogDatamoshEffect, Log, All);
//DECLARE_STATS_GROUP(TEXT("DatamoshEffect"), STATGROUP_DatamoshEffect, STATCAT_Advanced);

class FDatamoshModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
