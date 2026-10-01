# Masked Software Occlusion Culling (MSOC)

Hasselgren, Andersson, Akenine-Moller (Intel). High Performance Graphics 2016.
- Paper: https://fileadmin.cs.lth.se/graphics/research/papers/2016/culling/culling.pdf
- Code (Apache-2.0): https://github.com/GameTechDev/MaskedOcclusionCulling
- Overview: https://www.intel.com/content/www/us/en/developer/articles/technical/masked-software-occlusion-culling.html

Status: paper text read.

## Idea

Rasterize a coarse set of occluder triangles on the CPU into a *hierarchical*
depth buffer designed for occlusion queries only, then test bounding boxes
against it. Reported: about 1 ms for the scene's first frame at 1920x1080
(AVX2), culling within about 2% of a full-resolution HiZ, about 3x faster than
a SIMD HiZ baseline. Query and occluder rasterization can be interleaved, so it
fits scene-graph traversal (front-to-back, test, then add as occluder).

## Data structure

Per tile (32x8 pixels with AVX2, 256-bit mask):
- `zMax0` reference layer depth,
- `zMax1` working layer depth,
- a 32-bit-per-lane mask saying which layer each pixel belongs to.

No per-pixel depth. Memory is tiny; the whole buffer is SIMD-friendly.

## Rasterizing an occluder triangle

1. Visit tiles overlapped by the triangle's bounding box.
2. Coverage mask for a whole tile from the three edge functions using a few
   SIMD shifts/AND ops (all three edges only in tiles overlapping the middle
   vertex, two edges elsewhere).
3. Update the tile conservatively using the triangle's `zMax`:
   - if the triangle is farther than the working layer by more than the gap
     between layers, discard the working layer (prevents background
     silhouettes leaking through foreground occluders);
   - merge the triangle into the working layer (`zMax1 = max(zMax1, tri.zMax)`);
   - when the combined mask is full, the working layer replaces the reference
     layer and the working layer is cleared.
   Invariant: `zMax0 >= zMax1`, so queries compare only against `zMax0`.

## Occlusion query

Project the box to a screen rectangle with a minimum depth `zMin`. Walk the
tiles under the rectangle; the object is occluded only if `zMin > zMax0` for
**every** overlapped tile. Early-out on the first tile that fails.

## Constraints

- Occluders must be *inner-conservative* (inside the real geometry). The paper
  flags the burden of authoring such meshes.
- Depth rounding is deliberately conservative: false "visible" is allowed,
  false "occluded" is not.

## Mapping to PrimeStruct

- Chunk occluders can be generated from voxel data (see
  procworld_voxel_occlusion.md), solving the authoring problem.
- Tile mask math (edge functions as bit ops) fits the existing software
  renderer in `examples/shared/scene_bgra8_renderer.h`; a scalar 64-bit-mask
  variant (8x8 tiles) is a portable first version before any SIMD.
- Tests: golden depth-buffer dumps for small occluder sets, plus a property
  test that no chunk rejected by the hierarchical buffer is visible in a
  brute-force full-depth-buffer render.
