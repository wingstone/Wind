---
description: 为 Wind 模块里的某个类或行为补无头自动化测试
argument-hint: <被测试的类或行为描述>
---

为以下目标新增 Unreal 自动化测试：$ARGUMENTS

要求：

- 新测试放在 `Source/Wind/Tests/`。UBT 会递归编译模块目录下所有 `.cpp`，纯测试文件无需改 `*.Build.cs`。
- 用 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` 声明，测试名以 `Wind.` 开头（例如 `Wind.Flight.GustFalloff`），这样无头过滤串 `Automation RunTests Wind.` 能选中它。
- Flags 默认用 `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`，除非确有理由换成别的 context。
- Harness 用 `-nullrhi -unattended` 运行，测试**不得**依赖渲染视口、加载好的地图或 PIE。
- 优先写纯逻辑测试（数学、状态机、数据结构）。实在要用 Actor，就在测试内部临时创建一个 world，不要依赖关卡现成的 Actor。
- 断言用 `TestEqual` / `TestTrue` / `TestNotNull`（带失败信息的 `TestEqual` 重载），确保失败时能看见实际值。

然后以无头方式跑一次测试：

```
Engine\Binaries\Win64\UnrealEditor-Cmd.exe E:\Github\Wind\Wind.uproject -ExecCmds="Automation RunTests Wind.;Quit" -unattended -nopause -nullrhi -log -stdout
```

注意过滤串**必须带尾点**，否则会误匹配引擎的 `Slate.Window.*` 测试。汇报通过/失败数量以及总运行数。
