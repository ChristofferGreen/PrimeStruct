# PrimeStruct Changelog

## 2026-03-06

### Docs
- Expanded `docs/Graphics_API_Design.md` with a locked v1 spinning-cube mini-spec:
  concrete `/std/gfx/*` resource/frame API signatures, compile-target profile
  deduction, locked `/std/gfx/VertexColored` wire layout, deterministic
  `GfxError` code set, and `?` + `on_error<...>` fallible-flow rule.
- Added planned (non-locking) layered UI/software-renderer architecture notes in
  `docs/Graphics_API_Design.md` (command-list renderer, two-pass layout,
  basic/composite widgets, HTML adapter, and platform input adapter).
- Updated cross-doc references in README/design/spec/coding-guidelines/TODO to
  align with the new graphics mini-spec and roadmap direction.

## 2026-03-02

### Breaking Changes
- Removed legacy `include<...>` source-import compatibility aliases.
  `import<...>` is now required for source imports.
- Removed legacy `--include-path` CLI aliases from `primec` and `primevm`.
  Use `--import-path` (or `-I`) to provide import roots.
