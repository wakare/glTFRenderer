# Feature Note - 2026-03 ModelViewer 间接光方案调研

## 1. 目的

本文记录当前与 `DemoAppModelViewer` 相关、且在业界仍然主流的实时间接光方案，并把它们收束成适合本仓库的实现优先级。

目标不是立刻拍板最终方案，而是先明确：

- 业界现在常见的落地路径是什么，
- 每种方案对渲染器的输入依赖和系统要求是什么，
- 哪一种最适合当前 `glTFRenderer` 的架构状态。

## 2. 当前仓库基线

目前 `ModelViewer` 的光照链路仍然是“直接光为主”：

- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
  中的 `GetLighting(...)` 主要按灯列表累加直接光 BRDF 与阴影可见性。
- `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
  读取 SSAO buffer，并把它作用在一项很小的 ambient 占位项上。

当前已经具备的基础：

- 来自 `RendererSystemSceneRenderer` 的 deferred 风格场景缓冲，
- 可用于 SSAO 和未来 screen-space GI 的 normal / depth，
- DX12 与 Vulkan 后端，
- 以 compute pass 为主的 render graph 风格，适合后续扩展屏幕空间或 lighting compute pass。

当前还不具备的能力：

- 环境 IBL 漫反射 / 镜面反射，
- reflection probe 或 irradiance probe 基础设施，
- lightmap 或 baked probe volume 工作流，
- screen-space GI 所需的时间历史与重建体系，
- ray tracing acceleration structure 路径，
- sparse distance field / radiance cache GI 基础设施。

这意味着当前这版 SSAO 只是一个过渡步骤，还不是完整的间接光方案。

## 3. 业界主流方案族

### 3.1 Image-Based Lighting（IBL）与 Reflection Probes

这是目前 PBR model viewer、产品展示类 viewer，以及大量游戏引擎里最基础、最常见的间接光层。

典型形态：

- 漫反射间接光来自 irradiance，
- 镜面间接光来自预过滤环境贴图，
- 室内或局部变化可叠加 local reflection probe，
- SSAO 只衰减 diffuse indirect，不参与直接光。

它为什么仍然主流：

- 实现风险低，
- 画面稳定，
- 和 glTF / PBR 工作流天然契合，
- 非常适合模型浏览、look-dev、静态环境展示。

它需要的输入：

- HDR 环境输入，
- irradiance 生成或 SH 投影，
- prefiltered specular cubemap 与 BRDF LUT，
- 在 lighting pass 中完成接线。

它不擅长解决的问题：

- 近处动态几何带来的局部漫反射反弹，
- 大型室内场景的强烈多次反射色漂移，
- 动态 emissive 对场景产生的实时 GI。

### 3.2 Baked Lightmaps 加 Probe Volumes

对于几何和光照大体静态的场景，这仍然是非常常见的工业落地方案。

典型形态：

- 静态几何走 baked lightmap，
- 动态物体走 light probes / adaptive probe volumes，
- 间接镜面反射走 reflection probes 或 sky lighting，
- SSAO 作为局部接触阴影补偿。

它为什么仍然主流：

- 性价比极高，
- 性能稳定，
- 编辑器与内容工作流成熟。

它需要的前提：

- 预计算烘焙流程，
- lightmap / probe 资产管理，
- 大场景下的流送与混合规则。

它不擅长解决的问题：

- 高动态场景，
- 可破坏环境，
- 运行时大幅改灯而不重新烘焙。

### 3.3 Screen-Space GI（SSGI）

如果一个渲染器已经具备 G-buffer 和时间历史，SSGI 往往是继 SSAO 之后最自然的动态 GI 增量方案。

典型形态：

- 基于 depth / normal / color 在屏幕空间做 ray march，
- 只求 diffuse bounce，
- 用 temporal reprojection 和 denoise 做稳定化，
- 与已有 ambient / sky / probe 间接光混合。

它为什么主流：

- 接入成本相对较低，
- 能直接复用 deferred buffer，
- 对屏幕内的局部反弹、接触处色漂移很有效，
- 不依赖 RT 硬件。

它需要的基础：

- depth、normal、scene color 或 lighting history，
- motion vectors 与 temporal stabilization，
- history rejection 与 denoise 体系，
- 比较细致的 artifact 管控。

它不擅长解决的问题：

- 屏幕外的光照贡献，
- 薄物体和遮挡切换导致的数据缺失，
- 没有 probe/IBL 辅助时的大尺度室内稳定 GI。

### 3.4 Probe-Based Dynamic GI（DDGI / Probe Volumes）

这类方案现在是“中档成本、但能支持场景尺度动态漫反射 GI”的很强候选。

典型形态：

- 在世界空间布置 probe 或体积探针，
- 采集或追踪 incident lighting 到 probe，
- 以 irradiance 或 SH 的形式存储，
- lighting 时对 probe 进行采样，
- 按需增量更新。

它为什么主流：

- 能处理屏幕外间接光，
- 相比 SSGI 更适合场景尺度稳定性，
- 可做多档质量缩放，
- 相比 full RTGI 更容易工业落地。

它需要的基础：

- probe 放置和更新策略，
- irradiance 存储与混合，
- leak 处理与可见性启发式，
- 场景更新调度，
- 以及可选的 RT、SDF tracing 或 raster 辅助采样。

它不擅长解决的问题：

- 高频、锐利的间接光细节，
- 完全精确的 glossy 多次反射，
- 超低维护成本的大世界自动化流程。

### 3.5 完整动态 GI 体系（Lumen / Brixelizer GI / RTGI 家族）

业界高端趋势正在转向“多种场景表示 + tracing + cache”混合式动态 GI，而不是单一 pass 解决全部问题。

典型组件：

- screen traces，
- software / hardware ray tracing，
- distance fields 或 scene cards，
- radiance caches，
- probe-like 或 cache-like 的时空复用结构。

公开代表：

- Unreal Engine 5 的 Lumen，
- AMD FidelityFX Brixelizer GI，
- 各类基于硬件 RT + temporal cache 的 RTGI 体系。

它为什么越来越流行：

- 场景尺度 bounce lighting 更强，
- 动态灯光、动态 emissive 处理更好，
- 更接近现代 AAA 的 fully dynamic lighting 目标。

它需要的代价：

- 明显更多的渲染器基础设施，
- 更复杂的几何加速表示，
- 时间缓存与 denoiser，
- 更严格的内存与更新预算，
- 更强的调试体系。

它不适合作为本仓库的第一步原因：

- 实现复杂度高，
- 对低端硬件 fallback 设计要求高，
- 对当前 renderer 的重构面过大。

## 4. 当前业界阅读结论

截至 2026-03，从主流引擎和 GPU 厂商公开资料看，比较清晰的结论是：

1. IBL 仍然是 PBR renderer 和 model viewer 的基础间接光层。
2. Lightmap + probes 在静态或半静态场景里依然非常常见。
3. SSGI 仍然是 deferred renderer 很常见的动态 GI 增量方案。
4. DDGI / probe volumes 仍然是“中等成本、但支持场景尺度间接光”的实用路线。
5. 高端引擎正在向混合式 GI 系统演进，而不是单一 SSAO/SSGI 式方案。

换句话说：

- 对 viewer 和相对简单的 runtime，第一缺口依然是 IBL；
- 对中等成本动态 GI，SSGI 和 probe GI 是现实候选；
- 对高端 fully dynamic GI，行业方向是 tracing + cache 的混合系统。

## 5. 结合本仓库的对比

| 方案 | 当前行业角色 | 场景动态性 | 常见输出 | 在本仓库中的接入成本 | 主要风险 |
|---|---|---|---|---|---|
| IBL + reflection probes | 基础层，极常见 | 低到中 | 漫反射间接光 + 镜面间接光 | 低 | 局部 bounce 不强 |
| baked lightmaps + probes | 静态 / 混合场景常见 | 低 | 稳定 diffuse GI | 脱离编辑器时成本高 | 需要烘焙工作流 |
| SSGI | 常见的动态 GI 增量方案 | 中 | 主要是 diffuse indirect | 中 | 屏幕内限定、时序伪影 |
| DDGI / probe volumes | 实用型动态 GI 中档方案 | 中到高 | 场景尺度 diffuse indirect | 中到高 | probe leak 与更新复杂度 |
| Lumen / Brixelizer GI / RTGI | 高端趋势 | 高 | 大尺度 diffuse，部分还含 specular GI | 很高 | 系统范围过大 |

## 6. 对 glTFRenderer 的建议实现顺序

### 6.1 第一优先级

先补环境间接光：

- HDR sky / cubemap 输入，
- irradiance diffuse，
- prefiltered specular IBL，
- BRDF LUT，
- 让现有 SSAO 只作用于 diffuse indirect。

优先这样做的原因：

- 它先补上了当前 `ModelViewer` 最大的物理正确性缺口，
- 它是 glTF / PBR viewer 的基础预期能力，
- 它能让现有 SSAO 回到正确语义位置，
- 它不会过早把仓库拖入重型 GI 架构。

### 6.2 第二阶段候选

在 IBL 完成之后，再从下面两条里选一条：

- 如果目标是以较低到中等成本获得动态 bounce：
  SSGI 是最直接的下一步。
- 如果目标是获得更稳定、支持屏幕外贡献的场景尺度 diffuse GI：
  DDGI / probe volumes 是更强的长期候选。

### 6.3 不建议作为第一步的方案

下面这些不适合作为本仓库的第一个间接光实现：

- Lumen 类混合 GI，
- Brixelizer 级 sparse-distance-field GI，
- 以硬件 RTGI 为起点的完整动态 GI 架构。

它们是很重要的长期参考，但对当前 renderer 的基础设施要求明显过高。

## 7. 建议决策路径

对本仓库最稳妥的路线是：

1. 先做环境 IBL 漫反射 + 镜面反射。
2. 把 SSAO 从“占位 ambient”迁到真正的 diffuse indirect 上。
3. 再决定第二步是：
   - SSGI，还是
   - probe-based GI。
4. 只有当 renderer 具备了下面这些基础后，再考虑高端混合 GI：
   - 稳定的 history 管理，
   - 更完善的间接光 debug view，
   - 更明确的屏幕外场景表示策略。

## 8. 参考资料

- [Epic Games: Lumen Technical Details in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/lumen-technical-details-in-unreal-engine)
- [Epic Games Tech Blog: Unreal Engine 5 goes all-in on dynamic global illumination with Lumen](https://www.unrealengine.com/en-US/tech-blog/unreal-engine-5-goes-all-in-on-dynamic-global-illumination-with-lumen)
- [Unity HDRP Features Overview](https://docs.unity.cn/Packages/com.unity.render-pipelines.high-definition%4017.0/manual/HDRP-Features.html)
- [Unity HDRP: Screen Space Global Illumination](https://docs.unity.cn/Packages/com.unity.render-pipelines.high-definition%4016.0/manual/Override-Screen-Space-GI)
- [NVIDIA RTXGI-DDGI](https://github.com/NVIDIAGameWorks/RTXGI-DDGI)
- [AMD GPUOpen: FidelityFX Brixelizer / GI](https://gpuopen.com/fidelityfx-brixelizer/)
- [AMD GPUOpen Manual: FidelityFX Brixelizer GI](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/brixelizer-gi/)
- [Google Filament: Image Based Lights](https://google.github.io/filament/main/filament.html)
