# WaterSystem Plugin

2D water fluid simulation plugin for Unreal Engine 5.7 with dual solver support.

## 功能概述

WaterSystem 插件提供了两种基于物理的 2D 流体模拟方法，用于实现玩家与水的交互效果：

### 🌊 求解器类型

#### 1. **浅水方程求解器 (Shallow Water Equations)**
- **适用场景**：大规模水体模拟、湖泊、海洋波浪
- **特点**：快速、适合高度场模拟、支持重力波
- **物理模型**：简化的流体动力学，假设垂直方向压力分布均匀
- **性能**：⭐⭐⭐⭐⭐

#### 2. **纳维-斯托克斯方程求解器 (2D Navier-Stokes)**
- **适用场景**：精细流体效果、漩涡、烟雾、液体细节
- **特点**：物理准确、支持涡旋生成、完整的不可压缩流体
- **物理模型**：完整的流体动力学方程
- **性能**：⭐⭐⭐

### 核心特性
- **GPU 加速**：两种求解器均使用计算着色器在 GPU 上进行
- **玩家交互**：自动处理玩家进入水体、游泳、产生波浪等效果
- **物理反馈**：浮力、水阻、波浪力等物理效果
- **可切换**：运行时可在两种求解器间切换

## 核心组件

### 1. WaterSubsystem
世界子系统，管理整个水体模拟：
```cpp
UWaterSubsystem* WaterSys = GetWorld()->GetSubsystem<UWaterSubsystem>();

// 选择求解器类型
WaterSys->FluidConfig.SolverType = EFluidSolverType::ShallowWater;  // 或 NavierStokes

// 配置流体参数
WaterSys->FluidConfig.GridSize = 256;        // 网格大小
WaterSys->FluidConfig.WorldSize = 10000.0f;  // 世界尺寸 (cm)
WaterSys->FluidConfig.Gravity = 980.0f;      // 重力加速度 (仅浅水方程使用)
WaterSys->FluidConfig.Viscosity = 0.1f;      // 粘度
WaterSys->FluidConfig.Damping = 0.05f;       // 阻尼

// 创建水花
WaterSys->CreateSplash(FVector2D(0, 0), 100.0f, 200.0f);

// 查询水位
float Height = WaterSys->GetWaterHeight(FVector2D(100, 100));
```

### 2. WaterInteractionComponent
添加到角色上实现自动水体交互：
```cpp
UCLASS()
class AMyCharacter : public ACharacter
{
    GENERATED_BODY()
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UWaterInteractionComponent* WaterInteraction;
    
    AMyCharacter()
    {
        WaterInteraction = CreateDefaultSubobject<UWaterInteractionComponent>(TEXT("WaterInteraction"));
        WaterInteraction->SetupAttachment(RootComponent);
        
        // 配置交互参数
        WaterInteraction->InteractionRadius = 100.0f;
        WaterInteraction->SplashStrength = 50.0f;
        WaterInteraction->bApplyBuoyancy = true;
        WaterInteraction->bApplyWaterDrag = true;
    }
};
```

## 物理模型对比

### 方法 1: 浅水方程 (Shallow Water Equations)

插件实现了完整的 2D 浅水方程组：

**连续性方程**（质量守恒）：
```
∂h/∂t + ∂(hu)/∂x + ∂(hv)/∂y = 0
```

**动量方程**（X 方向）：
```
∂u/∂t + u∂u/∂x + v∂u/∂y = -g∂h/∂x + ν∇²u - ku
```

**动量方程**（Y 方向）：
```
∂v/∂t + u∂v/∂x + v∂v/∂y = -g∂h/∂y + ν∇²v - kv
```

参数说明：
- `h`: 水柱高度
- `u, v`: 速度分量
- `g`: 重力加速度
- `ν`: 粘性系数
- `k`: 阻尼/摩擦系数

**优势**：
- ✅ 计算速度快
- ✅ 适合大规模水体
- ✅ 支持高度场查询
- ✅ 适合波浪和水面模拟

**限制**：
- ❌ 不能模拟涡旋
- ❌ 不适合细节流体效果

**数值方法**：
- 有限差分法（空间离散）
- 向前欧拉法（时间积分）

---

### 方法 2: 纳维-斯托克斯方程 (2D Navier-Stokes)

实现了完整的 2D 不可压缩 Navier-Stokes 方程：

**动量方程**：
```
∂u/∂t + (u·∇)u = -1/ρ ∇p + ν∇²u + f
```

**不可压缩约束**：
```
∇·u = 0
```

参数说明：
- `u`: 速度场 (u, v)
- `p`: 压力
- `ρ`: 密度
- `ν`: 运动粘度
- `f`: 外力

**优势**：
- ✅ 物理精确
- ✅ 支持涡旋生成
- ✅ 适合烟雾和细节流体
- ✅ 完整的流体动力学

**限制**：
- ❌ 计算成本较高
- ❌ 不直接支持高度场
- ❌ 需要压力求解

**数值方法**（投影法/Chorin方法）：
1. **对流**：半拉格朗日回溯
2. **扩散**：隐式粘性（Jacobi迭代）
3. **外力**：速度场更新
4. **压力求解**：泊松方程（Jacobi迭代）
5. **投影**：减去压力梯度确保散度为零

---

### 求解器选择指南

| 场景 | 推荐求解器 | 原因 |
|------|-----------|------|
| 大型湖泊/海洋 | 浅水方程 | 性能优异，适合大规模波浪 |
| 室内水池 | Navier-Stokes | 可呈现细节和涡旋 |
| 侧视角水面 | 浅水方程 | 高度信息直观 |
| 烟雾/气体 | Navier-Stokes | 标准流体模拟 |
| 移动平台 | 浅水方程 | 性能优先 |
| 电影级质量 | Navier-Stokes | 物理准确性优先 |

## 数值方法细节

### 浅水方程数值方法
- **有限差分**：空间离散化
- **向前欧拉法**：时间积分
- **边界条件**：网格边缘零速度

## 使用示例

### 示例 1：切换求解器
```cpp
void AMyGameMode::ConfigureWaterSystem()
{
    UWaterSubsystem* WaterSys = GetWorld()->GetSubsystem<UWaterSubsystem>();
    
    // 使用浅水方程求解器（快速，适合大场景）
    WaterSys->FluidConfig.SolverType = EFluidSolverType::ShallowWater;
    WaterSys->FluidConfig.Gravity = 980.0f;  // 浅水方程使用重力
    
    // 或者使用Navier-Stokes求解器（精确，适合细节）
    // WaterSys->FluidConfig.SolverType = EFluidSolverType::NavierStokes;
    // WaterSys->FluidConfig.Density = 1.0f;  // NS需要密度参数
    
    // 通用参数
    WaterSys->FluidConfig.GridSize = 256;
    WaterSys->FluidConfig.Viscosity = 0.05f;
    WaterSys->FluidConfig.Damping = 0.02f;
}
```

### 示例 2：基础水花效果
```cpp
void AMyActor::CreateWaterSplash()
{
    UWaterSubsystem* WaterSys = GetWorld()->GetSubsystem<UWaterSubsystem>();
    if (WaterSys)
    {
        FVector Location = GetActorLocation();
        FVector2D Position2D(Location.X, Location.Y);
        
        // 创建半径 200cm，强度 100 的水花
        WaterSys->CreateSplash(Position2D, 100.0f, 200.0f);
    }
}
```

### 示例 3：检测玩家是否在水中
```cpp
void AMyCharacter::CheckWaterStatus()
{
    if (WaterInteraction->IsSubmerged())
    {
        float SubmersionRatio = WaterInteraction->GetSubmersionRatio();
        UE_LOG(LogTemp, Log, TEXT("Player is %.1f%% submerged"), SubmersionRatio * 100.0f);
        
        // 获取水流信息
        FWaterSample Sample = WaterInteraction->GetWaterSample();
        FVector WaterVelocity(Sample.Velocity.X, Sample.Velocity.Y, 0);
        
        // 应用水流推力
        AddMovementInput(WaterVelocity.GetSafeNormal(), Sample.Velocity.Size() * 0.1f);
    }
}
```

### 示例 4：求解器对比与选择
```cpp
void ADebugActor::ConfigureFluidSolver()
{
    UWaterSubsystem* WaterSys = GetWorld()->GetSubsystem<UWaterSubsystem>();
    
    // 场景1：大型水面 (推荐浅水方程)
    if (bLargeWaterBody)
    {
        WaterSys->FluidConfig.SolverType = EFluidSolverType::ShallowWater;
        WaterSys->FluidConfig.GridSize = 512;  // 可以使用更大的网格
        WaterSys->SimulationSubsteps = 1;      // 单次迭代即可
        WaterSys->FluidConfig.Gravity = 980.0f;
        UE_LOG(LogTemp, Log, TEXT("Using Shallow Water solver for performance"));
    }
    
    // 场景2：精细流体效果 (推荐Navier-Stokes)
    else
    {
        WaterSys->FluidConfig.SolverType = EFluidSolverType::NavierStokes;
        WaterSys->FluidConfig.GridSize = 256;  // 中等网格
        WaterSys->SimulationSubsteps = 4;      // 需要多次迭代求解压力
        WaterSys->FluidConfig.Density = 1.0f;
        UE_LOG(LogTemp, Log, TEXT("Using Navier-Stokes solver for accuracy"));
    }
}
```

### 示例 5：持续波浪生成
```cpp
void AShip::GenerateWaves(float DeltaTime)
{
    if (!WaterInteraction)
        return;
    
    // 船移动时自动生成波浪
    WaterInteraction->bGenerateContinuousWaves = true;
    WaterInteraction->InteractionStrength = GetVelocity().Size() * 0.01f;
}
```

### 示例 6：手动应用物理力
```cpp
void AFloatingObject::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (UPrimitiveComponent* PhysicsComp = Cast<UPrimitiveComponent>(RootComponent))
    {
        if (PhysicsComp->IsSimulatingPhysics())
        {
            // 应用浮力
            WaterInteraction->ApplyBuoyancyForce(PhysicsComp);
            
            // 应用水阻
            WaterInteraction->ApplyDragForce(PhysicsComp);
        }
    }
}
```

## 性能优化

### GPU 计算
- 两种求解器均在 GPU 上使用计算着色器完成
- 支持 16x16 线程组并行计算
- 动态网格大小调整（64-512）

### 求解器性能对比

| 求解器 | 相对性能 | 每帧GPU耗时 (256²网格) | 推荐网格大小 |
|--------|---------|----------------------|-------------|
| 浅水方程 | ⭐⭐⭐⭐⭐ | ~0.8ms | 256-512 |
| Navier-Stokes | ⭐⭐⭐ | ~3.2ms | 128-256 |

### 空间优化
```cpp
// 浅水方程：可以使用更大的网格
if (Config.SolverType == EFluidSolverType::ShallowWater)
{
    WaterSys->FluidConfig.GridSize = 512;        // 大网格性能仍可接受
    WaterSys->FluidConfig.WorldSize = 20000.0f;  // 大范围
    WaterSys->SimulationSubsteps = 1;            // 单次迭代
}

// Navier-Stokes：需要适度网格
else
{
    WaterSys->FluidConfig.GridSize = 256;        // 中等网格
    WaterSys->FluidConfig.WorldSize = 10000.0f;  // 中等范围
    WaterSys->SimulationSubsteps = 4;            // 压力求解需要多次迭代
}
```

### 移动平台优化
```cpp
// 移动设备建议配置
WaterSys->FluidConfig.SolverType = EFluidSolverType::ShallowWater;  // 性能优先
WaterSys->FluidConfig.GridSize = 128;  // 降低分辨率
WaterSys->SimulationSubsteps = 1;      // 最少迭代
```

## 参数调优指南

### 浅水方程参数

### 浅水方程参数

#### 真实海洋波浪
```cpp
Config.SolverType = EFluidSolverType::ShallowWater;
Config.Gravity = 980.0f;    // 地球重力
Config.Viscosity = 0.01f;   // 低粘度（水）
Config.Damping = 0.02f;     // 低阻尼（波浪持续）
Config.GridSize = 512;      // 大网格
```

#### 平静池塘
```cpp
Config.SolverType = EFluidSolverType::ShallowWater;
Config.Gravity = 980.0f;
Config.Viscosity = 0.05f;   // 中等粘度
Config.Damping = 0.1f;      // 较高阻尼（快速平静）
```

---

### Navier-Stokes参数

#### 真实水体（精确模拟）
```cpp
Config.SolverType = EFluidSolverType::NavierStokes;
Config.Density = 1.0f;         // 水的密度
Config.Viscosity = 0.01f;      // 低粘度
Config.Damping = 0.01f;        // 极低阻尼
WaterSys->SimulationSubsteps = 5;  // 较多迭代保证精度
```

#### 粘稠液体（蜂蜜、油）
```cpp
Config.SolverType = EFluidSolverType::NavierStokes;
Config.Density = 1.4f;         // 更高密度
Config.Viscosity = 0.5f;       // 高粘度
Config.Damping = 0.05f;
WaterSys->SimulationSubsteps = 8;  // 高粘度需要更多迭代
```

#### 烟雾效果
```cpp
Config.SolverType = EFluidSolverType::NavierStokes;
Config.Density = 0.001f;       // 极低密度
Config.Viscosity = 0.001f;     // 极低粘度
Config.Damping = 0.05f;        // 中等阻尼（烟雾扩散）
```

---

### 游戏优化配置

#### 快速衰减（性能优先）
```cpp
Config.SolverType = EFluidSolverType::ShallowWater;
Config.Damping = 0.2f;         // 快速消失的波浪
Config.GridSize = 128;         // 小网格
WaterSys->SimulationSubsteps = 1;
```

## 蓝图支持

所有主要功能都支持蓝图：
- `Create Splash` - 创建水花
- `Get Water Sample` - 获取水流信息
- `Is Submerged` - 检测是否在水中
- `Get Submersion Ratio` - 获取浸没比例
- `Set Solver Type` (枚举) - 切换求解器类型

## 技术要求

- Unreal Engine 5.7+
- Shader Model 5.0+ (支持计算着色器)
- GPU 至少支持 DirectX 11 / Vulkan

## 已知限制

- 仅支持 2D 模拟（适用于俯视或侧视游戏）
- 不支持 3D 流体效果（如飞溅、泡沫立体效果）
- 边界条件固定为零速度

## 未来计划

- [ ] CPU 回读优化（异步 GPU→CPU）
- [ ] 自适应网格细化
- [ ] 水面渲染集成
- [ ] 3D 可视化调试工具

## 许可证

Copyright Epic Games, Inc. All Rights Reserved.
