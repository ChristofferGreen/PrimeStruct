# Chunk Culling Design Sketch

Status: proposal / experiment plan. Nothing here is implemented. Background
and sources: `docs/references/chunk_culling/` (start with its README).

## Goal

Reject chunks from **rendering** and, where soundly possible, from
**generation**, so hidden terrain is never generated, meshed, or drawn.

Invariant (non-negotiable): a chunk may be rejected only if it provably
contributes no visible pixel. False "visible" costs time; false "hidden" is a
bug. Rejection before generation must be proven from declared **bounds**, since
no voxels exist yet.

## What we have (and do not have)

- `examples/shared/scene_bgra8_renderer.h` is a **2D** UI scene renderer. There
  is no 3D rasterizer and no depth buffer to extend.
- `stdlib/std/math/{vector,matrix,quaternion}.prime` give us the projection
  math. Collections (`vector`, `map`, `soa`) can hold chunk tables.
- VM/native/Wasm backends all run the same canonical IR, and examples must be
  deterministic. So the culling core should be **pure, deterministic
  PrimeStruct code** (no GPU, no threads, no unordered iteration), runnable on
  the VM and golden-testable.

## What we pick, and what we skip

| Idea (source) | Decision | Reason |
|---|---|---|
| Conservative low-res depth buffer, tile `zMax`, box queries (MSOC, Hi-Z) | **Adopt**, scalar tiles first | Core occlusion test; SIMD and two-layer masking are later optimizations |
| Occluders derived from chunk data (Procedural World) | **Adopt**, extended with *bound-derived* occluders | Solves occluder authoring and works before voxels exist |
| Chunk state machine: empty / missing / in-production / resident (GigaVoxels DP) | **Adopt** | Gives dedup and free "empty" chunks |
| Visibility-driven generation: only produce what a ray/BFS reaches (GigaVoxels DP) | **Adopt** as the generation gate | Directly answers "reject from generation" |
| Coarse-then-fine bounds test, worst-case bounds per generator (Kuth) | **Adopt** | Cheap frustum/bounds stage before any voxel work |
| Face-pair connectivity BFS (Checchi) | **Later**, optional pre-pass | Strong for caves; under-culls on surface ravines |
| Self-chunk exception, AABB-depth conservativeness (Aokana) | **Adopt** | Needed for correctness |
| Two-pass Hi-Z with temporal reuse, GPU compute | **Skip for now** | Only matters on a compute backend |
| PVS preprocessing, SVDAG, learned occluders | **Skip** | Poor fit for procedural, streamed, editable worlds |

## Proposed approach: occlusion-ordered, bound-aware generation

Per frame (or camera move), traverse chunks **near to far** (BFS/ring order,
sorted by chunk coordinate for determinism):

1. **Frustum + distance** reject by chunk AABB (coarse, as in Kuth).
2. **Occlusion query**: project the chunk AABB, take the conservative minimum
   depth `zNear`, and compare against the depth buffer's farthest stored depth
   `zFar` over the covered tiles. Rejected only if `zNear > zFar` for every
   tile (MSOC rule).
3. If it passes, act on the chunk state:
   - `Empty`: skip, no cost.
   - `Resident`: draw it, and insert its **real occluders** (voxel-derived
     solid quads) into the depth buffer for later chunks.
   - `Missing`: mark `InProduction`, enqueue generation (deduped), insert its
     **bound-derived occluder** now (see below).
   - `InProduction`: wait (or draw coarse proxy, see open questions).
4. Chunks never passing step 2 are never generated.

### Bound-derived occluders (the new part)

The generator exposes a conservative **guaranteed-solid** query per column,
for example `solidBelow(x, z)`: a height such that every voxel below it is
solid regardless of caves or features (min height minus max cave/feature
depth). A chunk whose range lies under `solidBelow` is a solid-core chunk. Its
box can be inserted as an occluder **without generating a single voxel**. That
gives terrain-like worlds early occluders for the very first near-to-far pass.

Soundness contract: `solidBelow` may only under-estimate. A debug mode
generates everything and asserts that no chunk rejected by culling has a
visible pixel in the exact (full-resolution) depth buffer.

### Why this combination

- It needs only a tiny scalar depth buffer (target 64x64 tiles or 8x8-pixel
  tiles, as in Procedural World) and AABB queries: minimal code, fully
  deterministic, runs on the VM.
- Occlusion-ordered traversal means occluders always precede the queries they
  affect, so no temporal two-pass is needed.
- The state machine and near-to-far queue make generation demand-driven by
  visibility, the only published mechanism we found for skipping generation of
  hidden volume (GigaVoxels DP).

## Experiment plan (what to try first)

**Phase 0: measure before building.** A small, throwaway deterministic
simulator (language TBD, see open questions) that answers: *how many chunks can
each method reject, with zero wrong rejections?* Setup:

- Synthetic heightfield + cave world (seeded, deterministic), chunks 16^3 or
  32^3, a handful of camera poses (ground level looking along terrain, hilltop,
  inside a cave, high altitude).
- Ground truth: generate everything and rasterize an exact depth buffer; the
  visible set is every chunk with at least one visible pixel.
- Methods compared, cumulatively:
  A. frustum + distance;
  B. A + occlusion with *voxel-derived* occluders (Procedural World style);
  C. B + *bound-derived* occluders (our proposal);
  D. C + cave-connectivity BFS pre-pass.
- Metrics: chunks passed to generation, chunks drawn, wrong rejections (must be
  0), and depth-buffer resolution sensitivity (16x16, 32x32, 64x64, 128x128).

Success: C rejects a large share of A's chunk set on terrain camera poses with
zero wrong rejections at 64x64 or lower. If C does not beat B meaningfully,
bound-derived occluders are not worth their complexity, and we fall back to B.

**Phase 1: PrimeStruct prototype** (only if Phase 0 supports it): stdlib-style
module for the depth buffer and chunk queries, plus an example and golden
tests.

**Phase 2:** integrate generation gating and the state machine; consider
cave connectivity if Phase 0 shows D adds value.

## Testing plan

- Unit: AABB projection depth bounds; tile coverage; query accepts/rejects on
  hand-made occluder layouts (including camera inside a chunk, the Aokana
  self-chunk case).
- Property: for random seeded worlds and cameras, `rejected set` intersect
  `exactly-visible set` is empty.
- Golden: depth-buffer dumps and chunk-state transitions for fixed camera
  paths, stable across VM/native/Wasm.
- Negative: a deliberately unsound `solidBelow` must make the property test
  fail (proves the test has teeth).

## Open questions

1. **Simulator language.** Python script under `scripts/` is fastest to
   iterate; PrimeStruct directly gives dogfooding and a path to a real module,
   but slows exploration. Recommendation: Python for Phase 0, port after.
2. **World model for the experiment.** Heightfield + caves (matches most voxel
   games), or something from this repo's own graphics direction?
3. **Coordinates and precision.** f32 projection near the clip plane needs
   conservative handling (boxes crossing the near plane: treat as visible).
4. **In-production chunks.** Block, draw a coarse proxy, or skip for a frame?
   Affects pop-in vs hitching.
5. **Editing.** Voxel edits invalidate occluders and connectivity bits;
   incremental update policy is undefined.
6. **Docs/TODO bookkeeping.** `AGENTS.md` requires a `docs/todo.md` leaf when
   implementation remains open. This sketch is docs-only; add leaves once the
   Phase 0 scope is agreed.
