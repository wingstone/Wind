# WaterSystem架构说明 - SceneExtension版本

## 架构概述

WaterSystem现已重构为使用UE5的现代化**SceneExtension**架构，这样可以更好地集成到引擎的渲染管线中。

## 核心组件

### 1. **FWaterFieldSceneExtension** (渲染线程)
- **位置**: `WaterSystemRenderer`模块
- **职责**: 管理GPU资源和流体模拟
- **特点**:
  - 继承自`FSceneExtension`
  - 自动注册到场景渲染管线
  - 使用RenderGraph管理GPU资源
  - 在`PreRender`阶段执行流体模拟

### 2. **UWaterSubsystem** (游戏线程)
- **位置**: `WaterSystemRuntime`模块
- **职责**: 游戏逻辑和交互管理
- **特点**:
  - WorldSubsystem，每个World一个实例
  - 管理交互组件注册
  - 通过`ENQUEUE_RENDER_COMMAND`与渲染线程通信

### 3. **WaterInteractionComponent** (游戏线程)
- **位置**: `WaterSystemRuntime`模块
- **职责**: 角色/物体与水体的交互
- **特点**:
  - 自动检测水下状态
  - 应用浮力和阻力
  - 生成波浪和水花

## 数据流

```
游戏线程                          渲染线程
-----------                       ------------
UWaterSubsystem                   FWaterFieldSceneExtension
     |                                   |
     | ENQUEUE_RENDER_COMMAND           |
     |---------------------------------->|
     |                                   |
     | Config/Disturbance/Interaction   |
     |                                   |
     |                             PreRender()
     |                                   ├─ Initialize Resources (RenderGraph)
     |                                   ├─ Apply Disturbances
     |                                   ├─ Apply Interactions
     |                                   └─ Execute Solver
     |                                         ├─ ShallowWater或
     |                                         └─ NavierStokes
     |                                   |
     |                             PostRender()
     |                                   └─ Readback (optional)
```

## 与旧架构的对比

### 旧架构 (FWaterFieldProxy)
```cpp
// 手动管理RHI资源
FBufferRHIRef HeightFieldBuffer;
HeightFieldBuffer = RHICreateStructuredBuffer(...);

// 手动清理
HeightFieldBuffer.SafeRelease();
```

**问题**:
- ❌ 需要手动管理资源生命周期
- ❌ 不与RenderGraph集成
- ❌ 难以与其他渲染pass协调
- ❌ 依赖过时的RHI API

### 新架构 (SceneExtension + RenderGraph)
```cpp
// 使用RenderGraph自动管理
TRefCountPtr<IPooledRenderTarget> HeightFieldRT;
FRDGTextureRef HeightTexture = GraphBuilder.RegisterExternalTexture(HeightFieldRT);

// 自动清理，无需手动释放
```

**优势**:
- ✅ RenderGraph自动管理资源生命周期
- ✅ 与引擎渲染管线深度集成
- ✅ 支持资源别名和优化
- ✅ 线程安全的资源访问
- ✅ 遵循UE5最佳实践

## 关键API

### 游戏线程 (Subsystem)

```cpp
// 获取Subsystem
UWaterSubsystem* WaterSys = GetWorld()->GetSubsystem<UWaterSubsystem>();

// 配置
WaterSys->FluidConfig.SolverType = EFluidSolverType::ShallowWater;
WaterSys->FluidConfig.GridSize = 256;

// 创建水花
WaterSys->CreateSplash(Position, Strength, Radius);

// 应用交互
WaterSys->ApplyInteraction(InteractionData);
```

### 渲染线程 (SceneExtension)

```cpp
// 在PreRender中执行
void FWaterFieldSceneExtension::PreRender(FRDGBuilder& GraphBuilder)
{
    // 注册外部资源到RenderGraph
    FRDGTextureRef HeightTexture = GraphBuilder.RegisterExternalTexture(
        HeightFieldRT, TEXT("WaterHeightField"));
    
    // 创建临时资源
    FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(
        Desc, TEXT("TempWater"));
    
    // 添加compute pass
    FComputeShaderUtils::AddPass(
        GraphBuilder,
        RDG_EVENT_NAME("WaterSolver"),
        ComputeShader,
        PassParameters,
        GroupCount);
}
```

## 线程通信

### 游戏线程 → 渲染线程

```cpp
void UWaterSubsystem::SendDisturbanceToRT(...)
{
    ENQUEUE_RENDER_COMMAND(AddWaterDisturbance)(
        [Position, Strength, ...](FRHICommandListImmediate& RHICmdList)
        {
            // 在渲染线程执行
            FWaterFieldSceneExtension* Ext = GetSceneExtension();
            Ext->AddDisturbance(Position, Strength, ...);
        });
}
```

### 渲染线程 → 游戏线程 (Readback)

```cpp
// 请求异步回读
SceneExtension->RequestHeightFieldReadback(
    [this](const TArray<float>& Data)
    {
        // 在游戏线程执行callback
        ProcessHeightData(Data);
    });
```

## 资源管理

### Pooled Render Targets

```cpp
// 创建持久化的RT
FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
    FIntPoint(256, 256),
    PF_R32_FLOAT,
    FClearValueBinding::Black,
    TexCreate_None,
    TexCreate_ShaderResource | TexCreate_UAV,
    false);

GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RT, TEXT("MyRT"));
```

**优点**:
- 自动复用
- 减少内存分配
- 性能优越

### RenderGraph临时资源

```cpp
// 创建临时资源（仅在当前帧使用）
FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
    Extent,
    PF_R32_FLOAT,
    FClearValueBinding::None,
    TexCreate_ShaderResource | TexCreate_UAV);

FRDGTextureRef Texture = GraphBuilder.CreateTexture(Desc, TEXT("TempTexture"));
```

**优点**:
- 自动优化内存使用
- 支持资源别名
- 无需手动释放

## 调试和性能分析

### RenderDoc捕获

由于使用RenderGraph，所有的pass都会在RenderDoc中正确显示：

```
Frame Capture:
└─ PreRender
    └─ WaterField
        ├─ ShallowWaterSolver
        ├─ ApplyDisturbances
        └─ ApplyInteractions
```

### GPU时间统计

```cpp
// RenderGraph自动支持GPU timing
RDG_EVENT_NAME("WaterSolver")  // 会出现在stats中
```

使用命令：
```
stat RenderGraph
stat GPU
```

## 扩展性

### 添加新的求解器

1. 创建新的compute shader (`.usf`)
2. 创建shader类继承`FGlobalShader`
3. 在`ExecuteSolver()`中调度

```cpp
void FWaterFieldSceneExtension::ExecuteCustomSolver(FRDGBuilder& GraphBuilder)
{
    // 准备参数
    FCustomSolverCS::FParameters* PassParameters = ...;
    
    // 调度compute shader
    FComputeShaderUtils::AddPass(
        GraphBuilder,
        RDG_EVENT_NAME("CustomSolver"),
        ComputeShader,
        PassParameters,
        GroupCount);
}
```

### 添加渲染输出

SceneExtension可以直接写入到Scene的渲染目标：

```cpp
void FWaterFieldSceneExtension::RenderWaterSurface(FRDGBuilder& GraphBuilder)
{
    // 访问SceneColor
    FRDGTextureRef SceneColor = GraphBuilder.RegisterExternalTexture(...);
    
    // 渲染水面
    AddWaterSurfacePass(GraphBuilder, SceneColor, HeightFieldRT);
}
```

## 迁移指南

### 从旧架构迁移

1. ✅ **移除FWaterFieldProxy** - 已删除旧文件
2. ✅ **创建FWaterFieldSceneExtension** - 完成
3. ✅ **更新Subsystem以使用ENQUEUE_RENDER_COMMAND** - 完成
4. ✅ **注册SceneExtension** - 完成
5. ⚠️ **将Structured Buffers转换为Textures** (待完成)
6. ⚠️ **实现Shader调度** (待完成)

### 待完成项

- [ ] 将compute shader从StructuredBuffer改为Texture2D
- [ ] 实现完整的shader调度逻辑
- [ ] 添加CPU回读机制
- [ ] 性能优化和资源别名

## 性能考虑

### RenderGraph开销
- **优势**: 自动优化资源使用，减少GPU内存
- **开销**: 极小的CPU开销用于构建依赖图（< 0.1ms）

### Pooled RT vs Transient Resources
- **Pooled**: 持久化，跨帧复用（适合主要的模拟缓冲）
- **Transient**: 单帧使用，自动别名（适合临时计算）

## 参考资料

- UE5 SceneExtensions 文档
- RenderGraph编程指南
- Compute Shader最佳实践
- WindSystem实现参考（如果有）

## 总结

新的SceneExtension架构使WaterSystem:
- ✅ 更加现代化和可维护
- ✅ 更好地集成到UE5渲染管线
- ✅ 更容易调试和优化
- ✅ 未来更容易扩展新功能

这是推荐的UE5渲染扩展方式！
