# Advanced Cave Culling (Minecraft PE)

Tommaso Checchi, 2014.
- Part 1: https://tomcc.github.io/2014/08/31/visibility-1.html
- Part 2: https://tomcc.github.io/2014/08/31/visibility-2.html
- Vintage Story variant (ray casting): https://github.com/tyronx/occlusionculling

Status: web summaries of both parts; some filter details were not recoverable.

## Per-chunk connectivity graph

15 bits per chunk, one per unordered pair of the 6 faces: "can you get from
face A to face B through non-opaque space inside this chunk?". Built by
flood-filling each non-opaque region and connecting every chunk face the fill
touches. Recompute on block change: about 0.1-0.2 ms, done on a background
thread.

## Traversal

BFS from the camera's chunk. Queue entries are (chunk, face we entered
through). For a neighbor C across face F of the current chunk B:
1. skip if stepping backward: face normal . view vector < 0 test;
2. skip if B's graph says the entry face does not connect to F;
3. other filters;
4. frustum test last (most expensive).

BFS visits chunks front-to-back and solves culling, depth sorting, and rebuild
scheduling in linear time. Reported culling: 50% to 99% of geometry.

## Weakness

Under-culls: paths may bend (10 chunks straight, then 7 down) into caves and
ravines the camera cannot actually see. Heuristic penalties (+1 step going
below sea level, +3 for fully dark chunks) recover only 5-15% of cull ratio and
do nothing for sunlit surface ravines.

## Mapping to PrimeStruct

- Cheap, needs no rasterizer: good baseline and a pruning pass before depth
  tests (only depth-test chunks the BFS reaches).
- **Generation angle**: the 15 bits are a conservative summary. If a generator
  can produce them (or a conservative over-approximation, "all connected")
  from noise bounds without generating voxels, the BFS can decide which
  neighbors to generate next. Over-approximating connectivity keeps it safe.
- Combine with depth: BFS order = front-to-back occluder insertion order for
  MSOC-style buffers.
