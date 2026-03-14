# Bug 标题

配套文档：

- 英文版本：
  - `docs/debug-notes/TEMPLATE.md`

- 状态：Accepted
- 日期：YYYY-MM-DD
- 范围：组件 / backend / 测试用例
- 提交：`<hash>`
- 配套文档：
  - 同目录配套文件，例如 `2026-03-11-example-bug_ZH.md` 或 `2026-03-11-example-bug_EN.md`
- 路径约定：repo 相对代码路径应从 `glTFRenderer/` 开始；当可执行程序依赖工作目录解析时，可以使用运行时专用的 `Resources/...` 或 `glTFResources/...` 路径。

## Symptom

描述用户可见的症状。

## Reproduction

列出复现该问题所需的测试用例、backend、相机状态或 snapshot。

## Wrong Hypotheses Or Detours

### Detour 1

- Why it looked plausible:
- Why it was not the final cause:

### Detour 2

- Why it looked plausible:
- Why it was not the final cause:

## Final Root Cause

描述精确的失效生命周期、同步、数学或状态流转问题。

## Final Fix

- 列出实际代码修改。
- 如果有 helper tooling 或 repro 改进，应与最终 fix 分开说明。

## Validation

- Build result:
- Runtime validation:
- User acceptance:

## Reflection And Prevention

- What signal should have been prioritized earlier:
- What guardrail or refactor would reduce recurrence:
- What to check first if a similar symptom appears again:
