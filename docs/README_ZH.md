# 项目文档目录说明

这份文件是 `docs/` 目录的中文索引，对应英文版：

- `docs/README.md`

## 1. 根目录约定

- 当前仓库根目录以 repo 根为准。
- `docs/` 与 `scripts/` 位于仓库根下。
- 源码与解决方案位于 `glTFRenderer/` 子树。
- 文档中的 repo 相对路径应优先从仓库根开始，例如：
  - `glTFRenderer/...`
  - `docs/...`
  - `scripts/...`

## 2. 架构与设计文档

- `docs/RendererFramework_Architecture_Summary.md`
  - 中文版渲染框架架构总结。
- `docs/RendererFramework_Architecture_Summary_EN.md`
  - 英文版渲染框架架构总结。
- `docs/RendererFramework_KeyClass_Diagram.md`
  - 渲染架构关键类依赖说明图，采用双语图源。
- `docs/RendererDemo_AppLayer_Toolkit_Design.md`
  - 当前 `RendererDemo` 应用层封装工具设计说明。
- `docs/RendererDemo_AppLayer_Toolkit_Design_EN.md`
  - `RendererDemo` 应用层封装工具设计英文版。
- `docs/RendererDemo_AppLayer_Toolkit_Usage.md`
  - 当前应用层封装工具的使用说明。
- `docs/RendererDemo_AppLayer_Toolkit_Usage_EN.md`
  - 当前应用层封装工具使用说明英文版。
- `docs/RDG_Algorithm_Notes.md`
  - RenderGraph 算法与执行计划说明。
- `docs/RDG_Algorithm_Notes_ZH.md`
  - RenderGraph 算法与执行计划说明中文版。
- `docs/SurfaceResizeCoordinator_Design.md`
  - surface resize 协调器设计说明。
- `docs/SurfaceResizeCoordinator_Design_ZH.md`
  - surface resize 协调器设计说明中文版。
- `docs/DocReferencePolicy.md`
  - 文档引用规范英文版。
- `docs/DocReferencePolicy_ZH.md`
  - 文档引用规范中文版。

## 3. 合同与计划文档

- `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - Frosted Glass 主开发与验收计划。
- `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan_ZH.md`
  - Frosted Glass 主开发与验收计划中文版。
- `docs/RendererDemo_FrostedGlass_FramePassReference.md`
  - 当前 Frosted Glass 帧内 pass / resource 合同参考。
- `docs/RendererDemo_FrostedGlass_FramePassReference_ZH.md`
  - 当前 Frosted Glass 帧内 pass / resource 合同参考中文版。

## 4. Feature Note 归档

- `docs/FeatureNotes/Feature_Note_Template.md`
  - Feature Note 模板。
- `docs/FeatureNotes/Feature_Note_Template_ZH.md`
  - Feature Note 模板中文版。
- `docs/FeatureNotes/20260315_B9_RenderDocFrameCapturePlan.md`
  - RenderDoc 帧截取接入计划英文版。
- `docs/FeatureNotes/20260315_B9_RenderDocFrameCapturePlan_ZH.md`
  - RenderDoc 帧截取接入计划中文版。
- `docs/FeatureNotes/*.md`
  - 已归档的 feature 设计、实现和验证记录。

## 5. 调试记录

- `docs/debug-notes/README.md`
  - 调试记录约定与索引。
- `docs/debug-notes/README_ZH.md`
  - 调试记录约定与索引中文版。
- `docs/debug-notes/TEMPLATE.md`
  - 调试记录模板。
- `docs/debug-notes/TEMPLATE_ZH.md`
  - 调试记录模板中文版。
- `docs/debug-notes/*.md`
  - 已确认问题的根因、修复和验证记录。

## 6. 放置规则

- 长生命周期的架构、设计、合同、计划文档放在 `docs/`。
- 运行时 bug 的最终记录放在 `docs/debug-notes/`。
- 脚本专属说明放在对应脚本旁边。

## 7. 双语与说明图规则

- 对 `docs/` 下新建或实质更新的项目文档，默认要求同时维护中文和英文版本。
- 现有无后缀文档可继续保留，配套语言版本放在同目录下，使用 `_EN.md` 或 `_ZH.md`。
- 对渲染架构或应用层框架相关文档，优先维护共享说明图，并在相关文档中引用：
  - `docs/RendererFramework_KeyClass_Diagram.md`

## 8. 校验命令

提交前建议运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-DocReferences.ps1
```

## 9. 当前推广状态

- 顶层架构、设计、合同、计划文档已经补齐双语覆盖，或统一改为引用共享双语说明图。
- Feature Note 和 debug-note 的模板 / 索引文档已经补齐双语配套。
- `docs/FeatureNotes/` 与 `docs/debug-notes/` 下的历史归档记录，当前仍可能是单语版本；后续在被实质更新时补齐对应配套文档。
