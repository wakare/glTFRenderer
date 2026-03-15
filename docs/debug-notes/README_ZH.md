# 调试记录目录说明

配套文档：

- 英文版本：
  - `docs/debug-notes/README.md`

这个目录用于存放已经被接受的 bug 修复记录，适合那些排查成本较高、容易复现困难或未来容易再次踩到的问题。

## 仓库根约定

- 记录中的 repo 相对路径以当前仓库根目录为基准。
- 源码路径应优先从 `glTFRenderer/` 开始。
- 像 `Resources/...` 或 `glTFResources/...` 这类依赖运行目录的路径，可以在描述可执行程序启动行为时保留无前缀写法。

## 什么时候添加记录

- 在用户明确接受修复后再归档记录。
- 对 backend-specific、timing-sensitive、cross-frame 或其他容易误判的问题，应补调试记录。
- 如果问题仍在调查中，不要先放进这个目录。
- 对新增加的记录，如果它会长期作为项目参考，应在同目录补一个 `_ZH.md` 或 `_EN.md` 配套版本。

## 文件命名

- `YYYY-MM-DD-short-bug-name.md`

## 必需章节

- Symptom
- Reproduction
- Wrong hypotheses or detours
- Final root cause
- Final fix
- Validation
- Reflection and prevention

## 当前记录

- [2026-03-11-dx12-shadow-lifetime-review.md](./2026-03-11-dx12-shadow-lifetime-review.md)
- [2026-03-11-vulkan-shadow-ghosting.md](./2026-03-11-vulkan-shadow-ghosting.md)
- [2026-03-15-renderdoc-ui-disabled-stack.md](./2026-03-15-renderdoc-ui-disabled-stack.md)
