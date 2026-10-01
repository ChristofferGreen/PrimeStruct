# Citation Leads (Google Scholar "Cited by" sweep)

Searched 2026-10-01 via scholar.google.com (cites= lists for MSOC and the
Masked depth culling paper, plus topic queries). Scholar rate-limits quickly
(HTTP 429 after about 3 requests); the Aokana "cited by" list came back empty
(it has ~1 citation). Only titles/abstracts were read for everything below;
none of these papers has been read in full. Ranked by relevance to chunk
rejection.

## Highest priority

| Paper | Why |
|---|---|
| Omlor, Radicke, Stiegler et al., *Two-Pass Occlusion Culling for Dynamic Voxel Scenes based on Hierarchical Z-Buffering*, 2025. https://ieeexplore.ieee.org/abstract/document/11321175/ | Exactly our domain: dynamic voxel volumes, sparse voxel octree, full octree nodes replaced by larger proxy geometry to build the HZB. Reports up to 90% faster than no culling, 42% faster than per-octree-node culling (mesh shading). Cites MSOC. Paywalled (IEEE returns 418 to scripted fetch) and no open preprint found, so not read or vendored; ask the user or authors for a copy. |
| Lee, Jeong, Seok, Lee, *Hierarchical Raster Occlusion Culling*, CGF 2021. https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.142649 | Rasterizes coarse groups of occludees in a BVH, per-pixel ray cast inside; constant draw calls for batch tests. Related thesis: Lee, *Efficient Object Visibility Culling with Screen-Space Ray Casting* (cg.skku.edu, 2021). |
| (READ, see gigavoxels_dp.md) Richermoz, Neyret, *GigaVoxels DP*, HPG/PACMCGIT 2024. https://dl.acm.org/doi/10.1145/3675389 | Ray-guided cache that **produces only visible voxel bricks on demand**. This is the closest published mechanism to "reject chunks from generation": production is driven by what rays actually hit. Starvation/synchronization is the hard part. |
| (READ, see kuth_work_graphs.md) Kuth et al., *Real-Time Procedural Generation with GPU Work Graphs*, HPG 2024 (Best Paper). https://dl.acm.org/doi/10.1145/3675376 | Procedural generation as a dynamic work graph, so nodes can cull/skip child generation before expanding. Not voxel-specific. Read: culling is frustum-only, from worst-case bounds; no occlusion before generation. |

## Medium priority

| Paper | Why |
|---|---|
| Wu, He, Pan, Gao, *Occluder generation for buildings in digital games*, CGF 2022. https://onlinelibrary.wiley.com/doi/10.1111/cgf.14669 | Automatic occluder generation, relevant to our per-chunk occluder extraction. |
| Cao et al., *Practical Occluder Generation for Mobile Games*, 2026 (IEEE). | Follow-up on occluder generation. |
| Wu, Lin, Lu, *DR-occluder*, TOG 2023; Tan et al., *Differentiable Rendering based Part-Aware Occlusion Proxy Generation*, CGF 2025. | Learned/optimized occluder proxies; likely overkill for axis-aligned voxel quads. |
| Gonakhchyan, *Occlusion culling algorithm based on software visibility checks*, 2020. | Software visibility checks; cited in the same MSOC neighborhood. |
| Davies, Strugar (Intel), *Merging Masked Occlusion Culling Hierarchical Buffers*. https://www.intel.com/content/dam/develop/external/us/en/documents/merging-masked-occlusion-culling-hierarchical-buffers-faster-rendering.pdf | Multi-threaded buffer merge for MSOC. Matters if culling is parallelized. |
| van den Bergen, *Conservative mesh decimation for collision detection and occlusion culling*, GDC 2021. | How to get inner-conservative occluders from meshes. |

## PVS-style (likely low value for procedural worlds, but note them)

- Voglreiter et al., *Trim Regions for Online Computation of From-Region PVS*, TOG 2023.
- Künzel et al., *Potentially Visible Set Generation with the Disocclusion Buffer*, 2025.
- Wang et al., *NeuralPVS: Learned Estimation of Potentially Visible Sets*, 2025.
Cinevva's argument (terrain_horizon_and_generation.md) that stream-time
preprocessing costs as much as runtime visibility applies; these are worth a
skim only for from-region conservativeness ideas.

## Other leads

- Unterguggenberger et al., *Conservative meshlet bounds for robust culling of
  skinned meshes*, CGF 2021 (conservative bounds under deformation; cites
  Masked depth culling).
- Anglada et al., early visibility resolution / Omega-test (hardware early-Z
  prediction; not useful for software culling).
- Zhang et al., *Visibility culling using hierarchical occlusion maps*, 1997
  (HOM; classic software occlusion maps predating MSOC).
- Sudarsky, Gotsman, *Dynamic scene occlusion culling*, TVCG 1999.
- Schutz et al., *Potree* (point cloud streaming; visibility-driven loading
  pattern).
- Molenaar, Eisemann, *Transform-Aware Sparse Voxel DAGs*, 2025 and Modisett,
  Billeter, *Encoding Occupancy in Memory Location*, CGF 2025 (voxel storage,
  not culling).

## Suggested next reads (in order)

1. Omlor et al. 2025 (voxel two-pass HZB with octree proxies).
2. GigaVoxels DP 2024 (on-demand production of visible bricks).
3. Kuth et al. 2024 (culling inside procedural generation graphs).
4. Lee et al. 2021 (hierarchical raster occlusion culling).
