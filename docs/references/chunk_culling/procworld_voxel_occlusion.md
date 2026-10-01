# Procedural World: Voxel Occlusion

Blog post, 2015: http://procworld.blogspot.com/2015/08/voxel-occlusion.html

Status: web summary only; re-read the post before relying on details.

## Idea

Instead of rasterizing real geometry, derive *occluder quads* from each
chunk's voxel data: rectangles fully inscribed in the solid region. Rasterize
them into a small software depth buffer (the post uses 64x64 for speed). A
chunk is hidden if its screen footprint is covered by closer depth.

## Details reported

- Scan along the three axes to find maximal rectangles where rays enter and
  exit solid voxels.
- Air voxels adjacent to partial-geometry surfaces are handled conservatively
  (those surfaces do not become occluders).
- The author calls it "probably the best optimization we have ever done".
  No numbers given.

## Caveats

- Quality of occluders (size, count) controls culling rate; thin or fragmented
  solids yield few useful quads.
- Occluders must be rebuilt when a chunk's voxels change.

## Mapping to PrimeStruct

- Best fit for a first prototype: per-chunk occluder extraction (stdlib
  function over chunk voxel storage) + low-res depth buffer + AABB queries.
- Occluder extraction is a pure function of chunk contents: easy to unit-test
  and to cache with the chunk.
- Extraction can be done at meshing time, but a coarser *bounds-only* variant
  (e.g. "this 16^3 core is solid") could exist before full voxel data does;
  see terrain_horizon_and_generation.md.
