// Copyright 2025 Andrew Laptev. All Rights Reserved.

#include "Datamosh.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FDatamoshModule"

void FDatamoshModule::StartupModule()
{
	// The exact timing is specified in the .uplugin file per-module
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("DatamoshEffect"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugins/DatamoshEffect"), PluginShaderDir);
}

void FDatamoshModule::ShutdownModule()
{
	ResetAllShaderSourceDirectoryMappings();
	// This function may be called during shutdown to clean up your module. For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDatamoshModule, DatamoshEffect)