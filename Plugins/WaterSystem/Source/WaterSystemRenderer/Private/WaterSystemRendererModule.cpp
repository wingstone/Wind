// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

class FWaterSystemRendererModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Map shader directory
		FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("WaterSystem"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/WaterSystem"), PluginShaderDir);
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FWaterSystemRendererModule, WaterSystemRenderer)
