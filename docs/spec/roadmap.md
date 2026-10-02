# Dependencies, Risks, and Next Steps

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **roadmap / history**.

## Dependencies & Related Work
- Stable IR definition & serialization (std::serialization once available).
- Scene graph/rendering plans (`docs/finished/Plan_SceneGraph_Renderer_Finished.md`).

## Risks & Research Tasks
- **Memory model** – value/reference semantics, handle lifetimes, POD vs GPU staging rules.
- **Resource bindings** – unify descriptors across CPU/GPU backends.
- **Debugging** – source maps, stack inspection across VM/GPU.
- **Performance** – ensure parity with handwritten code; benchmark harnesses.
- **Adoption** – migration strategy from existing shaders/scripts.
- **Diagnostics** – map compile/runtime errors back to PrimeStruct source across backends.
- **Concurrency** – finalise coroutine/await integrations.
- **Security** – sandbox policy for transforms/archives.
- **Tooling** – IDE/LSP roadmap, incremental compilation caches, metadata outputs.

## Validation Strategy
- Golden tests comparing outputs from C++, VM, GLSL for shared modules.
- Performance benchmarks recorded under `benchmarks/`.
- Static analysis/lint integrated into CI to catch undefined constructs before codegen.

## Next Steps (Exploratory)
1. Draft detailed syntax/semantics spec and circulate for review. _(Draft captured in a separate syntax spec on
   2025-11-21; review/feedback tracking TBD.)_
2. Prototype parser + IR builder (Phase 0).
3. Evaluate reuse of existing shader toolchains (glslang, SPIRV-Cross) vs bespoke emitters.
4. Design import/package system (module syntax, search paths, visibility rules, transform distribution).
5. Define library/versioning strategy so import resolution enforces compatibility.
6. Flesh out stack/class specifications (calling convention, class sugar transforms, dispatch strategy) across backends.
7. Lock down composite literal syntax across backends and add conformance tests.
8. Decide machine-code strategy (C++ emission, third-party JIT) and prototype.
9. Define diagnostics/tooling plan (source maps, error reporting pipeline, incremental tooling, editor roadmap).
10. Document staffing/time requirements before promoting PrimeStruct onto the active roadmap.
