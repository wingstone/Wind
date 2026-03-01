// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindSystemRendererModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FWindSystemRendererModule"

void FWindSystemRendererModule::StartupModule()
{
	// Map plugin shader directory so compute shaders can be found
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("WindSystem"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/WindSystem"), PluginShaderDir);
}

void FWindSystemRendererModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWindSystemRendererModule, WindSystemRenderer)
