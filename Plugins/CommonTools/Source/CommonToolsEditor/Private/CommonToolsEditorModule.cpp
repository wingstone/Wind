// Copyright TADemo. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * Editor-side module for the CommonTools plugin.
 *
 * The plugin descriptor (CommonTools.uplugin) declares this module, so it MUST provide an
 * IMPLEMENT_MODULE implementation. Without one, UnrealBuildTool still produces a DLL from the
 * auto-generated Module.CommonToolsEditor.cpp, but that DLL contains no FModuleInitializerEntry.
 * FModuleManager then fails to resolve the module initializer and reports
 * EModuleLoadResult::FailedToInitialize, which makes the entire plugin fail to load:
 *   "Plugin 'CommonTools' failed to load because module 'CommonToolsEditor' could not be
 *    initialized successfully after it was loaded."
 *
 * Editor-only tools (asset actions, detail customizations, menus, ...) belong in this module.
 */
class FCommonToolsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FCommonToolsEditorModule, CommonToolsEditor)
