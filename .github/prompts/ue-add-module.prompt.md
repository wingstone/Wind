---
description: "Add a new C++ module or plugin module to the Wind project correctly: descriptor entries, Build.cs dependencies, target registration, export macro, and the required project-file regeneration."
agent: "agent"
---

Add: <new module or plugin name, and what it will contain>

Follow this checklist in order:

1. **Game module** — create `Source/<Name>/<Name>.Build.cs` plus `<Name>.h` / `<Name>.cpp`. The primary game module uses `IMPLEMENT_PRIMARY_GAME_MODULE`; every additional module uses `IMPLEMENT_MODULE`. Register it with `ExtraModuleNames.Add("<Name>")` in **both** `Source/Wind.Target.cs` and `Source/WindEditor.Target.cs`.
2. **Plugin module** — add the module to the `.uplugin` `Modules` array with `Name`, `Type`, and `LoadingPhase`, and keep the plugin's `Build.cs` explicit about `PCHUsage`.
3. **Dependencies** — put dependencies in `PublicDependencyModuleNames` (types appear in this module's public headers) or `PrivateDependencyModuleNames` (used only in `.cpp`). If the dependency is a module owned by another plugin, also add that plugin to the `Plugins` array of the owning `.uplugin`, otherwise packaging silently misses it.
4. **PCH** — stay consistent with the Wind module: `PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs` and an explicit `PrivatePCHHeaderFile`.
5. **Export macro** — cross-module types take `<NAME>_API`, matching the existing `WIND_API` convention.

Then:

- Run the workspace task `UE: Generate Project Files (VSCode)` and read the output for `Warning:` and `Exception while creating build target` lines — dependency problems appear only there.
- Finish with an incremental build of `WindEditor Win64 Development` and report the final `Result:` line.
