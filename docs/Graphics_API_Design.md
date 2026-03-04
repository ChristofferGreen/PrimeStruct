# PrimeStruct Graphics API Design

Status: Draft (v0)

This document defines the proposed graphics API architecture for PrimeStruct
across browser and native backends.

## Goals
- Provide one stable graphics API surface for PrimeStruct programs.
- Support browser, native desktop, and macOS Metal targets with aligned
  semantics.
- Keep behavior deterministic where practical (resource creation order,
  pipeline state hashing, diagnostics ordering).
- Enable the spinning-cube sample as a shared cross-target milestone.

## Non-goals (v1)
- Exposing backend-specific extension namespaces in the language surface.
- Full parity with every backend-specific advanced feature.
- Solving all rendering domains (ray tracing, mesh shaders, bindless, etc.).

## Decision Summary
1. PrimeStruct uses a single portable graphics API surface (`PrimeGfx Core`).
2. Backend support is controlled via explicit profiles:
   - `metal-osx`
   - `vulkan-native`
   - `webgpu-browser`
3. Feature differences are enforced through capability/limit gating, not
   backend-specific language APIs.
4. Backend-specific escape hatches are deferred until concrete gaps are proven
   by at least two production use cases.

## Architecture
### Layer model
1. Language layer:
   - Core graphics types and calls under `/std/gfx/*`.
   - No `/std/gfx/ext/*` namespace in v1.
2. Compiler layer:
   - Profile-aware validation of effects, types, and resource constraints.
   - Shader translation path selected by profile.
3. Runtime layer:
   - Backend adapter per profile.
   - Uniform command model from Core calls to backend command submission.

### Compile-time profile gating
- A profile is selected per build target.
- Unsupported features fail at compile time with deterministic diagnostics.
- Profile checks happen before backend emission.

## PrimeGfx Core (proposed)
### Resource model
- `Buffer<T>`
- `Texture2D<T>`
- `Sampler`
- `ShaderModule`
- `Pipeline`
- `BindGroupLayout`
- `BindGroup`

### Command model
- `CommandEncoder`
- `RenderPass`
- `Queue`
- `Swapchain`/presentation surface abstraction

### Core operations
- Create/destroy resources.
- Upload/download buffer data (download support may be profile-gated).
- Create shader modules and pipeline objects.
- Bind resources and issue draw commands.
- Submit command buffers and present frames.

## Shader Strategy
### Direction
- Keep one logical shader contract in PrimeStruct.
- Emit profile-specific shader artifacts:
  - `webgpu-browser`: WGSL-oriented output.
  - `metal-osx`: MSL (`.metal`/`metallib`) output path.
  - `vulkan-native`: SPIR-V path.

### Immediate constraints
- Keep v1 to a minimal graphics-safe subset used by the spinning-cube example.
- Reject unsupported shader constructs at compile time with profile-aware
  diagnostics.

## Capability and Limits Model
Each profile advertises:
- Supported shader stages.
- Buffer/texture formats.
- Resource binding limits.
- Push-constant/uniform support policy.
- Feature flags (for example: depth formats, instancing, index type support).

Compiler behavior:
- Validate Core API usage against selected profile limits.
- Produce stable diagnostics with explicit profile context.

Runtime behavior:
- Validate hardware/runtime-reported limits against compile assumptions.
- Fail early with deterministic error codes if runtime limits are insufficient.

## Determinism Rules
- Stable ordering for resource binding layout generation.
- Stable pipeline key/hash computation.
- Stable diagnostic ordering for profile validation failures.
- Deterministic simulation loop for example workloads (fixed-step updates).

## Testing Strategy
### Unit tests
- Core API validation logic.
- Profile capability matrix checks.
- Shader artifact metadata checks.

### Compile-run tests
- Positive/negative profile-gated compile cases.
- VM/native/Wasm simulation parity for shared cube transform math.

### Integration tests
- Build artifact checks for all targets (`.wasm`, native binary, shader assets).
- macOS Metal shader compile smoke (`xcrun metal`, `metallib`).
- Optional browser startup smoke checks where CI environment allows.

### Golden/snapshot tests
- Diagnostic output snapshots for unsupported feature paths.
- Pipeline/resource layout serialization snapshots.

## Spinning Cube Milestone Contract
The spinning-cube sample is the first conformance target for this design.

Required:
- Shared simulation logic.
- Browser host path (`webgpu-browser`).
- Native host path.
- macOS Metal host path.
- Cross-target deterministic transform progression checks.

## Evolution Policy
- Do not add backend-specific `/std/gfx/ext/*` APIs in v1.
- Re-evaluate extension namespaces only if:
  1. A concrete missing feature cannot be represented in Core.
  2. The feature is justified by at least two real workloads.
  3. The team approves a compatibility and testing plan.

## Open Questions
- Final shader intermediate strategy between front-end and profile emitters.
- Resource lifetime/ownership details for long-lived pipelines.
- Exact balance of compile-time vs runtime validation for hardware limits.
- Synchronization model surface (barriers/events) for post-v1.

## Next Steps
1. Lock the minimal `PrimeGfx Core` surface for spinning-cube scope.
2. Define profile capability tables (`metal-osx`, `vulkan-native`,
   `webgpu-browser`) in code and tests.
3. Implement profile-gated compiler diagnostics.
4. Wire first end-to-end sample pipelines with artifact checks in CI.
