---
description: "Fix Unreal Engine C++ build errors in the Wind project: capture the incremental Build.bat output, map each error to its source, apply minimal fixes, and rebuild until clean."
agent: "agent"
---

There are Unreal C++ build failures in the Wind project. Work this loop:

1. Build to capture the current errors using the workspace task `UE: Build WindEditor Development`, or directly:

   `Engine\Build\BatchFiles\Build.bat WindEditor Win64 Development E:\Github\Wind\Wind.uproject -waitmutex` — with `cwd = E:\ProgramFiles\Epic Games\UE_5.8`.

2. For each `error C####` / `error LNK####` line, open the referenced file and fix the cause.
3. Re-run the build. Repeat until it prints `Result: Succeeded`.

Rules:

- If build output is pasted below, start from that instead of a fresh build.
- If errors point into files under `Intermediate/` or `*-generated.h`, the real cause is the reflected declaration: check `GENERATED_BODY()` placement, that `#include "X.generated.h"` is the last include, and that the type name matches its header file name. Never edit the generated artifact.
- If a symbol is unresolved and lives in another module, add the dependency in `*.Build.cs` — do not deep-include another module's private header.
- Never hand-edit `*.generated.h`, `Intermediate/`, `Binaries/`, or `Wind.uproject`'s `EngineAssociation`.
- Fix the errors, not unrelated warnings. Do not refactor beyond what the errors require.

Finish by reporting the exact command, the files you changed, and the final build result line.

Errors:

```
<paste build output here>
```
