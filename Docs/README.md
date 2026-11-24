# Wind
一款与风相关的治愈游戏

## 打开工程时自动编译

[Config/DefaultEditorPerProjectUserSettings.ini](../Config/DefaultEditorPerProjectUserSettings.ini) 里打开了引擎自带的 `bForceCompilationAtStartup`（设置面板：Editor Preferences → General → Loading & Saving → Startup → Force Compilation at Startup）。启动编辑器时会先让 UBT 跑一次 `WindEditor` 的增量编译：C++ 有改动就编完再进编辑器，没改动就是一次几秒钟的空跑。

- 它是 per-project **user** settings，本机 `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` 里的同名值优先级更高。如果那里存着旧编辑器写下的 `bForceCompilationAtStartup=False`，启动时就不会编译，需要在设置面板里重新勾一次。
- 从 VS Code / Visual Studio 的启动配置启动时，前面的构建任务已经把代码编好，启动阶段这次编译只会报一句 `Target is up to date` 然后继续；只有代码没编就直接启动（例如双击 `.uproject`）时才会真的重编。
- 不想在启动时跑 UBT，加命令行参数 `-SKIPCOMPILE`。

## 风力飞行控制器（3C）

独立的飞行 3C，代码位于 [Source/Wind/Variant_Flight](./Source/Wind/Variant_Flight)。纯 C++ 实现，不依赖任何输入或美术资产。

### 启用

把 `AWindFlierGameMode` 设为默认 GameMode（Project Settings → Maps & Modes，或在关卡 World Settings 中覆盖）即可，它自带默认 Pawn。

### 操作

| 操作 | 键鼠 | 手柄 |
| --- | --- | --- |
| 飞行 | W / A / S / D | 左摇杆 |
| 升降 | Space / LeftCtrl | LB / RB |
| 视角 | 鼠标 | 右摇杆 |

如果给 Pawn 指定了可选的 Enhanced Input Actions（`MoveAction` / `LookAction`），会优先使用它们，否则自动退回 `Config/DefaultInput.ini` 里的 `WindFlier_*` 映射。

### 手感

飞行带拖拽：推力（`FlyAcceleration`）持续累加进速度，空气阻力（`AirDrag`）每帧按指数衰减，松手是滑行而不是急停；相机朝向用 `AimInterpSpeed` 追输入目标，并按侧向速度压 `BankAngleMax` 做滚转，形成轻微拖拽感。Pawn 全身没有任何碰撞体，会直接穿过场景。

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

## 砍竹（Procedural Mesh）

代码位于 [Source/Wind/Bamboo](../Source/Wind/Bamboo)。用 `UProceduralMeshComponent` 现场生成竹筒，砍断时把网格重建成树桩，并把上半段换成一段带物理的断竹。

| 放什么 | 作用 |
| --- | --- |
| `ABambooStalk` | **一根竹子**。把 Actor 拖进关卡就有竹筒，参数全在细节面板里。 |
| `ABambooCutPiece` | 砍断时自动生成的**上半段**，不用手摆：本地原点就是切口，**复用竹子的三个材质槽**，按砍击速度给物理冲量，`LifeSpanSeconds` 后自动销毁。 |
| `ABambooSword` | **剑**。一条 Box 当刀身（引擎自带 Cube 当外观），Actor 原点就是握把。角色不生成也不持有它，摆进关卡后由你自己驱动。 |

竹筒由 `FBambooGeometry::BuildSection` 生成，造型参考 Blender 的 *Cuttable Bamboo Generator*：

- **中空管壁**：外壁 + 内壁 + 上下环状切口（露出壁厚），不是封死的实心柱；
- **节环**：每个内部节点一处高斯隆起，节附近额外加密采样以保住节环形状；
- **节内隔膜**：每个内部节点上的一块薄盘，从切口看进去能看到；
- **自然感**：梢部收细 10%，叠加低频正弦的半径起伏与角度抖动，避免机械感。

网格分成三个材质槽，可直接挂不同材质：`0` 竹青外壁、`1` 内壁、`2` 切口截面（上下环带 + 节内隔膜）。顶点色也已按这三类写入（`OuterColor` / `InnerColor` / `CutColor`），法线、UV、切线都齐。

所有形状只由**绝对高度**决定，所以从任意高度砍断，切口两侧的半径、节环与纹理都能对上；砍下的那半段以切口为网格原点，和树桩严丝合缝。

`ABambooStalk`

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `Params.Height` | 500 | 整根高度（厘米），默认 5 m。 |
| `Params.BaseRadius` | 5 | 根部半径（直径 10 cm）。 |
| `Params.WallThickness` | 0.6 | 壁厚 6 mm；内壁半径 = 外壁半径 - 壁厚。 |
| `Params.NodeCount` | 8 | 节数。内部节点才有节环与隔膜，两端是竹子的两端口。 |
| `Params.SideCount` | 48 | 横截面边数，越大越圆（也越贵）。 |
| `Params.StepsPerBay` | 10 | 每个节间的纵向采样数，决定竹杆本身的光滑度。 |
| `Params.TaperRatio` | 0.1 | 梢部收缩比例。 |
| `Params.CollarStrength` / `CollarWidth` | 0.075 / 1.8 | 节环隆起高度与主隆起宽度。 |
| `Params.OuterColor` / `InnerColor` / `CutColor` | 竹青 / 竹黄 / 断面 | 三个材质槽的顶点色。 |
| `bUseCustomCutMesh` | true | 切法开关：`true` 用本项目自带的切面几何；`false` 用 `UKismetProceduralMeshLibrary::SliceProceduralMesh`。 |

砍法两种：

- 玩法代码调 `Cut(CutHeight, FallDirection)`（本地空间高度，水平切）、`CutAtLocation(WorldLocation, FallDirection)`（世界坐标命中点，水平切）或 `CutAlongPlane(WorldPoint, WorldNormal, FallDirection)`（任意斜切面）；三者都可以再带一个 `ImpactSpeed`（厘米/秒）当砍击速度，断竹的冲量就按它算。第二次调用直接返回 `nullptr`。
- **用剑砍**：`ABambooSword` 每帧把刀身 Box 从上一帧的变换扫到当前帧的变换（`SweepMultiByChannel`），刀梢速度超过 `MinCutSpeed` 才判定；扫到竹子时，**切面 = 刀身长轴与刀身在该点的运动方向张成的平面**，所以横向挥是平口、斜着削就是斜口，倒向取运动方向的水平分量（竖直劈砍时退化成"从剑指向竹子"）。

切口是真正的平面：几何按切面与每根母线求交，树桩与掉下的那一段共享同一圈边界点，拼回去严丝合缝；两段各自的环状截面也都落在该平面上。切面太竖（法线竖直分量 < 0.35）时会被夹回接近水平，保证竹子还能整段分离。

**两种切法**由 `bUseCustomCutMesh` 切换（默认开启）：

- **开启**：走上面这条自定义几何，斜切面精确、切口是中空的环、两段边界完全一致。
- **关闭**：直接调 `UKismetProceduralMeshLibrary::SliceProceduralMesh` 把整块程序化网格切开。它把**法线正侧**留在原组件里，所以内部传的是反向法线（竹子留下下半段）；切出来的另一半几何被搬进 `ABambooCutPiece`，材质、冲量、生命周期沿用同一条路。代价是引擎切片会把切口封成实心（中空的内圈也被填上），也没有节环/斜口的额外修形——正好可以用来做对照。

断竹的材质从竹子本体的 3 个材质槽原样复制，断口看起来和原来一致。物理这边不是"给个固定初速度"：冲量 = 质量 × 剑速 × `ImpactSpeedScale`，作用点取命中点在切面上的投影，所以砍得越快飞得越远、转得越猛，命中点偏离轴心时还会自然带出翻滚。

`ABambooCutPiece`

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `LifeSpanSeconds` | 8 | 倒下后存活时间，到点自动销毁。 |
| `ImpactSpeedScale` | 0.4 | 剑速换算成断竹初速度的比例。 |
| `MinLaunchSpeed` / `MaxLaunchSpeed` | 60 / 1200 | 初速度的下限/上限（厘米/秒），防止轻碰不动或极速击飞。 |
| `DensityGramsPerCubicCm` | 0.7 | 竹材密度，用来按中空管壁的真实体积算质量。 |

一个坑：`UProceduralMeshComponent` 默认用 ComplexAsSimple（trimesh）碰撞，而引擎不会把这种刚体当动态物体——`SetSimulatePhysics` 只会在 PIE 里打一条 `Trying to simulate physics on ... but it has ComplexAsSimple collision` 然后什么都不做。所以断竹会先关掉 `bUseComplexAsSimpleCollision`、沿高度切成 8 段凸包当简单碰撞，再开物理；质量也不能用引擎按凸包算出的值（凸包是实心的，会把中空竹段算成几十公斤的实心柱），而是按网格闭合体积 × `DensityGramsPerCubicCm` 覆盖。

剑不会自己动：角色不生成、不挂载、也不绑定任何输入。把它摆进关卡后，用关卡蓝图、Sequencer 或你自己的代码移动/旋转它就行——每帧的扫掠会自动把相邻两帧连成一段体积，挥得再快也不会漏判。也可以直接调自带的 `Swing()` 让它自己挥一下（绕握把本地 Z 轴转 `SwingAngle` 度再回位，`SwingDuration` 秒）。

`ABambooSword`

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `BladeLength` / `BladeWidth` / `BladeThickness` | 100 / 4 / 1.2 | 刀身尺寸（厘米）。Box 与外观同步，握把在 Actor 原点。 |
| `MinCutSpeed` | 120 | 刀梢速度低于这个值不判定，避免把剑靠在竹子上也算砍。 |
| `SwingAngle` / `SwingDuration` | 120 / 0.25 | `Swing()` 的角度与时长。 |
| `bDrawDebug` | false | 画出每帧的刀身 Box 与扫掠线段。 |

倒下的那段不是动画：网格建好后直接 `SetSimulatePhysics(true)`，之后的质量、坡度、碰撞都交给物理。

几何生成是纯函数（`FBambooGeometry`），自动化测试 `Wind.Bamboo.Geometry.*` 校验分段完整性、壁厚、节环隆起、节内隔膜、"砍断后切口两侧一致、掉落段以切口为原点"，以及斜切面下两段分别落在平面两侧且共享同一圈切口边界，不需要起世界；`Wind.Bamboo.Sword.BladeShape` 校验剑确实是一条从握把伸出的细长 Box。



