# Fluid2DSystem 快速参考

## 求解器选择 - 一分钟指南

### 🌊 浅水方程 (Shallow Water)
```cpp
Config.SolverType = EFluidSolverType::ShallowWater;
```
**何时使用**：大场景、需要性能、水面波浪、2D游戏  
**性能**：⭐⭐⭐⭐⭐ 快  
**质量**：⭐⭐⭐ 好  
**特色**：直接提供水位高度

### 💨 Navier-Stokes
```cpp
Config.SolverType = EFluidSolverType::NavierStokes;
```
**何时使用**：精细效果、涡旋必需、烟雾、高质量  
**性能**：⭐⭐⭐ 中等  
**质量**：⭐⭐⭐⭐⭐ 极佳  
**特色**：物理准确、涡旋效果

---

## 一键配置

### 海洋波浪（性能优先）
```cpp
Config.SolverType = EFluidSolverType::ShallowWater;
Config.GridSize = 512;
Config.Gravity = 980.0f;
Config.Viscosity = 0.01f;
Config.Damping = 0.02f;
Substeps = 1;
```

### 室内水池（质量优先）
```cpp
Config.SolverType = EFluidSolverType::NavierStokes;
Config.GridSize = 256;
Config.Density = 1.0f;
Config.Viscosity = 0.02f;
Config.Damping = 0.01f;
Substeps = 4;
```

### 移动平台
```cpp
Config.SolverType = EFluidSolverType::ShallowWater;
Config.GridSize = 128;
Config.Damping = 0.2f;  // 快速衰减
Substeps = 1;
```

---

## 参数速查

| 参数 | 浅水方程 | Navier-Stokes | 说明 |
|------|---------|---------------|------|
| `SolverType` | 必需 | 必需 | 求解器类型 |
| `GridSize` | 256-512 | 128-256 | 网格分辨率 |
| `Gravity` | ✅ 使用 | ❌ 不使用 | 重力加速度 |
| `Density` | ❌ 不使用 | ✅ 使用 | 流体密度 |
| `Viscosity` | 0.01-0.1 | 0.001-0.5 | 粘度系数 |
| `Damping` | 0.02-0.2 | 0.01-0.05 | 阻尼系数 |
| `Substeps` | 1 | 3-5 | 每帧子步数 |

---

## 常见问题

### Q: 如何切换求解器？
A: 运行时修改 `Config.SolverType` 即可，会在下一帧生效。

### Q: 哪个求解器更快？
A: 浅水方程快3-4倍。

### Q: 需要涡旋效果用哪个？
A: 必须使用Navier-Stokes，浅水方程不支持涡旋。

### Q: 如何获取水位高度？
A: 浅水方程直接提供；Navier-Stokes仅有速度场。

### Q: 移动端推荐哪个？
A: 浅水方程，设置GridSize=128。

---

## 性能对比 (256x256网格)

| 平台 | 浅水方程 | Navier-Stokes |
|------|---------|---------------|
| RTX 3060 | 0.8ms | 3.2ms |
| GTX 1060 | 1.2ms | 5.8ms |
| 移动端 | 2.5ms | 12ms |

---

## 使用检查清单

✅ 选择求解器类型  
✅ 设置合适的GridSize  
✅ 配置对应的物理参数（Gravity/Density）  
✅ 调整Substeps（NS需要更多）  
✅ 添加Fluid2DInteractionComponent到角色  
✅ 测试性能并调优

---

## 求救信号

### ⚠️ 性能太差
- 降低GridSize（512→256→128）
- 切换到浅水方程
- 减少Substeps
- 增加Damping（快速衰减）

### ⚠️ 效果不真实
- 切换到Navier-Stokes
- 增加Substeps
- 降低Damping
- 调整Viscosity

### ⚠️ 波浪消失太快
- 降低Damping
- 浅水方程：检查Gravity设置
- Navier-Stokes：增加Substeps

---

## 代码模板

```cpp
// 在GameMode或LevelScript中
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    UFluid2DSubsystem* Fluid2D = GetWorld()->GetSubsystem<UFluid2DSubsystem>();
    
    // 根据需求选择配置
    #if PLATFORM_MOBILE
        Fluid2D->FluidConfig.SolverType = EFluidSolverType::ShallowWater;
        Fluid2D->FluidConfig.GridSize = 128;
    #else
        Fluid2D->FluidConfig.SolverType = EFluidSolverType::NavierStokes;
        Fluid2D->FluidConfig.GridSize = 256;
    #endif
    
    Fluid2D->FluidConfig.Viscosity = 0.02f;
    Fluid2D->FluidConfig.Damping = 0.05f;
    Fluid2D->bEnableSimulation = true;
}
```

---

更多详细信息请参考：
- [README.md](../README.md) - 完整文档
- [SolverComparison.md](SolverComparison.md) - 技术对比
