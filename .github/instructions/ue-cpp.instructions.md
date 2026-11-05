---
description: "Use when writing or modifying Unreal Engine C++ in the Wind project: UCLASS/USTRUCT/UENUM and UPROPERTY/UFUNCTION reflection macros, GENERATED_BODY placement, GC and TObjectPtr ownership, WIND_API exports, include ordering, replication, UE logging and assertion macros, and UE naming prefixes."
applyTo: ["Source/**/*.h", "Source/**/*.cpp"]
---

# UE C++ 规范（Wind 模块）

## 头文件结构

每个含反射类型的头文件必须按此顺序，且 `*.generated.h` 永远是最后一个 include，不能被 `#if` 包裹：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyThing.generated.h"   // 必须最后
```

- `.cpp` 中第一个 include 是自己的头文件。
- 只 include 具体需要的头文件，不要 `#include "Engine.h"` / `"UnrealEd.h"` 这类聚合头。
- 反射类型必须与其头文件同名（`AWindFlierGameMode` → `WindFlierGameMode.h`），否则 UHT 报错。

## 反射

- `GENERATED_BODY()` 必须是类型体内第一项。
- `UENUM` 需继承 `uint8`；暴露给编辑器的枚举项加 `UMETA(DisplayName="...")`。
- 新增 `UCLASS` 时不要顺手加 `Blueprintable`，除非确实要派蓝图子类。需要编辑器友好名字用 `meta = (DisplayName = "...")`。
- 覆盖虚函数时用 `virtual ... override`；接口实现方法必须加 `virtual ... override`，否则不参与虚表分发。

## 注释

- 所有非copyright的注释都使用中文。

## GC 与对象指针

- 任何需要被 GC 追踪的 `UObject` 成员都必须标 `UPROPERTY()`；未标 `UPROPERTY` 的裸 `UObject*` 成员会被 GC 回收成悬垂指针。
- 新代码中对象成员用 `TObjectPtr<T>`，不要用裸 `T*`：

```cpp
UPROPERTY()
TObjectPtr<UStaticMeshComponent> Mesh;
```

- 非 UObject 的所有权用 `TUniquePtr`/`TSharedPtr`，不要用裸 `new`/`delete`。
- 弱引用用 `TWeakObjectPtr<T>`，避免用 `UPROPERTY()` 制造强引用循环。

## 命名前缀

`A`(Actor) `U`(Object) `F`(struct) `E`(enum) `I`(interface) `T`(template)，布尔成员用 `b` 前缀。结构体和枚举不加 `F`/`E` 之外的修饰。

## 导出

- 需要被其它模块或插件引用的类型/函数加 `WIND_API`（见 `Source/Wind/Variant_Combat/AI/EnvQueryContext_Danger.h`）。
- 模块内部使用的类型不要加，越多的 DLL 导出面意味着越长的链接时间和更大的二进制。

## 日志与断言

- 日志用 `UE_LOG(LogTemp, Warning, TEXT("..."))`；新增独立日志类别时用 `DECLARE_LOG_CATEGORY_EXTERN`/`DEFINE_LOG_CATEGORY` 成对声明，不要散落 `LogTemp`。
- 不可恢复的不变量用 `check()`/`checkf()`；可恢复的异常输入用 `ensure()`/`ensureMsgf()`，不要把 `ensure` 当 `check` 用。
- 指针解引用前用 `IsValid(Object)`（`AActor` 用 `IsValid(this)`），不要用 `!= nullptr` 判断已销毁的 Actor。

## 复制（仅在确有联网需求时）

- 复制属性在构造函数里标 `DOREPLIFETIME`，并在 `GetLifetimeReplicatedProps` 中声明。
- 只有 Actor 的 `bReplicates = true` 时才做复制，不要给本地单机类加复制代码。
