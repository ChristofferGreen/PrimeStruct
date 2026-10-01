# Chunk Culling: What We Set Out to Find, and What We Found

## The question

PrimeStruct is heading toward voxel and terrain worlds made of chunks. The
cheapest chunk is the one that is never drawn, and cheaper still is the one
that is never generated. We set out to learn how other engines and papers
decide that a chunk can be skipped, with a particular interest in a software
approach: rasterize a coarse depth buffer on the CPU (or in compute) and reject
every chunk that would write no visible pixel. We also wanted to know whether
the same idea can go one step further and reject chunks from *generation*, not
only from rendering.

## How we looked

We searched the web and Google Scholar for software occlusion culling,
voxel and Minecraft-style chunk culling, GPU Hi-Z pipelines, terrain horizon
culling, and generation-time rejection. We read four sources in full or in
their relevant parts (Masked Software Occlusion Culling, Aokana, GigaVoxels DP,
and the culling parts of the GPU work-graph generation paper), and read the
rest through summaries only; each note says which. We then followed
"cited by" links from the strongest papers to find newer work. Notes, links, and
three vendored PDFs are in this directory.

## What we found

**1. Software depth-buffer occlusion is mature and cheap.** Masked Software
Occlusion Culling (Intel, 2016) rasterizes a simplified occluder mesh into a
small hierarchical depth buffer on the CPU. It matches a full-resolution
hierarchical-Z result to within about 2%, runs in roughly a millisecond at
1080p, and has no GPU readback latency. Its one requirement is conservatism:
occluder depth is rounded farther, tested-box depth is rounded nearer, so the
test can be wrong only in the harmless direction. Its stated weakness is
authoring inner-conservative occluder meshes.

**2. Voxels solve the occluder problem.** A voxel engine post (Procedural
World) derives occluder quads directly from chunk data, as rectangles
fully inside solid space, and rasterizes them into a 64x64 buffer. The author
calls it their best optimization. Aokana (2025) contributes the key correctness
detail for chunks: using a chunk's bounding-box entry depth is automatically
conservative, except for the chunk containing the camera, which needs exact
treatment.

**3. Connectivity culling needs no rasterizer but over-includes.** Minecraft's
"cave culling" stores 15 bits per chunk (which face pairs are connected through
open space) and walks a breadth-first search from the camera. It culls 50 to 99%
of geometry, but the search can bend around corners and keep chunks that cannot
really be seen, and heuristics recover only 5 to 15%.

**4. GPU Hi-Z pipelines solve a problem we may not have.** Nanite-style
two-pass Hi-Z reuses last frame's depth pyramid and re-tests what it culled.
That machinery exists to avoid reading depth back. A CPU software rasterizer
that fills occluders in front-to-back order before querying does not need it.

**5. Rejecting from generation is mostly an open area.** No source we found
skips generating hidden volume because it is occluded using bounds alone.
Two pieces come close:
- GigaVoxels DP (2024) generates voxel bricks only when a ray reaches them, so
  anything hidden behind an opaque surface is never produced. The mechanism is
  visibility-driven demand with a four-state brick table (empty, missing,
  in-production, resident). It runs on the GPU and has frame-time spikes on
  disocclusion.
- The GPU work-graph generation paper (2024) skips generation of objects
  outside the frustum using worst-case bounding volumes and a coarse-then-fine
  grid test (55,642 objects omitted in one view). It uses no occlusion at all.
Everything else we found is distance-based streaming. A short post on
heightfield terrain adds that a max-height pyramid turns terrain visibility
into a cheap ray march, and argues that precomputed visibility sets are of
little value for procedural, streamed worlds.

**6. Newer papers worth reading next.** The "cited by" search surfaced a 2025
paper on two-pass occlusion for dynamic voxel scenes (not read; paywalled),
hierarchical raster occlusion culling (2021), and a cluster of occluder
generation papers. Potentially-visible-set work looks like a poor fit.

## What it means for us

The reading supports a specific plan. Combine a small conservative software
depth buffer with near-to-far traversal and a chunk state machine, so that
generation is driven by visibility. Add one idea that none of the sources
state: *bound-derived occluders*. If a generator can promise that everything
below a certain height is solid, chunks under that height can act as occluders
before any voxel exists, so even the first pass can reject hidden chunks
before generating them. That promise must only ever under-estimate, because
a wrong rejection is a visible bug while a missed one only costs time.

We deliberately skip GPU Hi-Z, precomputed visibility sets, and sparse voxel
DAGs for now.

## What we have not proven

- Bound-derived occluders are our own synthesis. No source validates them,
  and they may not beat voxel-derived occluders enough to justify their
  complexity. That is exactly what the first experiment measures.
- Several sources were read as summaries only, and the voxel two-pass paper
  was not read at all.
- Rejection results for caves, overhangs, and floating features are unknown;
  they are the cases most likely to break a height-based bound.

## Next step

Build a small deterministic simulator on a seeded heightfield-and-caves world,
compare four methods against an exact full-depth-buffer ground truth, and
require zero wrong rejections. The design and plan are in `docs/ChunkCulling.md`.
