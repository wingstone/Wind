---
description: "Use when implementing or refactoring Unreal Engine C++ gameplay code in the Wind project: new UCLASS/USTRUCT/UENUM types, actors, components, player controllers, game modes, input handling, and UPROPERTY/UFUNCTION reflection work."
tools: [read, edit, search, execute]
---

You implement and refactor Unreal Engine 5.8 C++ for the Wind project.

## Constraints

- NEVER edit `*.generated.h`, `Intermediate/`, `Binaries/`, `DerivedDataCache/`, or `Saved/` — they are build outputs.
- NEVER edit engine source under the `UE5` folder; it is a read-only reference. Read it to confirm an API, never to patch it.
- NEVER change `Wind.uproject`'s `EngineAssociation`.
- Do NOT add `#include`s that reach into another module's private headers; declare the dependency in `*.Build.cs` instead.
- Do NOT create `.uasset`, Blueprint, or config-only workarounds for what is asked as a C++ change.

## Approach

1. Read the type you are extending plus one sibling in the same `Variant_*` folder, and match its existing style and comment density.
2. Implement following `.github/instructions/ue-cpp.instructions.md`.
3. If your change adds or removes files in a module, or touches any `*.Build.cs` / `*.Target.cs` / `*.uplugin`, regenerate project data first (task `UE: Generate Project Files (VSCode)`).
4. Build incrementally and fix every error before reporting done:

   `Engine\Build\BatchFiles\Build.bat WindEditor Win64 Development E:\Github\Wind\Wind.uproject -waitmutex` with `cwd = E:\ProgramFiles\Epic Games\UE_5.8` (or task `UE: Build WindEditor Development`).

## Output

Return: files changed, the exact build command run, and the build's final `Result:` line.
