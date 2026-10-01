# Hi-Z and Two-Pass Occlusion (GPU reference)

- Nick Darnell, Hi-Z occlusion culling:
  https://www.nickdarnell.com/hierarchical-z-buffer-occlusion-culling/
- Nanite overview: https://cs418.cs.illinois.edu/website/text/nanite.html
- Nanite pipeline notes: https://www.thecandidstartup.org/2023/04/03/nanite-graphics-pipeline.html
- Primary source: Karis, Stubbe, Wihlidal, "A Deep Dive into Nanite Virtualized
  Geometry", SIGGRAPH 2021 Advances in Real-Time Rendering (not fetched).
- Greene et al. 1993, hierarchical Z-buffer (original).

Status: web summaries only.

## Hi-Z pyramid

Mip chain over a depth buffer where each coarser texel stores the **farthest**
depth of its 2x2 children (reverse-Z flips this to min). Example size: 512x256.

## Testing an AABB

Project to screen, pick the mip where the footprint spans at most about 2x2
texels, compare the box's nearest depth to the stored farthest depth. Gotcha
from Darnell: AMD's early sphere-width method ignored perspective distortion,
causing false negatives; use the proper projected width.

## Two-pass scheme (Nanite and others)

1. Pass 1: test against the *previous frame's* pyramid (reprojected with last
   frame's transforms), draw what passes; build this frame's pyramid.
2. Pass 2: re-test everything pass 1 culled against the new pyramid; draw the
   newly visible items; update the pyramid. Nanite does this per instance and
   then per cluster. Aokana does the same for (tile, chunk) pairs.

## Mapping to PrimeStruct

- A CPU software path (MSOC) avoids temporal reprojection entirely: occluders
  are rasterized this frame before queries, so no two-pass is needed.
- The two-pass structure matters only if culling later moves to a compute
  backend. Keep the culling API as "occluder insert + box query" so either
  implementation fits behind it.
