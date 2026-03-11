# glTFRenderer

Current documentation and automation use the current directory as the canonical repo root.

Project-owned documentation is indexed in `docs/README.md`.

## Directory Layout

- `AGENTS.md`: root workflow rules for build logging, doc handling, and runtime triage.
- `docs/README.md`: canonical documentation directory for all project-owned docs.
- `docs/`: unified project documentation tree, including architecture notes, feature-note archives, and debug notes.
- `scripts/`: root utility scripts for quiet verify builds, regression compare, and doc validation.
- `glTFRenderer/`: Visual Studio solution subtree and maintained renderer source tree.
- `glTFRenderer/scripts/`: `RendererDemo` perf/regression loop scripts.

## Path Convention

- Repo-root-relative code paths should start with `glTFRenderer/`, for example `glTFRenderer/RendererCore/...`.
- Root utility scripts stay under `scripts/`.
- Runtime-only asset or shader paths such as `Resources/...` and `glTFResources/...` are reserved for executable working-directory examples.

## Project Scope

- Learn and iterate on glTF loading/rendering workflows.
- Current maintained runtime path centers on `RendererDemo` + `RendererCore`.

