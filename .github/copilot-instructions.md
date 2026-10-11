# Wind — Unreal Engine 5.8 项目

## 环境

- 引擎：`E:\ProgramFiles\Epic Games\UE_5.8`，在工作区中挂载为 `UE5` 文件夹
- 工程：`Wind.uproject`，`EngineAssociation: "5.8"`
- 工具链：VS2022（`E:\ProgramFiles\VS2022`）+ MSVC 14.44，C++20
- 游戏模块：`Source/Wind/`，模块名 `Wind`，导出宏 `WIND_API`

## 构建与验证

构建命令的 `cwd` 必须是引擎根目录，参数顺序为 `<Target> Win64 <Config> <uproject>`：

```
Engine\Build\BatchFiles\Build.bat WindEditor Win64 Development E:\Github\Wind\Wind.uproject -waitmutex
```

- 默认只做**增量**构建 `WindEditor Win64 Development`。不要跑 Rocket、全平台或全引擎重编，除非用户明确要求。
- 编译错误由 `$msCompile` problemMatcher 解析。改完 C++ 必须重新构建一次并确认通过，不要只凭阅读代码就断言已修好。
- 无头自动化测试（测试名以 `Wind.` 为前缀，过滤串**必须带尾点**，否则 `Wind` 会误匹配引擎的 `Slate.Window.*` 测试）：

```
Engine\Binaries\Win64\UnrealEditor-Cmd.exe E:\Github\Wind\Wind.uproject -ExecCmds="Automation RunTests Wind.;Quit" -unattended -nopause -nullrhi -log -stdout
```

  注意：该命令在**测试失败**和**过滤串零匹配**两种情况下都返回非零退出码，所以"没找到测试"会被当成失败。

- 增删模块、修改 `*.Build.cs`/`*.uplugin`/`*.Target.cs` 后，必须重新生成 IntelliSense 数据：

```
Engine\Build\BatchFiles\Build.bat -Mode=GenerateProjectFiles -Project=E:\Github\Wind\Wind.uproject -VSCode
```

- UE 5.8 **不存在** `RunUBT.bat`，不要引用它。

## 代码结构

- `Source/Wind/Wind.Build.cs` — 唯一运行时模块；`WindPCH.h` 为私有 PCH（`UseExplicitOrSharedPCHs`）
- `Source/Wind/Tests/` — 自动化测试，UBT 递归编译模块目录下所有 `.cpp`，新增测试文件无需改 `*.Build.cs`
- `Source/Wind/Variant_{Combat,Flight,Platforming,SideScrolling}/` — Epic 官方模板的四个玩法变体，各自含 `AI/`、`Gameplay/`、`UI/`、`Interfaces/`、`Animation/`；这些目录已全部加入 `PublicIncludePaths`，因此变体之间可以直接互相 include
- `Source/Wind.Target.cs`（Game）、`Source/WindEditor.Target.cs`（Editor）
- `Plugins/` — `WindSystem`、`Fluid2DSystem`、`WeatherSystem`、`CommonTools`、`ElectronicNodes`
- `Tools/` — 文本文件合并/还原辅助脚本

## 硬性约束

- 绝不手工编辑 `*.generated.h`、`Intermediate/`、`Binaries/`、`DerivedDataCache/`、`Saved/`
- 绝不修改 `Wind.uproject` 的 `EngineAssociation`
- 新增依赖只能写进 `*.Build.cs`；不要在源码里用 `#include` 绕过模块边界
- `UE5` 文件夹下的引擎源码是只读参考，不要修改引擎文件
- `.vscode/compileCommands_*.json` 和 `c_cpp_properties.json` 由 UBT 生成，不要手改；`.vscode/tasks.json` 与 `.vscode/settings.json` 是手写的，可以改
- Git的拉取与推送都由人工操作，禁止在 CI/CD 里自动拉取或推送

## 详细规范

- UE C++ 写法：见 [ue-cpp.instructions.md](instructions/ue-cpp.instructions.md)
- 构建脚本改动：见 [ue-build.instructions.md](instructions/ue-build.instructions.md)
