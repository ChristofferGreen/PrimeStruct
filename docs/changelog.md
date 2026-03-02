# PrimeStruct Changelog

## 2026-03-02

### Breaking Changes
- Removed legacy `include<...>` source-import compatibility aliases.
  `import<...>` is now required for source imports.
- Removed legacy `--include-path` CLI aliases from `primec` and `primevm`.
  Use `--import-path` (or `-I`) to provide import roots.
