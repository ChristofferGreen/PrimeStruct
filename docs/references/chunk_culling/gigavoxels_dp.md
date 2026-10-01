# GigaVoxels DP: Ray-Guided On-Demand Production

Richermoz, Neyret. HPG 2024 / PACMCGIT 7(3).
- Open copy (HAL, CC BY-NC-SA 4.0): https://hal.science/hal-04654692v1
- Vendored: `pdfs/gigavoxels_dp.pdf` (about 20 MB)
- Original idea: Crassin et al. 2009, GigaVoxels (not read).

Status: full text read (sections 1-4; conclusion skimmed).

## Why it matters

This is the closest published mechanism to **rejecting chunks from
generation**: voxel bricks are produced *only when a ray reaches them*, so
anything occluded is never generated. Visibility drives production directly,
with no separate culling pass and no precomputed visibility.

## Mechanism

- Data: 8^3-voxel bricks in a brick pool (3D atlas, LRU cache); an
  **indirection table** (a clipmap, about 300^3 entries per LOD, 8 LODs) maps
  (x, y, z, LOD) to {pool slot, empty, missing, in-production}.
- LOD per ray via differential ray cones (like mip selection), so every visible
  brick has near-constant screen size and the working set per frame is
  near-constant. This "mostly opaque scene" assumption is what bounds memory.
- Render task (one thread per pixel, 32x16 tiles) marches the ray through the
  indirection table:
  - `empty`: skip the brick (hierarchical empty-space skipping);
  - `missing`: `atomicCAS(missing -> inProduction)`; the winner launches a
    production task, the ray saves its state (distance, accumulated color,
    lock) in a payload buffer and stops;
  - `inProduction`: save state and stop;
  - resident: sample, accumulate, advance. Stops at alpha = 1, so everything
    behind an opaque hit is never requested.
- Production task: one thread per voxel evaluates the (procedural) function;
  if all voxels are empty the brick is marked `empty` (no pool slot), else a
  slot is taken from a free-slot list. Then it relaunches ray threads over the
  brick's screen-space bounding box; each re-acquires its ray lock via
  `atomicCAS` and resumes.
- Eviction: per-brick timestamp buffer, radix-sorted each frame; least recently
  used slots refill the free-slot list (list sized at 10% of the pool).
- Dynamic parallelism lets render and production run concurrently instead of
  alternating CPU-synchronized passes, giving 2.1x average over the original
  GigaVoxels scheduling (1.1x to 4.4x across scenes and motions).

## Notable results and caveats

- Test scenes use procedural producers with **no oracle or bounding box to
  pre-register empty space**; the cache plus hierarchical production alone
  concentrate work where it is needed.
- Worst cases are disocclusion-heavy: aligned spheres (free sight to the
  horizon, many simultaneous requests) and moving backward or sideways.
- 1% low FPS is about 60% of average, so frame-time spikes from production
  remain.
- A known rare data race (about 1 per 1000 frames) can leave an empty pixel;
  fix is an extra full-screen pass.
- Needs CUDA dynamic parallelism (or persistent threads / device-side enqueue).
  Voxels are continuous density with interpolation, not binary Minecraft-style
  boxels; the authors explicitly contrast with boxel engines that mesh.

## Mapping to PrimeStruct

- Adopt the idea, not the GPU machinery: a **demand-driven generation queue**
  fed by visibility. Software equivalent: a CPU or compute pass finds the
  chunk each pixel/ray first needs; missing chunks enter a request set,
  generated chunks write occluders/depth, and the pass resumes.
- Four-state indirection (empty / missing / in-production / resident) is a
  clean chunk state machine: `Empty` chunks cost nothing and need no storage;
  `InProduction` dedups requests (the `atomicCAS` trick).
- "Empty brick -> mark and skip" gives a cheap early-out for all-air chunks.
- Pair with conservative bounds (terrain_horizon_and_generation.md): rays can
  skip chunks whose bounds already prove them empty, without requesting them.
- Frame-time risk: on-demand production hitches on disocclusion. Mitigate with
  a margin around the frustum and a per-frame generation budget.
