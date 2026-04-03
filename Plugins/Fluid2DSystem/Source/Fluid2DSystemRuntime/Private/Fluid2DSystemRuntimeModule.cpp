// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

class FFluid2DSystemRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Map shader directory
		FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("Fluid2DSystem"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/Fluid2DSystem"), PluginShaderDir);
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FFluid2DSystemRuntimeModule, Fluid2DSystemRuntime)
