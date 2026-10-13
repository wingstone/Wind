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

## 花瓣收集与拖尾

代码位于 [Source/Wind/Petals](./Source/Wind/Petals)。纯 C++，不依赖任何美术资产：花瓣默认用引擎自带的 `/Engine/BasicShapes/Plane`，材质留了覆盖位。

玩法是"边飞边收"：世界里漂浮着待收集的花瓣，靠近就被吸过来，**每收一片，身后的花瓣链就长一片**；链子到上限后，尾巴最老的花瓣被顶掉，所以视觉和开销都恒定。

### 场景里怎么摆

| 放什么 | 作用 |
| --- | --- |
| `APetalPickup` | **手动摆一片花瓣**。视口里有图标手柄，好拖好选；属性就是玩法参数。 |
| `APetalPickupField` | 一片**程序化花瓣海**，按 `FieldExtents × Density` 批量生成。 |
| `UPetalTrailComponent` | 身后的花瓣链。挂在一个**静止**的 Actor 上（世界原点即可）。 |
| `UPetalMagnetComponent` | 挂在 Pawn 上，负责吸。会自动去找同一个 Actor 上的 `UPetalTrailComponent`。 |

`APetalPickup` 只是**授权用的代理**：`BeginPlay` 时最近的 `APetalPickupField` 会把它的位置和参数收进池子，然后这个 Actor 就退场（隐藏、停 tick、关碰撞，可选分帧销毁）。多个场共存时按距离就近认领，所以摆放位置本身就决定了归属。运行时才生成的花瓣（流送、脚本 spawn）会自己向最近的场注册，两条路都收敛到同一处，所以 `BeginPlay` 顺序无关。

`APetalPickup` 上的玩法参数：`PetalValue`（这一片值几片，大花瓣可以给 5）、`Variation`（材质用的外观索引）、`Scale`、`bRespawn`/`RespawnDelay`、`AttractRadiusOverride`（局部覆盖磁吸半径，用来做"这条路上的花瓣更好吸"）、`Group`（分组，配合 `GetPickupTransformsByGroup` 让玩法脚本读到设计师摆的位置）。

真要一个"特殊大花/剧情道具"级别的花瓣时，勾上 `bAuthoringOnly`：它不会被捕获，保留自己的 Mesh 与蓝图逻辑，由一个极薄的 `UPetalPickupCollectableComponent` 自己判定吸收。这条路适合几十个，不适合几百个。

### 参数速查

`UPetalTrailComponent`

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `MaxPetals` | 3000 | 链子上限。到顶后每收一片就顶掉最老的一片。 |
| `PetalSpacing` | 15 cm | 相邻花瓣的间距，也决定整条链的长度：`MaxPetals × PetalSpacing`。 |
| `PetalScale` / `PetalScaleJitter` | 0.15 / 0.35 | 花瓣大小与随机浮动（相对于 Mesh 自身尺寸，默认 Plane 是 1 m）。 |
| `PetalTiltDegrees` | 25 | 花瓣相对路径方向的散开角度。 |
| `InstanceCullDistance` | 20 m | 超过这个距离的实例被剔除。 |
| `FollowComponent` | 空 | 跟踪目标，留空就用 Owner 的位置。 |

`UPetalMagnetComponent`

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `MagnetRadius` | 1500 | 进入这个半径的花瓣开始飞过来。 |
| `AbsorbRadius` | 120 | 到这个距离就算收下。 |
| `MaxAttractPerFrame` | 64 | 一帧最多拉几片，防止高速穿过花瓣海时瞬间拉几百片。 |

`UPetalPickupFieldComponent`

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `FieldExtents` | (2000, 2000, 1000) | 程序化花瓣体积的半尺寸。 |
| `Density` | 0.02 | 每立方米几片，再被 `MaxProceduralPickups` 封顶。 |
| `MaxProceduralPickups` | 2000 | 程序化花瓣数量上限。 |
| `PullAcceleration` / `AttractDrag` / `MaxAttractSpeed` | 12000 / 1.5 / 6000 | 花瓣被拖向玩家的手感：加速度、阻尼、限速。 |

### 性能：花瓣多起来为什么不卡

这是这套设计唯一值得解释的地方。

1. **拖尾每帧的开销与花瓣总数无关。** 每帧只做一件事：把玩家新走过的那点距离按 `PetalSpacing` 记进环形缓冲。花瓣不是"第 m 片"，而是"离头多远"（serial），serial 落在哪个槽位是固定的，所以**环槽和 ISM 实例一一绑死、永不重排** —— 一帧通常只有 1~3 个实例需要重写。3000 片和 300 片的每帧成本一样，实例数只影响显存（3000 × 64B ≈ 0.2 MB）。
   到上限时更简单：新节点直接覆盖最老的槽，天然就是"顶掉尾巴"。
2. **花瓣场待机不耗任何东西。** 一片花瓣 = 池子里一条 POD 数据 + 一个实例，没有 Actor、没有组件、没有碰撞体。待机的上下浮动/自转交给材质（`PerInstanceCustomData` 带相位 + Time 节点），CPU 侧位置一动不动，因此**待机时连 tick 都不开**；只有"正在被吸"的少数花瓣才在 CPU 上积分。磁吸查询走均匀网格，只看玩家所在格和邻格。
3. **手摆多少花瓣都不影响运行时开销。** 这点是刻意的：如果让每个 `APetalPickup` 自己渲染自己，几百片就是几百个 Actor、几百个 proxy、几百次 draw call —— 所以它们会被捕获进池，运行时只剩 1 个 draw call。
4. **用 `UInstancedStaticMeshComponent`，不要用 HISM。** HISM 的实例变换一改就会重建聚簇树（`UHierarchicalInstancedStaticMeshComponent::UpdateInstanceTransform` 会置 `bIsOutOfDate`），而我们的实例每帧都在动。ISM 走的是增量路径：一帧内多次 `UpdateInstanceTransform(..., bMarkRenderStateDirty=false)`，末尾一次 `MarkRenderStateDirty()`，只上传真正变了的实例。
5. 阴影、碰撞、overlap、贴花、遮挡查询全部关掉；逐实例差异走 custom data（`0` = 相位，`1` = 外观索引），不要用多材质。

如果将来要把花瓣数量推到十万级，`FPetalTrailCore` 与渲染是解耦的：可以照 [Plugins/WindSystem](./Plugins/WindSystem) 的 `FWindFieldSceneExtension`（scene extension + global shader）那样，把路径节点塞进 structured buffer，让花瓣位置在顶点着色器里按实例序号算出来；或者照 `NiagaraDataInterfaceWindField` 做一个把路径交给 Niagara 的 Data Interface。两条路都保持"CPU 只维护一条路径"这个前提。

### 释放与开花

`AddPetals` / `ReleasePetals` / `SetPetalCount` 是对外的三个入口，`ReleasePetals` 就是"把花瓣花出去"：吐掉 N 片后链子变短，之后可以继续涨，同时广播 `OnPetalsReleased` 给特效和玩法用。

### 已知取舍

- **站着不动不会长拖尾**：链子是沿着你走过的路径铺开的，没飞过就没有路径。收集数照常累加，等你再次移动时链子会顺着路径"补"上来。
- `MaxPetals` 与 `PetalSpacing` 在 `BeginPlay` 生效，运行中改不会重建实例。
- 实例坐标是世界空间的，关卡离世界原点超过约 10 km 时浮点精度会退化。真要做那么大，参考 `WindFieldSceneExtension` 的滚动量化做法。

### 调试

- `UPetalTrailComponent::bDrawDebug`：画出路径上的节点与当前片数。
- `UPetalPickupFieldComponent::bDrawDebug`：画出花瓣体积与存活/在飞数量。
- `UPetalMagnetComponent::bDrawDebug`：画出磁吸半径与吸收半径。
- 场初始化时会在 `LogWind` 里打印收下多少花瓣、总共多少片，方便估数量。

