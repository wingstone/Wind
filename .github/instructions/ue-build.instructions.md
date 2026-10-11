---
description: "Use when editing Unreal Build Tool scripts (*.Build.cs, *.Target.cs, *.uplugin) in the Wind project: module dependency lists, plugin dependency declaration, PCH settings, and the required project-file regeneration step after changing the build graph."
applyTo: ["**/*.Build.cs", "**/*.Target.cs", "**/*.uplugin"]
---

# 构建脚本规范（Wind）

## 依赖声明

- 依赖模块必须写进 `PublicDependencyModuleNames`（该模块的公共头文件里出现）或 `PrivateDependencyModuleNames`（仅 `.cpp` 里出现）。二者选一是性能问题，选错是编译问题。
- 跨插件模块依赖**同时**要在对应 `.uplugin` 的 `Plugins` 数组里声明插件依赖，否则打包含缺失。
  已知案例：`Plugins/CommonTools/CommonTools.uplugin` 未声明 `Niagara`，但 `CommonTools`/`CommonToolsEditor` 模块依赖 `Niagara` 模块，UBT 生成工程文件时会持续告警。
- 不要为了少写一行依赖而把 `#include` 直接指向另一个模块的内部头文件。

## 保持现有设置

`Source/Wind/Wind.Build.cs` 中的以下设置不要改动：

```csharp
PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
PrivatePCHHeaderFile = "WindPCH.h";
```

`Source/*.Target.cs` 中的 `DefaultBuildSettings = BuildSettingsVersion.V7` 与 `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8` 也保持不动。

改动 `DefaultBuildSettings`、`IncludeOrderVersion`、`PCHUsage` 或 `bUseUnity` 会触发全量重编（该项目有 5 个插件 + 引擎依赖，代价极高），必须先向用户确认。

## 改完必须重新生成

任何 `.Build.cs` / `.Target.cs` / `.uplugin` 改动之后都要重新生成 IntelliSense 与工程数据，否则新模块对编辑器与语言服务器都不可见：

```
Engine\Build\BatchFiles\Build.bat -Mode=GenerateProjectFiles -Project=E:\Github\Wind\Wind.uproject -VSCode
```

生成结果中的 `Warning:` / `Exception while creating build target` 行要读一遍：插件依赖缺失、程序目标不受支持等问题只会在这里暴露。
