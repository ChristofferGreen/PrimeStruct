# Terrain Horizon Culling and Generation-Time Rejection

Sources:
- Cinevva, terrain-aware occlusion culling:
  https://app.cinevva.com/blog/2026-05-19-terrain-occlusion-culling
- Horizon occlusion culling for hierarchical terrains:
  https://www.researchgate.net/publication/220943527_Horizon_Occlusion_Culling_for_Real-time_Rendering_of_Hierarchical_Terrains
- Cesium horizon culling: https://cesium.com/blog/2013/04/25/horizon-culling/

Status: web summaries only.

## Search result: generation-time rejection

Searches (visibility-driven procedural generation, conservative noise bounds,
view-dependent generation) found **no paper or engine write-up** that skips
generating hidden chunks. Findings that do exist:
- Procedural worlds generate by proximity/streaming (generate under movable
  objects, evict by priority); this rejects by distance only.
- Cinevva argues PVS-style preprocessing is of little value for procedural,
  chunk-streamed terrain because "chunk-stream-time preprocessing costs about
  as much as just doing runtime visibility".
- Minecraft-style density-function bounds / interval-arithmetic early-outs were
  not documented in the public sources found (search returned only generic
  worldgen docs).

So everything under "Options" is derived from the sources above, not cited.

## Heightfield horizon culling (the one directly useful technique)

If occluders can be described by a 1D height function, visibility is a ray
march over a 2D heightmap. A max-height pyramid (mip chain storing max height)
lets the ray skip flat regions in huge steps and refine only over hills. Cinevva
numbers: level 0 256x256 at 2.25 m/texel (max of 4x4 jittered samples), a few
hundred KB, builds in tens of ms; brute force is about 24 samples per instance.

## Options for rejecting chunks before they are generated (derived)

1. **Coarse height/bounds first.** Generate only per-column min/max height (cheap
   noise evaluation) ahead of voxels. Horizon-test whole columns; generate
   voxels only for columns that pass. Safe if the coarse max is an upper bound
   of the real surface and the coarse min a lower bound.
2. **Occlusion-ordered generation.** Process chunks near-to-far (BFS order).
   After each chunk is generated, add its occluders to the depth buffer; test
   the next candidates against it and generate only survivors. Rejected chunks
   stay ungenerated until a later query passes.
3. **Conservative connectivity before voxels.** Provide the 15-bit face
   connectivity (cave_culling.md) from noise bounds, over-approximating
   (assume connected when unsure), so the BFS can drive generation.
4. **Solid-core occluders from bounds.** If bounds prove a region is solid
   (e.g. everything below the column's min height), it can serve as an
   occluder without generating its voxels.

## Risks

- Soundness: every rejection needs a proof from *bounds*, since no voxels
  exist. Under-estimating a max or over-estimating a min makes visible
  terrain disappear. Add a debug mode that generates everything and diffs
  against the rejected set.
- Features that break height-function assumptions (caves, overhangs, floating
  islands) must widen the bounds or disable the shortcut for that column.
- Latency: popping chunks in late costs a generation hitch; needs a
  look-ahead margin around the frustum.
