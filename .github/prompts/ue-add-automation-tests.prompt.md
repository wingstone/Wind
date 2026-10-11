---
description: "Scaffold Unreal automation tests for the Wind module so C++ changes can be verified headlessly. Use when adding test coverage for a Wind class or subsystem."
agent: "agent"
---

Add Unreal Engine automation tests covering: <describe the class or behavior to test>

Requirements:

- Put new tests under `Source/Wind/Tests/`. UBT compiles every `.cpp` under the module directory recursively, so no `*.Build.cs` change is needed for a test-only file.
- Declare each test with `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, and name it starting with `Wind.` (for example `Wind.Flight.GustFalloff`) so the headless filter `Automation RunTests Wind` selects it.
- Flag tests with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` unless a different context is genuinely required.
- The harness runs with `-nullrhi -unattended`, so tests must not need a rendered viewport, a loaded map, or PIE.
- Prefer pure-logic tests (math, state machines, data structures). If an actor is unavoidable, spawn it into a transient world created inside the test rather than relying on an existing map.
- Assert with `TestEqual` / `TestTrue` / `TestNotNull` (and the `TestEqual` overload taking a failure message) so failures report actual values.

Then verify by running the workspace task `UE: Run Automation Tests (headless)` and report the pass/fail summary, including how many tests ran.
