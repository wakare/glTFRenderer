# RendererDemo 手动帧截取使用说明

配套文档：

- 英文版本：`docs/RendererDemo_FrameCapture_Usage_EN.md`
- 相关背景：`docs/FeatureNotes/20260315_B9_RenderDocFrameCapturePlan.md`

## 1. 目标与范围

这份文档说明当前 `RendererDemo` 维护路径下，共享手动帧截取入口的使用方式。

当前覆盖的能力是：

- `DemoBase` 下共享的 RenderDoc 单帧截取
- `DemoBase` 下共享的 PIX 单帧截取
- `DemoAppModelViewerFrostedGlass` 的自动 RenderDoc / PIX regression 入口
- 启动参数、UI 操作、默认输出目录、运行时限制与 regression 入口

当前不在范围：

- `glTFApp` 旧运行时路径
- regression compare / report 管线的实现细节
- 外部工具本身的安装教程

## 2. 能力矩阵

| 工具 | Backend | 默认产物 | 入口 | 备注 |
| --- | --- | --- | --- | --- |
| RenderDoc | DX12 / Vulkan | `.rdc` | `Runtime / Diagnostics > RenderDoc` | 可重新打开最近一次 capture。 |
| PIX | DX12 | `.wpix` | `Runtime / Diagnostics > PIX` | 当前只支持 DX12。 |

共同约束：

- 两条路径都是 request-driven 的单帧 capture，不是持续录制。
- capture runtime 必须在目标设备初始化前完成 preload。
- RenderDoc 和 PIX 不能同时 arm 同一帧 capture。

## 3. 启动约定

### 3.1 Demo 名称参数

`RendererDemo.exe` 期望第一个位置参数就是 demo 命令名。

当前内置 demo 为：

- `DemoTriangleApp`
- `DemoAppModelViewer`
- `DemoAppModelViewerFrostedGlass`

如果把 `-dx12` 或 `-vulkan` 放在第一个位置参数，程序会把它当成 demo 名称并报 `Unknown demo`。

### 3.2 工作目录

推荐从输出目录启动，或在 IDE 里把工作目录设为 `$(OutDir)`。

从仓库根启动的推荐方式：

```powershell
Set-Location .\glTFRenderer\x64\Debug
```

### 3.3 Backend 参数

常用 backend 参数：

- DX12：`-dx` 或 `-dx12`
- Vulkan：`-vk` 或 `-vulkan`

### 3.4 Capture 启动参数

共享 `DemoBase` 手动 capture UI 当前支持：

- `-renderdoc-ui`
  - 在创建设备前预加载 RenderDoc，供共享手动 UI 使用。
- `-renderdoc-capture`
  - 对共享手动 UI 来说，本质上也是“本次运行启用 RenderDoc preload”。
  - 这个参数本身不会在进程启动后自动生成 `.rdc`。
- `-renderdoc-required`
  - 当 RenderDoc API 不可用时，让初始化直接失败。
- `-pix-ui`
  - 在创建设备前预加载 PIX GPU capturer，供共享手动 UI 使用。
- `-pix-capture`
  - 对共享手动 UI 来说，本质上也是“本次运行启用 PIX preload”。
  - 这个参数本身不会在进程启动后自动生成 `.wpix`。
- `-pix-required`
  - 当 PIX capture API 不可用时，让初始化直接失败。

说明：

- 在 demo 专属 regression 路径里，`-renderdoc-capture` 与 `-pix-capture` 可能还有额外语义；但在共享 `DemoBase` 手动 UI 下，它们仍然只表示 preload / enable。
- PIX 参数当前只对 DX12 有意义。

## 4. 推荐命令模板

### 4.1 DX12 + PIX

```powershell
Set-Location .\glTFRenderer\x64\Debug
.\RendererDemo.exe DemoTriangleApp -dx12 -pix-ui
```

如果希望工具不可用时直接失败：

```powershell
Set-Location .\glTFRenderer\x64\Debug
.\RendererDemo.exe DemoTriangleApp -dx12 -pix-required
```

### 4.2 DX12 / Vulkan + RenderDoc

```powershell
Set-Location .\glTFRenderer\x64\Debug
.\RendererDemo.exe DemoTriangleApp -dx12 -renderdoc-ui
```

```powershell
Set-Location .\glTFRenderer\x64\Debug
.\RendererDemo.exe DemoTriangleApp -vulkan -renderdoc-ui
```

### 4.3 DX12 + PIX regression 自动化（仅 `DemoAppModelViewerFrostedGlass`）

从仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json `
  -Backend dx `
  -DemoApp DemoAppModelViewerFrostedGlass `
  -PIXCapture
```

如果希望把 PIX 不可用视为本次运行失败，可再加上 `-PIXRequired`。

相关 suite JSON 字段：

- 全局默认值：
  - `default_capture_pix`
  - `default_pix_capture_frame_offset`
  - `default_keep_pix_on_success`
- 单 case 的 `capture` 块：
  - `capture_pix`
  - `pix_capture_frame_offset`
  - `keep_pix_on_success`
- 兼容回退：
  - parser 也接受把这三个字段直接写在 case 根节点

这条 regression 路径当前有这些限制：

- 只支持 DX12
- 目前只有 `DemoAppModelViewerFrostedGlass` 会生成 PIX regression 工件
- RenderDoc 与 PIX 自动化在同一次 regression run 中互斥
- 在共享 `DemoBase` 手动 UI 路径里，`-pix-capture` 仍然只表示 preload / enable，不代表启动即自动截帧

## 5. 手动操作流程

1. 以目标 backend 和 capture 启动参数启动 `RendererDemo.exe`。
2. 打开 `Runtime / Diagnostics`。
3. 展开目标工具分组：
   - `RenderDoc`
   - `PIX`
4. 视需要修改：
   - `Capture Name`
   - `Capture Dir`
   - 自动打开 UI 选项
5. 点击 `Capture Current Frame`。
6. 观察：
   - `Pending Capture Frame`
   - `Last Capture`
   - `Status`
7. 如需重新打开最近一次成功 capture，使用：
   - `Open Last Capture In Replay UI`
   - `Open Last Capture In PIX`

## 6. 默认输出与状态判读

默认输出目录是相对当前工作目录解析的。

如果工作目录是 `glTFRenderer\x64\Debug`，默认输出会落在：

- RenderDoc：`build_logs/renderdoc/*.rdc`
- PIX：`build_logs/pix/*.wpix`

共享 UI 中常见状态字段的含义：

- `Capture Enabled`
  - 当前运行是否已经启用该 capture 路径。
- `Capture Available`
  - 当前进程内是否真的拿到了对应工具的 capture API。
- `Startup Status`
  - 说明 preload 阶段是否成功，以及失败原因。
- `Pending Capture Frame`
  - 已经 arm，但尚未 finalize 的目标帧号。
- `Last Capture`
  - 最近一次成功 capture 的产物路径。
- `Status`
  - 最近一次请求、完成或打开 UI 的结果说明。

## 7. 代码落点与维护边界

共享手动 frame-capture 入口当前主要落在：

- `glTFRenderer/RendererCore/Public/RendererInterface.h`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`

工程与依赖侧当前涉及：

- `glTFRenderer/RendererCore/RendererCore.vcxproj`
- `glTFRenderer/RendererCore/packages.config`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj`
- `glTFRenderer/RendererDemo/packages.config`
- `glTFRenderer/glTFRenderer/glTFRenderer.vcxproj`
- `glTFRenderer/glTFRenderer/packages.config`

维护时应保留的约束：

- preload 必须发生在目标 backend 设备初始化之前。
- capture start / end 必须和共享帧生命周期对齐，而不是挂在临时 UI 逻辑上。
- runtime recreate 或 shutdown 时，先结束 capture，再清理 GPU-backed helper。
- 不要让 RenderDoc 和 PIX 进入互相覆盖的 armed / capturing 状态。

## 8. 当前限制

- PIX 只支持 DX12；当前没有 Vulkan PIX capture 路径。
- 共享 `DemoBase` UI 只支持单帧手动 capture。
- PIX regression 自动化目前只在 `DemoAppModelViewerFrostedGlass` 上实现，还不是共享 `DemoBase` 能力。
- RenderDoc 与 PIX 自动化在同一次 regression run 中互斥。
- `-pix-capture` 与 `-renderdoc-capture` 在共享手动 UI 下只负责 preload / enable，不代表“启动即自动截帧”。

## 9. 当前验证状态

截至 `2026-03-23`，这条路径已具备以下确认结果：

- `RendererDemo` 编译链路通过，PIX 相关工程依赖已接通。
- 共享 `DemoBase` 手动 PIX UI 已接入 DX12 运行时路径。
- 当前工作站上的手动 PIX capture 已由用户确认可以生效。
- `Capture-RendererRegression.ps1` 与 `Validate-RendererRegression.ps1` 已支持 DX12 `DemoAppModelViewerFrostedGlass` regression 的 PIX 自动化参数。

这意味着共享手动 capture 入口已经能作为 `RendererDemo` 的日常 GPU 调试入口使用；同时 frosted-glass regression 路径现在也补上了 DX12 下的 PIX 自动化入口。
