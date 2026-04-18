// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FWindSystemRuntimeModule"

class FWindSystemRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FWindSystemRuntimeModule::StartupModule()
{
	// Map virtual shader path so IMPLEMENT_GLOBAL_SHADER can find the .usf/.ush files
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("WindSystem"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/WindSystem"), PluginShaderDir);
}

void FWindSystemRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWindSystemRuntimeModule, WindSystemRuntime)
