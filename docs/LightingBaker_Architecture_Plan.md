# LightingBaker 架构与阶段计划

> 本文用于规划一个独立于 `RendererDemo` 的新可执行程序 `LightingBaker`。目标是在当前维护中的基础设施上，逐步实现基于 path tracing 的 lightmap 间接光烘焙。

共享关键类说明图：

- `docs/LightingBaker_KeyClass_Diagram.md`
- 英文配套版本：`docs/LightingBaker_Architecture_Plan_EN.md`

## 1. 目标与边界

### 1.1 目标

- 新建独立工程 `glTFRenderer/LightingBaker`，而不是把烘焙逻辑继续塞进 `RendererDemo`。
- 初版目标是支持静态场景 lightmap 烘焙，核心求解器使用 path tracing。
- 初版优先复用当前维护路径：`glTFRenderer/RendererCore`、`glTFRenderer/RHICore`、`glTFRenderer/RendererScene`。
- 已有旧实现中的 DXR / ray tracing pass、HLSL 采样逻辑、AS/shader-table 组装逻辑可以复用思路，但不再把老 `glTFApp` / `glTFRenderPass` 体系作为新工程主骨架。

### 1.2 非目标

- 不把 `LightingBaker` 做成 `RendererDemo` 的一个命令行 mode 或 demo 子模式。
- 不在第一阶段就同时解决自动 unwrap、真正 headless RHI、局部脏区增量重烘、动画烘焙、降噪器接入。
- 第一阶段支持 progressive accumulation / pause / resume，但不把“渐进式预览”与“局部增量重烘”混为一谈。
- 不为了规避框架缺口而退回旧 `glTFApp` 路径继续扩展维护范围。

### 1.3 第一阶段运行方式建议

当前现代运行时仍然以 swapchain / window 为中心，`ResourceManager` 初始化直接走 `HWND` 和 swapchain 创建。因此第一阶段建议：

- 采用“独立可执行程序 + 隐藏窗口或最小窗口 + 最小 swapchain + 离屏烘焙输出”的方案。
- 等 lightmap 路径打通后，再把“无 swapchain 的真正 headless 模式”作为第二阶段框架清理目标。

这样做的原因不是算法需要窗口，而是当前维护中的运行时主链还没有被完全拆成无窗口模式。

## 2. 现有基础设施能复用什么

### 2.1 可以直接复用的部分

- `glTFRenderer/RHICore`
  - 已经有 `IRHIRayTracingAS`、`IRHIShaderTable`、RT pipeline state、底层 `TraceRay(...)`。
- `glTFRenderer/RendererCore`
  - 已经支持 `RenderPassType::RAY_TRACING`。
  - `RenderGraph` 已经有现代资源生命周期、依赖推导、执行调度、GPU profiler、统一提交逻辑。
- `glTFRenderer/RendererScene`
  - 已经提供 glTF 场景导入、`RendererSceneGraph`、scene bounds、mesh/material 遍历。
- `glTFRenderer/RendererCore::RendererSceneResourceManager`
  - 已经把场景数据导出到现代 `RendererModule`/`RendererSystem` 路径可消费的 mesh accessor 接口上。

### 2.2 只能作为参考、不适合作为新主骨架的部分

旧 `glTFRenderer` 工程中的 path tracing 代码仍然有价值，但更适合作为“算法参考源”：

- `glTFRenderer/glTFRenderer/glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassPathTracing.cpp`
- `glTFRenderer/glTFRenderer/glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassWithMesh.cpp`
- `glTFRenderer/glTFRenderer/glTFResources/ShaderSource/RayTracing/PathTracingMain.hlsl`
- `glTFRenderer/glTFRenderer/glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl`
- `glTFRenderer/glTFRenderer/glTFResources/ShaderSource/RayTracing/LightSampling.hlsl`

原因是这条旧链路目前有两个关键问题：

- 它属于老 `glTFApp` / `glTFRenderPass` 路径，不是当前维护主线。
- 现有 path tracing shader 明确是屏幕域 / camera 域实现：
  - 依赖 `SceneView.hlsl`
  - 使用 `DispatchRaysIndex -> screen uv`
  - 通过 `inverseProjectionMatrix` / `inverseViewMatrix` 生成屏幕空间主射线
  - 还输出 `screen_uv_offset`

lightmap baker 需要的是 atlas texel 域，而不是屏幕像素域。

## 3. 关键框架缺口

### 3.1 UV1 / lightmap UV 数据通路缺口

当前现代路径里，`MeshDataAccessorType` 只有：

- `VERTEX_POSITION_FLOAT3`
- `VERTEX_NORMAL_FLOAT3`
- `VERTEX_TANGENT_FLOAT4`
- `VERTEX_TEXCOORD0_FLOAT2`
- index / instance 数据

也就是说当前没有 `VERTEX_TEXCOORD1_FLOAT2`。同时：

- `glTFRenderer/RendererScene/Private/RendererSceneGraph.cpp` 目前只装载 `TEXCOORD_0`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp` 的 `RendererSceneResourceManager::AccessSceneData(...)` 也只导出 UV0
- `glTFRenderer/RendererDemo/RendererModule/RendererModuleSceneMesh.cpp` 也只消费 `VERTEX_TEXCOORD0_FLOAT2`

这会直接阻塞 lightmap 使用单独 UV 集。

补充说明：

- `UV1` 存在不等于“可直接用于 lightmap 烘焙”
- baker 需要验证 overlap、padding、退化三角形、越界 UV、目标分辨率下的 texel 覆盖
- `LightmapAtlasBuilder` 不能只检查 attribute 是否存在，还要生成 `valid texel mask`
- MVP 阶段若 authored UV1 不合法，应该 fail-fast 并输出问题 primitive / chart，而不是悄悄回退到错误结果

### 3.2 现代 RT 执行链缺口

当前现代 `RendererCore` 虽然已经支持：

- `RenderPassType::RAY_TRACING`
- `ExecuteCommandType::RAY_TRACING_COMMAND`

但真正执行时还没有打通：

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp` 中 `RenderGraph::ExecuteRenderGraphNode(...)`
- `ExecuteCommandType::RAY_TRACING_COMMAND` 分支目前是空实现

除此之外，现代描述层也还缺少 ray tracing 特有数据：

- `RenderExecuteCommand` 没有 trace size / shader table / callable layout 等专用参数
- `RenderPassDrawDesc` 只有 render targets / buffers / textures，没有 TLAS 或 shader table 的标准绑定入口

所以如果 `LightingBaker` 走维护中的现代路径，必须先把这条链补完整。

### 3.3 离线结果导出缺口

当前工程已有的可见输出能力主要是：

- `RenderGraph` 将 final output blit 到 swapchain
- `RendererDemo` 通过窗口截图写 PNG

但对 lightmap baker 来说，真正需要的是：

- 指定 bake texture 的 GPU readback
- HDR 纹理写盘
- 输出 manifest / metadata
- 中间结果调试导出

这部分目前不能直接复用 `RendererDemo` 的窗口截图路径，需要新建离线 readback 能力。

## 4. 建议的模块划分

建议 `LightingBaker` 初版在一个独立项目内拆成以下子模块：

| 模块 | 责任 | 备注 |
| --- | --- | --- |
| `App/LightingBakerApp` | 程序入口、CLI、任务调度、隐藏窗口模式、批处理执行。 | 第一阶段宿主。 |
| `App/BakeJobConfig` | 解析 job json / CLI 参数，定义 scene、output、resolution、samples、bounce、environment 等配置。 | 后续可扩成批量 job。 |
| `Scene/BakeSceneImporter` | 封装 `RendererSceneResourceManager`，抽取 mesh/material/light/emissive 信息，统一生成 baker 内部视图。 | 必须站在现代维护路径上。 |
| `Bake/Atlas/LightmapAtlasBuilder` | MVP 验证现有 `TEXCOORD_1` 的 overlap、padding、coverage；后续扩展 unwrap/chart/atlas build。 | 第一阶段先不做自动 unwrap。 |
| `Bake/Scene/BakeTexelScene` | 生成 texel -> triangle 映射、barycentric、world position、normal、tangent、material id、chart id、valid mask。 | 是从屏幕域 path tracing 转到 atlas 域的核心桥接层。 |
| `Bake/RT/BakerRayTracingScene` | 构建 BLAS/TLAS、geometry table、instance table、shader table 依赖、材质与灯光缓冲。 | 参考旧 `glTFRayTracingPassWithMesh` 的组装逻辑。 |
| `Bake/Passes/LightmapPathTracingPass` | 执行 atlas 域 raygen，做 direct lighting、environment、multi-bounce accumulation。 | 核心求解器。 |
| `Bake/Post/BakeAccumulator` | 管理 progressive accumulation、收敛计数、pause / resume 状态、无效 texel 修补、边缘 dilation。 | 初版不一定带 denoiser。 |
| `Output/BakeOutputWriter` | 做 GPU readback、发布包写出、调试导出、结果清单和 sidecar 元数据。 | 离线输出主入口。 |
| `Debug/BakeDebugDumps` | 导出中间 buffer/texture，例如 texel validity、chart id、sample count、first bounce radiance。 | 便于定位 seam、漏光、无效 texel。 |

建议目录：

```text
glTFRenderer/LightingBaker/
  LightingBaker.vcxproj
  App/
  Scene/
  Bake/
    Atlas/
    Scene/
    RT/
    Passes/
    Post/
  Output/
  Debug/
  Resources/
    Shaders/
```

## 5. 对现有框架的改动清单

### 5.1 `glTFRenderer/RendererCore`

这是新工程落地时最重要的一层改造。

#### 必改

1. 给现代 RT pass 增加真正的 dispatch 描述能力

- 为 `RenderExecuteCommand` 增加 ray tracing 专用参数，至少要能表达：
  - trace width / height / depth
  - shader table 句柄或 pass 内部持有对象的引用方式

2. 在 `RenderGraph::ExecuteRenderGraphNode(...)` 中真正执行 RT command

- 当前 `RAY_TRACING_COMMAND` 分支是空的
- 需要最终调用 `RHIUtilInstanceManager::Instance().TraceRay(...)`

3. 增加现代 RT 资源绑定入口

- 当前 `RenderPassDrawDesc` 只有 buffer / texture / render target 资源
- 需要补齐 acceleration structure / shader table 或等价描述

4. 增加 bake 纹理 readback 能力

- 将 GPU texture 拷贝到 staging/readback buffer
- 提供 map / save / format conversion 流程

#### 建议但可延后

- 为 `RenderGraph` 加一个无 swapchain 的离线模式
- 让 final output blit 和 present 完全可选

### 5.2 `glTFRenderer/RendererScene` 与 `glTFRenderer/RendererCore::RendererSceneResourceManager`

这里主要是为 lightmap UV 和 baker 所需几何数据补齐通路。

#### 必改

1. 增加 `TEXCOORD_1` 解析与存储

- `glTFRenderer/RendererScene/Private/RendererSceneGraph.cpp`
- 对应 vertex layout / accessor 也要补齐

2. 扩展 `MeshDataAccessorType`

- 新增 `VERTEX_TEXCOORD1_FLOAT2`

3. 扩展 `RendererSceneResourceManager::AccessSceneData(...)`

- 导出 UV1
- 后续可继续导出 emissive 相关标记、双面材质标记、alpha 模式等 bake 相关信息

#### 建议但可延后

- 为自动 unwrap 预留 sidecar 数据结构
- 增加 per-triangle 面积、chart id、triangle-to-material 分组等便于 baking 的辅助访问接口

### 5.3 `glTFRenderer/RHICore`

`RHICore` 本身已有：

- RT PSO
- shader table
- BLAS/TLAS
- `TraceRay`

所以这层不需要先做大改。但仍可能需要两类补充：

- texture readback / staging helper 的便利接口
- 后续 headless 模式下的 device / swapchain optional 初始化整理

### 5.4 `glTFRenderer/RendererDemo`

`LightingBaker` 不应该挂在 `RendererDemo` 里，但如果现代公共接口变更，`RendererDemo` 需要做跟随性修正：

- `RendererModuleSceneMesh`
- `SceneRendererCommon.hlsl`
- 任何实现了 `RendererSceneMeshDataAccessorBase` 的模块

这类改动是“接口跟随”，不是“把烘焙功能做进 Demo”。

运行时导入策略建议直接按 sidecar package 接入，而不是第一阶段回写原始 glTF：

- `RendererDemo` 在加载 scene 后，优先尝试查找 `<scene_stem>.lmbake/manifest.json`
- 兼容回退可以再查 `<scene>.lmbake/manifest.json`
- lightmap 绑定应按 primitive 或 instance 建立，不应按 material 建立
- binding 不能依赖运行时自增 `mesh_id` / `node.GetID()`，应使用稳定 key
- 当前更稳妥的键设计是：`primitive_hash`，以及可选的实例覆盖键 `node_key`
- 运行时需要新增 `RendererModuleLightmap` 或等价模块，管理 atlas 纹理和 binding table
- 几何/光照阶段读取 `UV1 + lightmap_binding_index`，合成 `direct_dynamic + baked_diffuse_indirect + specular_indirect`
- 已烘焙的 diffuse indirect 应避免与现有 environment diffuse 重复累计

## 6. Lightmap 路径的建议数据流

建议的数据流如下：

1. `BakeSceneImporter`
   - 加载 scene
   - 收集 mesh / material / light / emissive / bounds
2. `LightmapAtlasBuilder`
   - MVP：验证所有参与烘焙的 primitive 具备有效 UV1
   - 生成 atlas resolution、padding、chart metadata
3. `BakeTexelScene`
   - 把 atlas texel 映射到实际三角形
   - 为每个有效 texel 生成：
     - primitive id
     - barycentric
     - world position
     - geometric normal / shading normal
     - tangent frame
     - material id
     - chart id
4. `BakerRayTracingScene`
   - 构建 BLAS/TLAS
   - 上传 geometry / material / light / texel records
   - 建立 shader table
5. `LightmapPathTracingPass`
   - 在 atlas texel 域 dispatch ray
   - 首射线直接从 texel 的 world position + normal bias 发射
   - 复用现有 direct light / RIS / BRDF 逻辑，但把 raygen 改为 atlas 域
6. `BakeAccumulator`
   - 多次 sample 迭代
   - 记录 sample count / invalid texel / convergence
   - 支持 progressive 预览与 pause / resume
   - 做 dilation / seam repair
7. `BakeOutputWriter`
   - readback
   - 写发布包与 cache
   - 产出 debug 附件

### 6.1 产出包设计

建议把 baker 产出拆成两层：

- `cache`：只给 baker 自己恢复 progressive accumulation 使用
- `published package`：给 `RendererDemo` 或其他运行时导入

建议目录：

```text
<scene_stem>.lmbake/
  manifest.json
  atlases/
    indirect_00.master.rgba16f.bin
    indirect_00.runtime.logluv8.bin
    indirect_00.preview.png
  debug/
    coverage_00.png
    chart_id_00.png
    invalid_uv.png
  cache/
    resume.json
    texel_records_00.bin
    accum_00.rgb32f.bin
    sample_count_00.r32ui.bin
    variance_00.r32f.bin
```

`manifest.json` 需要至少覆盖：

- schema version
- source scene path / bake id / profile
- integrator 配置，例如 target spp、max bounces
- validation hash，例如 geometry / material / UV1 hash
- atlas 列表、尺寸、格式、语义
- primitive 或 instance 到 atlas 的 binding 列表
- 稳定绑定键，例如 `primitive_hash`，以及可选的实例覆盖键 `node_key`
- runtime codec 与 decode 参数

`resume.json` 只给 baker 自己使用，应作为 `cache/` 的权威索引入口，至少记录：

- `atlas_inputs`
- 每个 atlas 的 `texel_record_file`、`texel_record_count`、`texel_record_stride`
- progressive 缓存文件，例如 `accumulation_file`、`sample_count_file`、`variance_file`
- 当前 progressive 状态，例如 `completed_samples`

当前已落地的 scaffold 会额外输出：

- `debug/import_summary.json`
- `debug/atlas_summary.json`
- `cache/texel_records_00.bin`
- `cache/accum_00.rgb32f.bin`
- `cache/sample_count_00.r32ui.bin`
- `cache/variance_00.r32f.bin`

当前已落地到可运行状态的是“`--resume` 校验 + progressive 继续累积的占位实现”：

- 重新导入场景并重建 atlas texel records
- 校验 `resume.json`、texel record count、cache 文件尺寸、关键 bake 参数是否一致
- `accum` / `sample_count` / `variance` cache 按 atlas 像素域分配，而不是按 valid texel count 分配；这样 cache 文件布局与 published atlas 共享同一分辨率约定
- 校验通过后，使用 atlas texel 域的 `debug hemisphere` 占位积分器继续追加 sample count，并刷新 `manifest.json`、`resume.json` 与 published atlas
- 达到 target sample 后不再继续推进 cache，仅保持 metadata 与 published atlas 一致

真正的 DXR atlas-domain path tracing pass 仍属于后续阶段；当前占位积分器只用于验证 progressive bake、cache layout、sidecar package 与 runtime import 这条基础链路。

后续即便 cache 内部格式调整，也应继续由 `resume.json` 提供唯一入口，避免 DXR bake pass 或工具链直接硬编码具体文件名。

### 6.2 压缩与编码策略

间接光通常是低频、平滑信号，因此不应只从“文件压缩”角度设计，而应分成两层：

- 信号层：优先存 `diffuse_irradiance`，不要把高频 direct lighting 与其混合
- 编码层：给运行时准备更紧凑的 codec，但保留高精度 master

推荐顺序：

1. `cache` 始终保留高精度，不做有损压缩
2. `published master` 先使用 `RGBA16F`
3. `runtime payload` 第二步补 `LogLuv8` 或 `YCoCg8` 一类低频友好的编码
4. 后续如果底层补齐 compressed texture format，再扩展 `BC6H`

第一阶段不要把运行时链路直接建立在 EXR 解码上；更稳妥的是：

- master：`*.rgba16f.bin`
- runtime：`*.logluv8.bin` 或暂时仍用 master
- preview：`*.png`

## 7. 分阶段实现建议

### 阶段 A: 工程与运行时脚手架

- 新建 `LightingBaker.vcxproj`
- 接入 `RendererCore`、`RHICore`、`RendererScene`、`RendererCommonLib`
- 做最小 CLI 和隐藏窗口运行模式

验收标准：

- 可启动程序
- 可加载场景
- 可创建现代渲染上下文

### 阶段 B: 现代 RT 执行链补全

- 补 `RAY_TRACING_COMMAND` 参数与执行
- 打通 shader table / TLAS 绑定
- 做一个最小 RT test pass

验收标准：

- `RenderGraph` 中可稳定执行一个现代 RT node
- 能输出测试纹理，不经过旧 `glTFApp`

### 阶段 C: UV1 与 bake texel 数据链

- 补 `TEXCOORD_1`
- 构建 atlas texel records
- 输出 debug 图：valid mask / chart id / texel world normal

验收标准：

- atlas 域调试图正确
- seam / invalid texel 分布可见

### 阶段 D: lightmap path tracing MVP

- 先支持 authored UV1
- 先支持静态 mesh
- 先实现 direct lighting + environment + diffuse bounce
- 支持全场景 progressive accumulation / pause / resume
- 在 DXR bake pass 落地前，先保留 atlas texel 域 `debug hemisphere` 占位积分器，用于验证 cache / package / runtime 合同

验收标准：

- 能在简单场景里得到稳定的 HDR lightmap
- 多 bounce 结果可见
- 低 sample 下也能持续预览收敛过程

### 阶段 E: 输出与工程化

- GPU readback
- master HDR 输出
- runtime codec 输出
- debug artifacts
- bake manifest
- progressive cache / resume 文件

验收标准：

- 一次命令行调用能产出 lightmap 文件、cache 和元数据

### 阶段 F: 扩展项

- 自动 unwrap / charting
- 真正 headless 模式
- 增量烘焙
- 降噪器
- emissive / transmission / portal / importance sampling 提升

## 8. 初版范围建议

为了尽快跑通第一版，建议初版先约束到以下范围：

- 静态场景
- 静态光源
- 优先支持作者提供的 UV1
- 一张场景 lightmap 或少量 atlas 集
- HDR 主输出 + PNG 调试输出
- 全场景 progressive bake
- authored UV1 非法时显式报错，不做静默回退
- 先不支持 skinned mesh / 动画 / 自动 unwrap / denoise

这个范围不代表长期行为收缩，而是为了尽快把“独立 baker + 现代 RT 主链 + lightmap 输出”这三个核心点先打通。

## 9. 当前结论

从现有代码基础看，`LightingBaker` 已经从“纯规划”进入“可运行 scaffold”阶段：独立工程骨架、UV1/lightmap 绑定通路、atlas texel records、sidecar writer、runtime importer、`--resume` 校验，以及 atlas texel 域的 progressive 占位积分器都已经打通。

当前剩余的关键主线缩减为三条：

1. 现代 `RendererCore` 的 RT command 执行链
2. 用真正的 DXR atlas-domain path tracing pass 替换当前 `debug hemisphere` 占位积分器
3. 补齐 GPU readback 与真实 bake radiance 输出，而不是继续依赖 CPU 占位结果

现在这条 placeholder progressive 管线的价值，不是替代 DXR，而是先把 cache / package / runtime contract 固定住。这样后续迁入已有 DXR path tracing 初版算法时，会更接近“替换求解器”，而不是重写整条链路。
