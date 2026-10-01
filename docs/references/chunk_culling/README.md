# Chunk Culling References

Reading notes for rejecting voxel/terrain chunks from rendering (and, where
possible, from generation). Notes are written for PrimeStruct contributors;
each ends with a "Mapping" section. Three open-access PDFs are vendored in
`pdfs/` (~19 MB each): `msoc_hpg2016.pdf`, `aokana_2505.02017.pdf`, and `gigavoxels_dp.pdf`.
Everything else is linked.

Status legend: **read** = full text read, **summary** = read via a web
summary only, so details may be incomplete.

| Note | Source | Status | Verdict |
|---|---|---|---|
| [msoc.md](msoc.md) | Masked Software Occlusion Culling (HPG 2016) | read | Core CPU depth-buffer method |
| [procworld_voxel_occlusion.md](procworld_voxel_occlusion.md) | Procedural World blog, "Voxel Occlusion" | summary | Closest match to chunk occlusion by voxel-derived occluders |
| [aokana.md](aokana.md) | Aokana (arXiv 2505.02017, 2025) | read (sections 3.x) | Chunk selection, Hi-Z tiles, streaming/LOD |
| [cave_culling.md](cave_culling.md) | Checchi, Advanced Cave Culling parts 1 and 2 | summary | Cheap connectivity-graph culling, no rasterizer |
| [hiz_two_pass.md](hiz_two_pass.md) | Nanite two-pass HZB, Darnell Hi-Z | summary | Reference for a future compute/GPU path |
| [terrain_horizon_and_generation.md](terrain_horizon_and_generation.md) | Horizon culling, Cinevva terrain post, generation-time search | summary | Only material bearing on pre-generation rejection |
| [gigavoxels_dp.md](gigavoxels_dp.md) | GigaVoxels DP (HPG 2024) | read | Visibility-driven on-demand brick production: closest match to rejecting chunks from generation |
| [kuth_work_graphs.md](kuth_work_graphs.md) | Kuth et al., GPU work graphs (HPG 2024) | read (culling parts) | Frustum-only generation culling from worst-case bounds; coarse-then-fine |
| [citation_leads.md](citation_leads.md) | Google Scholar "cited by" sweep | titles/abstracts | Newer papers to read next (voxel two-pass HZB, GigaVoxels DP, work graphs) |

## Link-only further reading

- Vintage Story ray-cast chunk culling: https://github.com/tyronx/occlusionculling
- Intel MSOC code (Apache-2.0): https://github.com/GameTechDev/MaskedOcclusionCulling
- Voxy chunk-bound depth mask: https://github.com/srjefers/voxy-mseries-support/pull/4
- Voxel.wiki culling index: https://voxel.wiki/categories/culling/
- GPU occlusion experiments: https://interplayoflight.wordpress.com/2017/11/15/experiments-in-gpu-based-occlusion-culling/
- Life is Feudal GPU occlusion: https://bazhenovc.github.io/blog/post/gpu-driven-occlusion-culling-slides-lif/
- Cesium horizon culling: https://cesium.com/blog/2013/04/25/horizon-culling/
- CuRast (CUDA software rasterization, off-topic for culling): https://arxiv.org/pdf/2604.21749

## Cross-cutting takeaways

1. Every depth-based method needs the same invariant: the test must be
   **conservative**. Occluder depth is rounded *farther*, occludee depth is
   rounded *nearer*. A chunk is rejected only if provably no pixel it would
   write survives the depth test.
2. Chunks are tested by bounding box, so only the camera's own chunk needs
   exact treatment (Aokana "self chunk").
3. Occluders do not need to be the real geometry. Per-chunk inscribed solid
   quads (Procedural World) or box occluders keep rasterization cheap.
4. Connectivity (cave culling) and depth (MSOC/Hi-Z) are complementary: the
   first prunes the traversal, the second prunes what the traversal reaches.
5. Nothing found rejects chunks *before generation* directly; see
   terrain_horizon_and_generation.md for the options derived from these.
