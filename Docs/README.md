# Wind
一款与风相关的治愈游戏

## 风力飞行控制器（3C）

独立的飞行吹风 3C，代码位于 [Source/Wind/Variant_Flight](./Source/Wind/Variant_Flight)。纯 C++ 实现，不依赖任何输入或美术资产，也不接 `WindSystem` 风场。

### 启用

把 `AWindFlierGameMode` 设为默认 GameMode（Project Settings → Maps & Modes，或在关卡 World Settings 中覆盖）即可，它自带默认 Pawn 与 HUD。关卡里需要自己放一些开启 Simulate Physics 的物体来验证吹风效果。

### 操作

| 操作 | 键鼠 | 手柄 |
| --- | --- | --- |
| 飞行 | W / A / S / D | 左摇杆 |
| 升降 | Space / LeftCtrl | LB / RB |
| 指向（即风的方向） | 鼠标 | 右摇杆 |
| 按住蓄力吹风 | 按住左键 | 按住 LT |

如果给 Pawn 指定了可选的 Enhanced Input Actions（`MoveAction` / `LookAction` / `ChargeAction`），会优先使用它们，否则自动退回 `Config/DefaultInput.ini` 里的 `WindFlier_*` 映射。

### 手感的三个层次

从输入到物体，风一共经过三层，每一层都不是瞬时生效的：

1. **蓄力** —— 按住时 `ChargeAlpha` 按 `ChargeTime` 涨满，松开按 `DischargeTime` 回落。这只是"玩家的意图"，还不是风。
2. **风的包络** —— 风自身的速度与方向都是弹簧：`RiseStiffness` / `RiseDamping` 决定"起风"（阻尼略低于 1 会有一点过冲，读起来就是一阵风），`FallStiffness` / `FallDamping` 决定松手后的"衰减"尾巴，`DirectionStiffness` 让转向时风滞后于准心。HUD 上"蓄力条"和"实际风力条"一前一后的错位，就是这层包络。
3. **物体响应** —— 锥形范围内的刚体是被"拖向"当地风速（`WindResponseRate`），而不是把速度直接赋值，所以会逐渐加速、风停后继续滑行；`MassInfluence` 让重物反应更迟钝，但有 `MinMassResponse` 兜底不会推不动。

飞行本身同样带拖拽：推力（`FlyAcceleration`）持续累加进速度，空气阻力（`AirDrag`）每帧按指数衰减，松手是滑行而不是急停；相机朝向用 `AimInterpSpeed` 追输入目标，并按侧向速度压 `BankAngleMax` 做滚转，形成轻微拖拽感。Pawn 全身没有任何碰撞体，会直接穿过场景。

### 调试

- `UWindGustComponent::bDrawDebug`：绘制作用锥与风力指示线。
- `UWindGustComponent::bLogWind`：打印起风 / 松手。
- `AWindFlierHUD`：底部两条能量条，上方文字显示当前阶段（Idle / Rising / Holding / Falling）与实际风速，方便对着参数调手感。

若鼠标视角偏"重"，可以关掉 `Config/DefaultInput.ini` 中的 `bEnableMouseSmoothing`。

