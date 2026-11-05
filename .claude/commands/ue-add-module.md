---
description: 为 Wind 项目新增一个 C++ 游戏模块或插件模块（含 descriptor、Build.cs、target 注册、导出宏与工程重生成）
argument-hint: <模块名>，以及其内容描述
---

要新增：$ARGUMENTS

按顺序执行：

1. **游戏模块** —— 创建 `Source/<Name>/<Name>.Build.cs` 以及 `<Name>.h` / `<Name>.cpp`。主游戏模块用 `IMPLEMENT_PRIMARY_GAME_MODULE`，其余模块用 `IMPLEMENT_MODULE`。在 `Source/Wind.Target.cs` **和** `Source/WindEditor.Target.cs` **两处**都加上 `ExtraModuleNames.Add("<Name>")`。
2. **插件模块** —— 在 `.uplugin` 的 `Modules` 数组中加入 `Name` / `Type` / `LoadingPhase`，并在插件自身的 `Build.cs` 中显式声明 `PCHUsage`。
3. **依赖** —— 公共头文件里出现的类型放 `PublicDependencyModuleNames`，仅在 `.cpp` 中使用的放 `PrivateDependencyModuleNames`。若依赖的模块属于另一个插件，还需要把该插件写入所在 `.uplugin` 的 `Plugins` 数组，否则打包会悄悄缺失。
4. **PCH** —— 与 Wind 模块保持一致：`PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs`，并显式配置 `PrivatePCHHeaderFile`。
5. **导出宏** —— 跨模块使用的类型加 `<NAME>_API`，和现有 `WIND_API` 的习惯保持一致。

然后：

- 运行一次工程重生成：

  ```
  Engine\Build\BatchFiles\Build.bat -Mode=GenerateProjectFiles -Project=E:\Github\Wind\Wind.uproject -VSCode
  ```

  仔细读输出里的 `Warning:` 和 `Exception while creating build target`——依赖问题只在这里暴露。

- 以 `WindEditor Win64 Development` 做一次增量构建，汇报最终 `Result:` 行。
