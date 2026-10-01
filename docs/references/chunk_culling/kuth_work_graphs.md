# Real-Time Procedural Generation with GPU Work Graphs

Kuth, Oberberger, Faber, Baumeister, Chajdas, Meyer. HPG 2024 / PACMCGIT 7(3),
Best Paper. Preprint: https://CoburgGraphicsLab.github.io/files/Kuth24RPG.pdf
(about 32 MB, not vendored; fetch from the link). Official:
https://doi.org/10.1145/3675376

Status: full text searched for all culling/visibility content; sections on
ivy, garlands, and ray-traced markers skimmed, not read in depth.

## What it is

Procedural generation expressed as a GPU *work graph*: shader nodes spawn
child nodes dynamically, so recursive generators (ivy, garlands, market
stalls, ground clutter) run entirely on the GPU, combined with ray tracing
(invisible "marker" geometry in the BVH for placement queries) and procedural
mesh shaders.

## Generation-time culling in the paper (all frustum-based)

The paper's rejection of work *before generation* is simple and conservative:

1. **Worst-case bounding volume per generator.** Ivy: the user caps the number
   of segments a branch may spawn; that fixes a worst-case radius; the branch
   bounding sphere is centered at the initial position, and each `IvyBranch`
   terminates early when that sphere is outside the camera frustum.
2. **Grid-level culling for ground clutter.** The 8 frustum corners are
   projected onto the terrain grid plane; the intersection of their quantized
   world-axis-aligned bounding box with the clutter-receiving bounds gives
   exactly which `ClutterTile` nodes to launch (coarse stage). Each thread then
   does fine-grained frustum culling using the height map (fine stage).
3. Result on the "Market" view: 55,642 generated objects omitted, 3,209 more
   culled before rendering; the paper states generation time drops when
   non-visible generation is omitted. In the overview view nothing can be
   culled. No occlusion culling is used anywhere.

So: **frustum-only, coarse-to-fine, driven by provable worst-case bounds.** It
confirms that generation-time rejection from conservative bounds is practical
and cheap, but offers nothing for occlusion before generation.

## Why work graphs matter here

Indirect-dispatch culling needs worst-case buffer allocation and a barrier
between the "create" and "execute" stages. Work graphs remove both, which lets
a culling node amplify directly into generation nodes. Limits noted: graph
depth 32, only 8 BVH instance mask bits (so 8 generation phases), non-deterministic
draw-node scheduling (element order changes each frame), and generated data is
GPU-resident (CPU collision cannot see it without readback).

## Mapping to PrimeStruct

- Adopt the **coarse-then-fine** structure for chunk generation: a cheap
  conservative grid-level test chooses candidate chunks, then a finer per-chunk
  test runs before voxel generation. Same shape on a CPU job queue.
- Adopt the **worst-case-bounds rule**: every generator declares a bound on
  what it can write (height range, feature radius). Frustum rejection before
  generation is valid only if that bound is correct.
- Not applicable: GPU work graph API. For determinism, the repo's rule
  ("no unordered iteration that affects outputs") argues against the paper's
  non-deterministic scheduling; PrimeStruct generation queues should sort
  candidates by chunk coordinate.
- Gap remains: no source here combines occlusion with pre-generation rejection
  (see gigavoxels_dp.md for the ray-guided answer).
