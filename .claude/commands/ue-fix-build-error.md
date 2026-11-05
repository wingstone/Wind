---
description: 捕获 UE C++ 增量构建错误并循环修复，直到构建通过
argument-hint: [可选：粘贴构建输出]
---

Wind 项目出现 Unreal C++ 构建失败。按以下循环处理：

1. 若下方 `Errors:` 代码块已粘贴输出，从它开始；否则先运行一次构建以捕获当前错误：

   ```
   Engine\Build\BatchFiles\Build.bat WindEditor Win64 Development E:\Github\Wind\Wind.uproject -waitmutex
   ```

   `cwd` 必须为 `E:\ProgramFiles\Epic Games\UE_5.8`。

2. 针对每一条 `error C####` / `error LNK####`，打开对应文件修复根因。
3. 重新构建。重复直到输出 `Result: Succeeded`。

规则：

- 错误指向 `Intermediate/` 或 `*-generated.h` 时，真正的病因在反射声明：检查 `GENERATED_BODY()` 位置、`#include "X.generated.h"` 是否在最后一个 include、类型名是否与头文件同名。**绝不编辑生成产物**。
- 符号未解析且位于另一个模块时，在 `*.Build.cs` 补依赖，**不要**深入 include 其他模块的私有头。
- 绝不手工编辑 `*.generated.h`、`Intermediate/`、`Binaries/`，也不要动 `Wind.uproject` 的 `EngineAssociation`。
- 只修报错，不要顺手处理不相关的 warning，不要越界重构。

最终汇报：使用的构建命令、改动的文件列表、构建的最终 `Result:` 行。

Errors:

```
$ARGUMENTS
```
