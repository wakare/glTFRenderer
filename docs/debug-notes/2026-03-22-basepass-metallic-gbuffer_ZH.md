# Debug Note - BasePass 的 metallic 实际上没有写进 GBuffer

- Status: Accepted
- Date: 2026-03-22
- Scope: `RendererDemo -> BasePass / 材质导入 / GBuffer metallic-roughness 打包`
- Commit: `(working tree, not committed yet)`
- 配套文档：
  - `docs/debug-notes/2026-03-22-basepass-metallic-gbuffer.md`

## Symptom

在 `RendererDemo` 里，`SceneLighting.hlsl` 会把 `albedoTex.w` 当作 metallic 读取，但实际观察到的 BasePass 输出看起来像是这个 `w` 通道根本没有真正装入材质 metallic。

## Reproduction

- 使用当前维护中的 `RendererDemo` BasePass 路径：
  - `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- 把 BasePass 写入路径和 lighting 读取路径一起看。
- 对照预期打包约定：
  - `albedoTex.w -> metallic`
  - `normalTex.w -> roughness`
- 然后继续往前追材质来源链路：
  - `glTFRenderer/RendererScene/Private/RendererSceneGraph.cpp`
  - `glTFRenderer/RendererDemo/RendererModule/RendererModuleMaterial.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleMaterial.hlsl`

## Wrong Hypotheses Or Detours

### Detour 1: 问题只出在 lighting 读取端

- Why it looked plausible:
  `SceneLighting.hlsl` 里明确写着 `float metallic = albedo_buffer_data.w;`，所以第一反应很容易是 compute lighting pass 绑定错了资源，或者读错了通道。
- Why it was not the final cause:
  lighting 读端本身读的就是 BasePass 写进去的值。真正的问题更早：BasePass 根本没有把 metallic 写到 `SV_TARGET0.w`，roughness 也没有写到 `SV_TARGET1.w`。

### Detour 2: 问题只是在 metallic-roughness 纹理采样没有生效

- Why it looked plausible:
  `SampleMetallicRoughnessTexture(...)` 只返回了纹理 `.bg`，对于同时带有 glTF `metallicFactor` 和 `roughnessFactor` 的材质来说，这本身就很可疑。
- Why it was not the final cause:
  这只是问题的一半。就算采样函数返回了正确的 metallic/roughness，`ModelRenderingShader.hlsl` 仍然会把纯 albedo 写进 `output.color`，并把 `output.normal.w` 固定留在 `0.0`。

## Final Root Cause

同一条数据链上有两个断点，叠在一起才让 BasePass 的 metallic 通道看起来像“读不到”：

1. `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl` 没有把 metallic 和 roughness 写进 GBuffer。`output.color` 直接等于 `SampleAlbedoTexture(...)`，所以 `SV_TARGET0.w` 实际上装的是 base-color alpha，而不是 metallic；`SV_TARGET1.w` 也一直是 `0.0`，不是 roughness。
2. `RendererDemo` 的材质路径把 base color 和 metallic-roughness 建模成了“要么 texture，要么 factor”，而不是保留 glTF 语义要求的“texture 和 factor 并存并相乘”。一旦材质存在纹理，`baseColorFactor`、`metallicFactor` 和 `roughnessFactor` 就会在 GPU 材质缓冲构建前被丢掉。

## Final Fix

- 给 `MaterialParameter` 增加了一个同时携带 texture 和 factor 的构造方式，使 `RendererScene` 导入层可以把两者一起保留下来。
- 更新 `glTFRenderer/RendererScene/Private/RendererSceneGraph.cpp`，让它正确传递：
  - `baseColorTexture * baseColorFactor`
  - `metallicRoughnessTexture.bg * float2(roughnessFactor, metallicFactor)` 对应的存储 factor
- 更新 `glTFRenderer/RendererDemo/RendererModule/RendererModuleMaterial.cpp`，即使材质存在纹理句柄，也始终把 factor 上传到 GPU 材质结构里。
- 更新 `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleMaterial.hlsl`，让：
  - albedo 纹理采样结果乘 `info.albedo`
  - metallic-roughness 纹理采样结果乘 `info.metallicAndRoughness.bg`
- 更新 `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`，让 BasePass 现在真正写出：
  - `SV_TARGET0.xyz = base color`
  - `SV_TARGET0.w = metallic`
  - `SV_TARGET1.xyz = 编码后的 world normal`
  - `SV_TARGET1.w = roughness`

## Validation

- Build result:
  `RendererDemo` 验证构建通过，`0` 个 error，`43` 个 warning。
  日志：
  - `build_logs/rendererdemo_20260322_191051.msbuild.log`
  - `build_logs/rendererdemo_20260322_191051.stdout.log`
  - `build_logs/rendererdemo_20260322_191051.stderr.log`
  - `build_logs/rendererdemo_20260322_191051.binlog`
- Runtime validation:
  这次回合里没有额外跑新的运行时截图或 RenderDoc 验证；当前验证范围仅包括源码链路核对和验证构建通过。
- User acceptance:
  在确认诊断结果和修复内容后，用户要求把这组改动单独提交。

## Reflection And Prevention

- What signal should have been prioritized earlier:
  当下游 pass 明明按约定读取了某个 GBuffer 通道，但值还是不对时，应该优先回到 BasePass 写入点确认“到底写进去的是什么”，而不是先在 consumer 侧兜圈子。
- What guardrail or refactor would reduce recurrence:
  `RendererDemo` 的材质抽象不应该把 glTF PBR 输入收缩成 texture-or-factor 二选一；只要格式语义要求 texture-times-factor，就应该在 CPU 和 GPU 两侧都显式保留。
- What to check first if a similar symptom appears again:
  先检查对应 GBuffer 通道的写入语句，以及 CPU 到 GPU 的材质打包是否保留了正确的 texture、factor 和它们的乘积语义，再去看后续 pass 的读取端。
