# Aokana: GPU-Driven Voxel Rendering for Open World Games

Fang, Wang, Wang. Proc. ACM CGIT 8(1), Article 10, May 2025.
https://arxiv.org/pdf/2505.02017 (open access, ~19 MB)

Status: section 3 (method) read in full; experiments (section 4) not read.

## Pipeline

1. **Chunk selection**: frustum cull per-chunk AABBs (AABB covers only the
   valid voxels, precomputed).
2. **Tile selection**: screen split into 8x8 tiles. Each tile projects rays
   against chunk AABBs, tests the AABB entry depth against the **previous
   frame's Hi-Z**, and emits (tile, chunk) pairs.
3. **Ray march** SVDAG chunks per pair, writing a 64-bit visibility buffer
   (24-bit depth | 3-bit normal | 13-bit chunk id | 24-bit voxel xyz) with
   `InterlockedMax` as the depth test.
4. Build this frame's Hi-Z, **re-run tile selection on tiles that were culled**
   in step 2, march them too (fixes errors from stale previous-frame depth).
5. Color resolve from the visibility buffer.

## Key insight (reusable)

For chunks other than the camera's, the ray/AABB entry depth is always nearer
than the true surface depth, so using it for Hi-Z testing is conservative. For
the **self chunk** (camera inside) the AABB depth is *farther* than the
surface, so it would cull wrongly; the self chunk must use real intersection
depth.

## Data and streaming

- World split into M^3 chunks, each 256^3 voxels as a Sparse Voxel DAG
  (geometry and color compressed separately; 4x4x4 leaves as 64-bit bitmaps).
- LOD: 8 chunks aggregate into one LOD n+1 chunk of the same resolution
  (voxel kept if >= 2 of 8 children are non-empty).
- CPU-side implicit octree decides what to load when the camera's chunk
  changes. `LODError = ChunkSize * StreamingFactor - |ChunkCenter - Camera|`;
  error > 0 subdivides, error < 0 loads. Higher-LOD chunks load first and
  evict their children; nearer chunks load first when memory allows.

## Relevance and limits

- Streaming is distance/LOD-driven, not occlusion-driven: it does not reject
  chunks from loading because they are hidden. (Content here is pre-authored
  and preprocessed, not generated on demand.)
- GPU compute focus; the pipeline assumes ray marching, not meshes.

## Mapping to PrimeStruct

- Adopt: AABB-depth conservativeness rule and self-chunk exception; two-stage
  test (stale depth first, re-test culled set with fresh depth).
- Adopt: precomputed tight AABB over valid voxels per chunk.
- Adapt: tile-chunk pair lists are a good shape for a CPU software path too.
- Not needed initially: SVDAG, LOD octree.
