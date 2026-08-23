#include "third_party/doctest.h"

#include "test_compile_run_examples_docs_locks_shared.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("todo queue and skipped doctest debt stay source locked: ready-now and priority lanes") {
  const DocsLocksTodoQueueFixture fixture = loadDocsLocksTodoQueueFixture();
  const std::string &todo = fixture.todo;
  const std::string &todoFinished = fixture.todoFinished;

  CHECK(todo.find("## Purpose") != std::string::npos);
  CHECK(todo.find("Do not keep completed-task summaries, historical rollout notes, or closed\n"
                  "  coverage snapshots in this file.") !=
        std::string::npos);
  // Split into (a) the exact expected Ready Now bullet set and (b) a
  // looser check that the section header sequence still follows it,
  // rather than one giant literal spanning the free-text "Note (...)"
  // paragraph docs/todo.md keeps between the bullets and "### Immediate
  // Next 10" - that paragraph's own wording is expected to keep evolving
  // (e.g. TODO-5237 finishing) independent of which TODOs are queued, so
  // locking its exact prose here would make this test needlessly brittle
  // to changes that don't affect the actual queue content this check
  // cares about (see TODO-5237's resolution in docs/todo_finished.md).
  CHECK(todo.find("### Ready Now\n\n"
                  "- TODO-4739: Fix vector/at direct-call override precedence - multiple redundant, inconsistent native-fastpath classification sites | track: exe-backend-vector-at-precedence | surface: native-fastpath call classification\n"
                  "- TODO-4743: Reduce diffuse per-call resolution cost left over after TODO-4742's hasDefinitionFamilyPath fix | track: semantics-call-resolution-perf | surface: semantics call/definition resolution\n") !=
        std::string::npos);
  CHECK(todo.find("TODO-5255") == std::string::npos);
  CHECK(todoFinished.find("TODO-5255: Resync docs-lock test with docs/todo.md's Ready Now/Immediate Next 10/Execution Queue content") !=
        std::string::npos);
  CHECK(todo.find("### Immediate Next 10") != std::string::npos);
  CHECK(todo.find("- TODO-4604: Specify requirement contract phase split") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4601: Remove final map helper classifier trace | track: "
                  "map-helper-zero") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4602: Remove semantic vector-literal compiler traces | track: "
                  "semantic-vector-literal-zero") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4603: Remove IR-lowerer vector-literal compiler traces | track: "
                  "lowerer-vector-literal-zero") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4579: Enforce zero map/vector compiler-knowledge traces | track: "
                  "map-vector-zero-gate") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4579: Enforce zero map/vector compiler-knowledge traces") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4603: Remove IR-lowerer vector-literal compiler traces") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4614: Retire IR-lowerer call-helper source locks") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4614: Retire IR-lowerer call-helper source locks") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4598: Migrate semantics collection surface lookups") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4599: Migrate emitter collection surface lookups") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4600: Migrate IR lowerer collection surface lookups") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4615: Retire emitter private source locks | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4615: Retire emitter private source locks") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4616: Make semantic validation manifest executable | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4616: Make semantic validation manifest executable") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4617: Add semantic preflight missing-fact diagnostics | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4617: Add semantic preflight missing-fact diagnostics") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4618: Fail closed on stale CT-eval requirement facts | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4618: Fail closed on stale CT-eval requirement facts") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4619: Gate runtime reflection by backend profile | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4619: Gate runtime reflection by backend profile") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4621: Classify unsupported variadic-pack diagnostics | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4621: Classify unsupported variadic-pack diagnostics") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4569: Present scene-rendered UI through software surface bridge | track: "
                  "ui-scene-presentation") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4568: Emit scene nodes from the existing UI layout/widgets | track: "
                  "ui-scene-adapter") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4595: Add deterministic scene text shaping runs | track: "
                  "scene-text-shaping") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4596: Rasterize shaped scene text through a glyph atlas | track: "
                  "scene-text-rasterization") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4590: Add international text shaping and glyph atlas path | track:") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4567: Render first globally lit 3D SDF widget primitive | track: "
                  "scene-3d-sdf") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4565: Add minimal scene graph and camera data model | track: scene-renderer") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4570: Retire duplicate map2 candidate surface") ==
        std::string::npos);
  CHECK(todo.find("map2 replacement candidate") == std::string::npos);
  CHECK(todo.find("- TODO-4571: Add compiler-knowledge inventory for map/vector | track: collection-audit") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4572: Remove vector statement-helper compiler path | track: vector-special-case-deletion") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4573: Remove compiler-owned map literal lowering | track: map-special-case-deletion") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4575: Remove map helper/access compiler classifiers | track: map-special-case-deletion") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4574: Remove vector count/access compiler classifiers | track: vector-helper-classifier-deletion") ==
        std::string::npos);
  CHECK(todo.find("### Immediate Next 10\n\n"
                  "- TODO-4739: Fix vector/at direct-call override precedence - multiple redundant, inconsistent native-fastpath classification sites") !=
        std::string::npos);
  CHECK(todo.find("### Priority Lanes") != std::string::npos);
  CHECK(todo.find("Source-unit provenance ledger: TODO-4592 completed parser/semantic") ==
        std::string::npos);
  CHECK(todo.find("TODO-4583 added the IR schema/version\n"
                  "  contract that TODO-4593 must follow") ==
        std::string::npos);
  CHECK(todo.find("Scene graph renderer and UI presentation: TODO-4565 completed the data-only\n"
                  "  scene model and TODO-4566 completed the first BGRA8 2D primitive renderer;\n"
                  "  TODO-4567 completed the first globally lit 3D SDF widget primitive, and\n"
                  "  TODO-4595 completed deterministic shaped glyph runs. TODO-4596 completed\n"
                  "  deterministic text atlas/raster composition. TODO-4568 completed the first\n"
                  "  UI scene-record adapter, and TODO-4569 completed the software-surface UI\n"
                  "  presentation bridge.") !=
        std::string::npos);
  CHECK(todo.find("Map/vector compiler-independence: TODO-4570 retired the duplicate `map2`\n"
                  "  surface, TODO-4571 added the compiler-knowledge inventory categories") !=
        std::string::npos);
  CHECK(todo.find("Safe array extents and capability views: TODO-4604 completed the requirement\n"
                  "  contract phase split, TODO-4622 implemented the first contract-form\n"
                  "  `require(...)` runtime slice (integer-parameter and `count(parameter)`\n"
                  "  comparisons lower to deterministic call-boundary checks), and TODO-4605\n"
                  "  completed the non-null safe pointer optionality model. TODO-4606 specified the capability-parameterized\n"
                  "  reference/slice view model in the normative docs. TODO-4607 published the\n"
                  "  initial semantic-product array extent facts, and TODO-4608 added the first\n"
                  "  checked read-only array slice construction surface. TODO-4609 added the\n"
                  "  first conservative view-escape diagnostic") !=
        std::string::npos);
  // Split into a loose header check and the exact first-entry literal,
  // rather than one giant literal spanning the free-text "Note (...)"
  // paragraph docs/todo.md keeps between the header and the numbered
  // list (same reasoning as the "Ready Now" split above - that note's
  // prose is expected to keep evolving independent of which TODOs are
  // queued).
  CHECK(todo.find("### Execution Queue") != std::string::npos);
  CHECK(todo.find(
            "73. TODO-4739: Fix vector/at direct-call override precedence - multiple redundant, inconsistent native-fastpath classification sites\n") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4613: Retire semantic-validator private source locks | track: "
                  "semantic-source-lock-retirement") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4613: Retire semantic-validator private source locks") ==
        std::string::npos);
  CHECK(todo.find("### Task Blocks\n\n"
                  "- [x] TODO-4635: Derive the collection surface registry "
                  "from stdlib declarations") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4607: Publish initial array extent facts") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4606: Specify capability-parameterized views | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4606: Specify capability-parameterized views") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4605: Specify non-null pointer optionality | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4605: Specify non-null pointer optionality") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4605: Specify non-null pointer optionality") !=
        std::string::npos);
  CHECK(todoFinished.find("Documented `Pointer<T>` as valid non-null storage identity in safe code") !=
        std::string::npos);
  CHECK(todoFinished.find("Promoted TODO-4606 as the next safe-array docs leaf") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4606: Specify capability-parameterized views") !=
        std::string::npos);
  CHECK(todoFinished.find("Documented `Reference<T, Capability>` as the non-null count-one view\n"
                          "      shape") != std::string::npos);
  CHECK(todoFinished.find("Documented `Slice<T, Capability>` as the runtime-count contiguous view\n"
                          "      shape") != std::string::npos);
  CHECK(todoFinished.find("Promoted TODO-4607 as the next safe-array semantic-product leaf") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4607: Publish initial array extent facts") !=
        std::string::npos);
  CHECK(todoFinished.find("Added `SemanticProgramArrayExtentFact` plus deterministic dump") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4599: Migrate emitter collection surface lookups | track: "
                  "stdlib-registry-emitter") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4599: Migrate emitter collection surface lookups") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4599: Migrate emitter collection surface lookups") !=
        std::string::npos);
  CHECK(todoFinished.find("Production `src/emitter/` no longer contains the assigned") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4615: Retire emitter private source locks") !=
        std::string::npos);
  CHECK(todoFinished.find("source C++ emitter contract that emits a block expression") !=
        std::string::npos);
  CHECK(todoFinished.find("expression-emitter private source lock was\n"
                          "    retired without refactoring backend emitter ownership") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4619: Gate runtime reflection by backend profile") !=
        std::string::npos);
  CHECK(todoFinished.find("Added `RuntimeReflection` to the backend capability profile") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4613: Retire semantic-validator private source locks") !=
        std::string::npos);
  CHECK(todoFinished.find("Removed the private semantic-validator source-lock test") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4616: Make semantic validation manifest executable") !=
        std::string::npos);
  CHECK(todoFinished.find("Added typed `SemanticValidationPassId` values") !=
        std::string::npos);
  CHECK(todoFinished.find("Promoted TODO-4617 as the next semantic-product diagnostics follow-up") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4617: Add semantic preflight missing-fact diagnostics") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4618: Fail closed on stale CT-eval requirement facts") !=
        std::string::npos);
  CHECK(todoFinished.find("rejects\n"
                          "      missing published facts, incomplete string-table ids") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4621: Classify unsupported variadic-pack diagnostics") !=
        std::string::npos);
  CHECK(todoFinished.find("Promoted the lowerer/backend reference-pack forwarding diagnostic") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4602: Remove semantic vector-literal compiler traces") !=
        std::string::npos);
  CHECK(todoFinished.find("Routed vector collection-literal diagnostic subjects through\n"
                          "      `collection literal` wording") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4579: Enforce zero map/vector compiler-knowledge traces") !=
        std::string::npos);
  CHECK(todoFinished.find("PrimeStruct_map_vector_compiler_knowledge_zero_audit") !=
        std::string::npos);
  CHECK(todoFinished.find("broad `--enforce-zero` audit") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4569: Present scene-rendered UI through software surface bridge") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4595: Add deterministic scene text shaping runs") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4596: Rasterize shaped scene text through a glyph atlas") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4595: Add deterministic scene text shaping runs") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4596: Rasterize shaped scene text through a glyph atlas") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4568: Emit scene nodes from the existing UI layout/widgets") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4569: Present scene-rendered UI through software surface bridge") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4600: Migrate IR lowerer collection surface lookups") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4601: Remove final map helper classifier trace") !=
        std::string::npos);
  CHECK(todoFinished.find("collectionMemberPath(\"map\", \"map\")") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4603: Remove IR-lowerer vector-literal compiler traces") !=
        std::string::npos);
  CHECK(todoFinished.find("collectionLiteralExceedsLocalCapacityLimitMessage()") !=
        std::string::npos);
  CHECK(todoFinished.find("scans production\n"
                          "      `src/ir_lowerer/` `.cpp`/`.h` files for `VectorLiteral`,") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4614: Retire IR-lowerer call-helper source locks") !=
        std::string::npos);
  CHECK(todoFinished.find("public helper-contract coverage") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4598: Migrate semantics collection surface lookups") !=
        std::string::npos);
  CHECK(todoFinished.find("`--software-surface-ui-demo`") != std::string::npos);
  CHECK(todoFinished.find("ui_scene_surface_bridge.h") != std::string::npos);
  CHECK(todoFinished.find("Added `UiScene`, `UiSceneNodes`, and `UiSceneTextOverlays` records") !=
        std::string::npos);
  CHECK(todoFinished.find("LayoutTree.emit_scene_panel`, `emit_scene_label`") !=
        std::string::npos);
  CHECK(todoFinished.find("Local native UI scene adapter slice passed") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4568: Emit scene nodes from the existing UI layout/widgets") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4569: Present scene-rendered UI through software surface bridge") ==
        std::string::npos);
  CHECK(todoFinished.find("shapeSceneTextUtf8()") !=
        std::string::npos);
  CHECK(todoFinished.find("renderSerializedSceneWithTextOverlayToBgra8()") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4591: Add expanded-source provenance ledger") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4592: Map parser and semantic diagnostics through source units") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4593: Carry source-unit provenance into IR and VM debug maps") !=
        std::string::npos);
  CHECK(todoFinished.find("source-unit identity to\n"
                          "      instruction source-map entries") != std::string::npos);
  CHECK(todoFinished.find("TODO-4620: Index expanded-source segments for diagnostics") !=
        std::string::npos);
  CHECK(todoFinished.find("per-line segment buckets") !=
        std::string::npos);
  CHECK(todoFinished.find("source_location_mapper_lookup_budget.json") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4620: Index expanded-source segments for diagnostics") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4620: Index expanded-source segments for diagnostics | track:") ==
        std::string::npos);
  CHECK(todo.find("TODO-4592: Map parser and semantic diagnostics through source units") ==
        std::string::npos);
  CHECK(todo.find("TODO-4593: Carry source-unit provenance into IR and VM debug maps") ==
        std::string::npos);
  CHECK(todoFinished.find("Added public `ExpandedSource`, `SourceUnit`, and `SourceSegment` types") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4564: Lock scene renderer defaults and UI producer contract | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4564: Lock scene renderer defaults and UI producer contract") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4564: Lock scene renderer defaults and UI producer contract") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4565: Add minimal scene graph and camera data model") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4566: Render flat and rounded-rect scene primitives to BGRA8") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4565: Add minimal scene graph and camera data model") !=
        std::string::npos);
  CHECK(todoFinished.find("Added `stdlib/std/scene/scene.prime` as a data-only scene authoring") !=
        std::string::npos);
  CHECK(todoFinished.find("pixel rendering remains with TODO-4566") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4566: Render flat and rounded-rect scene primitives to BGRA8") !=
        std::string::npos);
  CHECK(todoFinished.find("Added `examples/shared/scene_bgra8_renderer.h` as a narrow shared helper") !=
        std::string::npos);
  CHECK(todoFinished.find("rounded-rect 2D SDF coverage when radius is\n"
                          "      positive") != std::string::npos);
  CHECK(todo.find("- [ ] TODO-4567: Render first globally lit 3D SDF widget primitive") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4567: Render first globally lit 3D SDF widget primitive") !=
        std::string::npos);
  CHECK(todoFinished.find("Added `/std/scene` primitive kind `primitive_sdf_button()`") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4571: Add compiler-knowledge inventory for map/vector") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4573: Remove compiler-owned map literal lowering") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4575: Remove map helper/access compiler classifiers") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4576: Remove map backing-type compiler classification | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4576: Remove map backing-type compiler classification") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4576: Remove map backing-type compiler classification") !=
        std::string::npos);
  CHECK(todoFinished.find("map-backing-classifier` category must remain at zero") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4572: Remove vector statement-helper compiler path") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4577: Remove vector backing-type compiler classification") !=
        std::string::npos);
  CHECK(todoFinished.find("The map/vector compiler-knowledge inventory now reports no\n"
                          "      `vector-backing-classifier` category.") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4570: Retire duplicate map2 candidate surface") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4562: Add task handle semantic facts and lifetime diagnostics | track:") ==
        std::string::npos);
  CHECK(todo.find("### Ready Now (Parallel-Candidate Leaves; No Unmet TODO Dependencies)") ==
        std::string::npos);
  CHECK(todo.find("### Immediate Next 10 (Track Successors; Not Ready Until Dependencies Land)") ==
        std::string::npos);
  CHECK(todo.find("- `soa-zero-audit`: TODO-4524/TODO-4529 completed the strict\n"
                  "  zero-production-trace audit; no SoA zero-audit leaf is ready.") ==
        std::string::npos);
  CHECK(todo.find("- `map-zero-audit`: TODO-4537 split the broad lowerer substrate item into\n"
                  "  bounded leaves; TODO-4539 removed generated MapValue path synthesis, and\n"
                  "  TODO-4540 removed the lowerer key/value local kind plus ref/pointer flags.") ==
        std::string::npos);
  CHECK(todo.find("no map zero-audit leaf is ready.") == std::string::npos);
  CHECK(todo.find("- `tuple-type-packs`: TODO-4276 completed helper/lifecycle pack\n"
                  "  expansion, TODO-4271 added compile-time pack indexing, TODO-4272 added\n"
                  "  the initial stdlib tuple surface, and TODO-4274 added tuple bracket\n"
                  "  indexing, TODO-4273 added heterogeneous `make_tuple` inference, and\n"
                  "  TODO-4277 added tuple destructuring. TODO-4278 added task multi-wait over\n"
                  "  ordinary stdlib tuples; no tuple-type-packs leaf is ready.") ==
        std::string::npos);
  CHECK(todo.find("- `multithreading-substrate`: TODO-4545 was split into TODO-4561,\n"
                  "  TODO-4562, and TODO-4563 so single-task spawn/wait can land as parser/effect,\n"
                  "  semantic/lifetime, and runtime execution slices before TODO-4278. TODO-4561\n"
                  "  locked the parser/effect spelling, TODO-4562 added semantic/lifetime facts\n"
                  "  and diagnostics, and TODO-4563 added single-task VM/native runtime\n"
                  "  execution; no multithreading-substrate leaf is ready.") ==
        std::string::npos);
  CHECK(todo.find("- `procedural-genericity`: TODO-4336 allowed type locals in local binding and\n"
                  "  struct-field envelopes, TODO-4337 added non-escaping local generated\n"
                  "  structs, TODO-4338 stabilized deterministic generated identity and\n"
                  "  provenance, TODO-4339 lowered procedural facts through semantic-product\n"
                  "  direct-call and layout metadata, and TODO-4340 added docs and positive\n"
                  "  examples, and TODO-4546 added negative conformance; no procedural-genericity\n"
                  "  leaf is ready.") ==
        std::string::npos);
  CHECK(todo.find("- `generic-requirements`: TODO-4331, TODO-4334, TODO-4341, TODO-4342,\n"
                  "  TODO-4343, TODO-4344, TODO-4352, TODO-4353, and TODO-4354 are complete;\n"
                  "  TODO-4355 wired the compile-time host to published `/std/meta/*` predicate\n"
                  "  facts; TODO-4356 prepared restricted compile-time callables; TODO-4357\n"
                  "  evaluates pure user predicates; TODO-4345 added statement-level concrete\n"
                  "  `ct_if`; TODO-4547 added generic-specialized branch selection, and\n"
                  "  TODO-4548 added expression-position `ct_if` values; TODO-4549 scoped\n"
                  "  selected-branch generated type facts; TODO-4346 documented compile-time flow\n"
                  "  policy; TODO-4550 enforced active compile-time budget limits; TODO-4358\n"
                  "  enforced phase-qualified compile-time effects; TODO-4551 added\n"
                  "  deterministic cache keys and invalidation; TODO-4347 routed requirement\n"
                  "  facts into overload selection; and TODO-4351 added integer value\n"
                  "  requirement facts. TODO-4348 was split into bounded diagnostic leaves,\n"
                  "  TODO-4552 published provenance-rich direct requirement failures,\n"
                  "  TODO-4553 published requirement-overload diagnostics, TODO-4554 published\n"
                  "  compile-time-flow diagnostics, and TODO-4555 added predicate/value\n"
                  "  conformance coverage. TODO-4556 added focused conformance for compile-time\n"
                  "  effect opt-ins, cache invalidation material, and active budget enforcement.\n"
                  "  TODO-4557 locked compiler-hosted CT VM boundary coverage. TODO-4558 added\n"
                  "  parser and source-lock conformance for public generic requirement and\n"
                  "  `ct_if` syntax. TODO-4560 added compile-run and user-facing diagnostic\n"
                  "  conformance coverage. TODO-4559 added semantic-product and IR-preparation\n"
                  "  handoff conformance. TODO-4359 was split into TODO-4555, TODO-4556, and\n"
                  "  TODO-4557 so compile-time VM conformance could be covered in parallel by\n"
                  "  predicate/value, effects/cache/budget, and compiler-host boundary leaves.\n"
                  "  TODO-4349 was split into TODO-4558, TODO-4559, and TODO-4560 so the broader\n"
                  "  conformance matrix could proceed in parser/source-lock, semantic/IR, and\n"
                  "  compile-run/diagnostic tracks; all three split leaves are complete.") ==
        std::string::npos);
  CHECK(todo.find("### Ready Now (Parallel-Candidate Leaves; No Unmet TODO Dependencies)\n\n"
                  "- none") == std::string::npos);
  CHECK(todo.find("### Immediate Next 10 (Track Successors; Not Ready Until Dependencies Land)\n\n"
                  "- none") == std::string::npos);
  CHECK(todo.find("- TODO-4273: Add heterogeneous value-pack inference") ==
        std::string::npos);
  CHECK(todo.find("- [~] TODO-4305: Rename and style canonical `.prime` SoA surface") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4305: Rename and style canonical `.prime` SoA surface") !=
        std::string::npos);
  CHECK(todo.find("- Semantic phase contract hardening:") == std::string::npos);
  CHECK(todo.find("- Deferred graph and inference hardening: TODO-4239") ==
        std::string::npos);
  CHECK(todo.find("- Deferred semantic-product/backend/tooling follow-ups: TODO-4245") ==
        std::string::npos);
  CHECK(todo.find("Generic contiguous-storage coverage needed before vector ordinary `.prime`") ==
        std::string::npos);
  CHECK(todo.find("- Deferred SoA finish: TODO-4252") ==
        std::string::npos);
  CHECK(todo.find("### Execution Queue (Recommended Track Order)\n\n"
                  "- none") == std::string::npos);
  CHECK(todo.find("- TODO-4563: Add single-task spawn/wait runtime execution | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4545: Implement first structured task spawn/wait substrate") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4545: Implement first structured task spawn/wait substrate") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4558: Add generic parser and source-lock conformance") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4559: Add generic semantic and IR conformance") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4560: Add generic compile-run diagnostics conformance") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4558: Add generic parser and source-lock conformance") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4559: Add generic semantic and IR conformance") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4560: Add generic compile-run diagnostics conformance") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4548: Add expression-position `ct_if` values") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4548: Add expression-position `ct_if` values") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4549: Scope branch-local generated type facts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4549: Scope branch-local generated type facts") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4346: Add compile-time flow effect and termination policy") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4346: Add compile-time flow effect and termination policy") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4547: Add specialization-aware `ct_if` over type facts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4547: Add specialization-aware `ct_if` over type facts") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4345: Add compile-time `if` over type facts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4345: Add compile-time `if` over type facts") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4357: Evaluate pure user predicates at compile time") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4357: Evaluate pure user predicates at compile time") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4356: Add restricted compile-time callable lowering") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4356: Add restricted compile-time callable lowering") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4355: Add compile-time host and meta intrinsics") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4355: Add compile-time host and meta intrinsics") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4354: Factor reusable VM interpreter kernel") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4354: Factor reusable VM interpreter kernel") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4353: Add typed compile-time value model") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4353: Add typed compile-time value model") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4352: Add compile-time VM facade and host") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4352: Add compile-time VM facade and host") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4344: Add capability and trait support predicates") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4344: Add capability and trait support predicates") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4343: Add builtin type relation predicates") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4343: Add builtin type relation predicates") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4342: Represent requirement predicates as semantic facts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4342: Represent requirement predicates as semantic facts") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4331: Implement compile-time argument channel model") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4331: Implement compile-time argument channel model") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4276: Expand type packs in helpers and lifecycle hooks") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4276: Expand type packs in helpers and lifecycle hooks") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4277: Add tuple destructuring sugar") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4273: Add heterogeneous value-pack inference") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4272: Add stdlib `tuple<Ts...>`") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4271: Add compile-time pack indexing") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4275: Expand type packs into struct storage") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4270: Add compile-time integer template arguments") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4269: Bind and monomorphize type-pack arguments") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4268: Add heterogeneous type-pack syntax and metadata") !=
        std::string::npos);
  CHECK(todo.find("TODO-4518: Migrate SoA compatibility fixtures") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4518: Migrate SoA compatibility fixtures") !=
        std::string::npos);
  CHECK(todo.find("TODO-4519: Delete `soa_vector` compatibility seams") ==
        std::string::npos);
  CHECK(todo.find("TODO-4521: Delete semantic SoA public-surface traces") ==
        std::string::npos);
  CHECK(todo.find("TODO-4522: Delete lowerer and emitter SoA public-surface traces") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4521: Delete semantic SoA public-surface traces") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4522: Delete lowerer and emitter SoA public-surface traces") !=
        std::string::npos);
  CHECK(todo.find("- [~] TODO-4524: Tighten SoA surface audit to zero") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4524: Tighten SoA surface audit to zero") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4525: Delete text-filter and IR-printer SoA zero-audit residue") !=
        std::string::npos);
  CHECK(todo.find("TODO-4527: Delete template-monomorph SoA zero-audit residue") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4527: Delete template-monomorph SoA zero-audit residue") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4533: Remove lowerer call-resolution SoA bridge traces") !=
        std::string::npos);
  CHECK(todo.find("TODO-4528: Reduce lowerer count-access SoA trace residue") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4528: Reduce lowerer count-access SoA trace residue") !=
        std::string::npos);
  CHECK(todo.find("TODO-4529: Replace SoA inventory with strict zero audit") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4529: Replace SoA inventory with strict zero audit") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4519: Delete `soa_vector` compatibility seams") !=
        std::string::npos);
}

TEST_CASE("todo queue and skipped doctest debt stay source locked: execution queue and skip debt summary") {
  const DocsLocksTodoQueueFixture fixture = loadDocsLocksTodoQueueFixture();
  const std::string &todo = fixture.todo;
  const std::string &todoFinished = fixture.todoFinished;
  const std::string &vmMath = fixture.vmMath;
  const std::string &vmMaps = fixture.vmMaps;
  const std::string &examplesDocs = fixture.examplesDocs;

  const std::vector<std::string> completedSemanticPhaseQueue = {
      "TODO-4351: Add value-level compile-time requirement facts",
      "TODO-4347: Integrate requirements with overload selection",
      "TODO-4358: Enforce phase-qualified compile-time effects",
      "TODO-4550: Enforce compile-time evaluation budgets",
      "TODO-4551: Add compile-time evaluation cache keys",
  };
  for (const std::string &entry : completedSemanticPhaseQueue) {
    CHECK(todo.find("- " + entry) == std::string::npos);
    CHECK(todo.find("- [ ] " + entry) == std::string::npos);
    CHECK(todoFinished.find(entry) != std::string::npos);
  }
  CHECK(todo.find("- [ ] TODO-4560: Add generic compile-run diagnostics conformance") ==
        std::string::npos);
  CHECK(todoFinished.find(
            "TODO-4560: Add generic compile-run diagnostics conformance") !=
        std::string::npos);
  CHECK(todo.find("| track: procedural-genericity-docs |") ==
        std::string::npos);
  CHECK(todoFinished.find("  - parallel_track: procedural-genericity-docs") !=
        std::string::npos);
  CHECK(todo.find("| track: procedural-genericity-negatives |") ==
        std::string::npos);
  CHECK(todo.find("TODO-4340: Add procedural generic docs and examples") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4340: Add procedural generic docs and examples") !=
        std::string::npos);
  CHECK(todo.find("TODO-4546: Add procedural generic negative conformance") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4546: Add procedural generic negative conformance") !=
        std::string::npos);
  CHECK(todo.find("TODO-4337: Add local generated nominal structs") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4337: Add local generated nominal structs") !=
        std::string::npos);
  CHECK(todo.find("TODO-4338: Stabilize generated type identity and mangling") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4338: Stabilize generated type identity and mangling") !=
        std::string::npos);
  CHECK(todo.find("TODO-4339: Lower procedural generic facts through semantics") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4339: Lower procedural generic facts through semantics") !=
        std::string::npos);
  CHECK(todo.find("TODO-4546: Add procedural generic negative conformance") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4546: Add procedural generic negative conformance") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4332: Add bare zero-arg execution syntax") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4333: Reject ambiguous value/execution names") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4334: Add compile-time `[type]` local bindings") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4335: Add `typeof<symbol>` compile-time primitive") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4336: Allow type locals in envelope positions") !=
        std::string::npos);
  CHECK(todo.find("TODO-4341: Define generic requirement predicate surface") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4341: Define generic requirement predicate surface") !=
        std::string::npos);
  CHECK(todo.find("TODO-4561: Add task spawn/wait parser and effect locks") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4561: Add task spawn/wait parser and effect locks") !=
        std::string::npos);
  CHECK(todo.find("TODO-4562: Add task handle semantic facts and lifetime diagnostics") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4562: Add task handle semantic facts and lifetime diagnostics") !=
        std::string::npos);
  CHECK(todo.find("TODO-4563: Add single-task spawn/wait runtime execution") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4563: Add single-task spawn/wait runtime execution") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4277, TODO-4563") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4278: Integrate multi-wait with stdlib tuple") !=
        std::string::npos);
  CHECK(todoFinished.find("  - depends_on: TODO-4277, TODO-4563") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4545: Implement first structured task spawn/wait substrate") !=
        std::string::npos);
  CHECK(todoFinished.find("  - depends_on: TODO-4342, TODO-4343") !=
        std::string::npos);
  CHECK(todoFinished.find("  - depends_on: TODO-4342, TODO-4335") !=
        std::string::npos);
  CHECK(todoFinished.find("  - parallel_track: soa-zero-audit") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4226") == std::string::npos);
  CHECK(todo.find("TODO-4227") == std::string::npos);
  CHECK(todo.find("TODO-4215") == std::string::npos);
  CHECK(todo.find("TODO-4214") == std::string::npos);
  CHECK(todoFinished.find("TODO-4215: Make semantic-product publication consume merged fact bundles") !=
        std::string::npos);
  CHECK(todo.find("TODO-4263: Design generic and unit sum variants") == std::string::npos);
  CHECK(todo.find("TODO-4288:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4288") == std::string::npos);
  CHECK(todoFinished.find("TODO-4288: Add executable unit sum variants") !=
        std::string::npos);
  CHECK(todo.find("TODO-4289: Add generic sum declarations and monomorphization") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4289: Add generic sum declarations and monomorphization") !=
        std::string::npos);
  CHECK(todo.find("TODO-4264: Add stdlib-owned `Maybe<T>` sum") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4264: Add stdlib-owned `Maybe<T>` sum") !=
        std::string::npos);
  CHECK(todo.find("TODO-4265: Add stdlib-owned `Result<T, E>` sum") ==
        std::string::npos);
  CHECK(todo.find("TODO-4267: Retire legacy Maybe/Result representations") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4267: Retire legacy Maybe/Result representations") !=
        std::string::npos);
  CHECK(todo.find("TODO-4363: Retire Result helper legacy wording") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4363: Retire Result helper legacy wording") !=
        std::string::npos);
  CHECK(todo.find("TODO-4364: Fence Result compatibility helper adapters") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4364: Fence Result compatibility helper adapters") !=
        std::string::npos);
  CHECK(todo.find("TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4292: Promote and style canonical `.prime` vector implementation") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4292: Split canonical vector promotion umbrella") !=
        std::string::npos);
  CHECK(todo.find("TODO-4365: Style canonical vector constructor wrappers") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4365: Style canonical vector constructor wrappers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4366: Route vector wrappers through canonical helpers") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4366: Route vector wrappers through canonical helpers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4294: Lower vector helpers through ordinary `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4294: Split vector ordinary `.prime` lowering") !=
        std::string::npos);
  CHECK(todo.find("TODO-4368: Route canonical vector mutator statements through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4368: Route canonical vector mutator statements through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4369: Route canonical vector read helpers through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4369: Route canonical vector read helpers through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4370: Route vector compatibility mutators through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4370: Route vector compatibility mutators through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4371: Split hard-coded vector layout lowering") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4371: Split hard-coded vector layout lowering") !=
        std::string::npos);
  CHECK(todo.find("TODO-4372: Route vector setter statements through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4372: Route vector setter statements through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4373: Route vector metadata inline helpers through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4373: Route vector metadata inline helpers through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4374: Route vector metadata expression fallbacks through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4374: Route vector metadata expression fallbacks through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4375: Replace vector constructor header materialization") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4375: Replace vector constructor header materialization") !=
        std::string::npos);
  CHECK(todo.find("TODO-4295: Move collection surface metadata out of C++") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4295: Move collection surface metadata out of C++") !=
        std::string::npos);
  CHECK(todo.find("TODO-4296: Stop publishing vector compatibility metadata") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4296: Stop publishing vector compatibility metadata") !=
        std::string::npos);
  CHECK(todo.find("TODO-4376: Delete vector wrapper helper aliases") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4376: Delete vector wrapper helper aliases") !=
        std::string::npos);
  CHECK(todo.find("TODO-4377: Reject rooted vector helper aliases") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4377: Reject rooted vector helper aliases") !=
        std::string::npos);
  CHECK(todo.find("TODO-4293: Bridge legacy `Result` helpers to the result sum") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4293: Bridge legacy `Result` helpers to the result sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4299: Bridge `Result.map2` to the result sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4298: Bridge `Result.and_then` to the result sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4297: Bridge `Result.map` to the result sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4296: Bridge `Result.ok` to the result sum variant") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4295: Bridge `Result.why` to the result sum payload") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4294: Bridge `Result.error` to the result sum tag") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4292: Add importable stdlib `Result<T, E>` sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4287: Add unit sum declaration metadata") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4227: Move semantic-product fact families into worker bundles") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4214: Introduce deterministic worker result bundles") !=
        std::string::npos);
  CHECK(todo.find("TODO-4282: Reject call-shaped struct field construction") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4282: Reject call-shaped struct field construction") !=
        std::string::npos);
  CHECK(todo.find("TODO-4254: Migrate generated construction surfaces") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4254: Migrate generated construction surfaces") !=
        std::string::npos);
  CHECK(todo.find("TODO-4255: Migrate collection construction surfaces") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4255: Migrate collection construction surfaces") !=
        std::string::npos);
  CHECK(todo.find("TODO-4256: Classify constructor-shaped helper compatibility") ==
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4256") == std::string::npos);
  CHECK(todoFinished.find("TODO-4256: Classify constructor-shaped helper compatibility") !=
        std::string::npos);
  CHECK(todo.find("TODO-4257: Add sum declaration metadata and layout") ==
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4257") == std::string::npos);
  CHECK(todoFinished.find("TODO-4257: Add sum declaration metadata and layout") !=
        std::string::npos);
  CHECK(todo.find("TODO-4258: Add explicit sum construction") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4258: Add explicit sum construction") !=
        std::string::npos);
  CHECK(todo.find("TODO-4259: Add inferred sum variant construction") ==
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4259") == std::string::npos);
  CHECK(todoFinished.find("TODO-4259: Add inferred sum variant construction") !=
        std::string::npos);
  CHECK(todo.find("TODO-4260: Add `pick` semantic validation") ==
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4260") == std::string::npos);
  CHECK(todoFinished.find("TODO-4260: Add `pick` semantic validation") !=
        std::string::npos);
  CHECK(todo.find("TODO-4262:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4262") == std::string::npos);
  CHECK(todoFinished.find("TODO-4262: Add public sum-type examples") !=
        std::string::npos);
  CHECK(todo.find("TODO-4284:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4284") == std::string::npos);
  CHECK(todoFinished.find("TODO-4284: Stabilize aggregate sum pick result copies") !=
        std::string::npos);
  CHECK(todo.find("TODO-4285:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4285") == std::string::npos);
  CHECK(todoFinished.find("TODO-4285: Route sum moves through active payload helpers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4286:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4286") == std::string::npos);
  CHECK(todoFinished.find("TODO-4286: Route sum drop through active payload destroy helpers") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4227") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4215") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4228") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4216") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4217") == std::string::npos);
  CHECK(todo.find("TODO-4217") == std::string::npos);
  CHECK(todoFinished.find("TODO-4217: Move stdlib compatibility rewrites behind surface adapters") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4224") == std::string::npos);
  CHECK(todo.find("TODO-4224") == std::string::npos);
  CHECK(todoFinished.find("TODO-4224: Cut over vector/map compatibility decisions to surface adapters") !=
        std::string::npos);
  CHECK(todo.find("TODO-4229") == std::string::npos);
  CHECK(todoFinished.find("TODO-4229: Cut over SoA compatibility decisions to surface adapters") !=
        std::string::npos);
  CHECK(todo.find("TODO-4230") == std::string::npos);
  CHECK(todoFinished.find("TODO-4230: Cut over gfx compatibility decisions to surface adapters") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4218") == std::string::npos);
  CHECK(todo.find("TODO-4218") == std::string::npos);
  CHECK(todoFinished.find("TODO-4218: Make local-auto graph facts the exclusive inference authority") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4231") == std::string::npos);
  CHECK(todo.find("TODO-4231") == std::string::npos);
  CHECK(todoFinished.find("TODO-4231: Make query/try/on_error graph facts the exclusive authority") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4219") == std::string::npos);
  CHECK(todo.find("TODO-4219") == std::string::npos);
  CHECK(todoFinished.find("TODO-4219: Fail closed on residual lowerer AST semantic fallbacks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4225") == std::string::npos);
  CHECK(todo.find("TODO-4225") == std::string::npos);
  CHECK(todoFinished.find("TODO-4225: Close call-target and helper-routing lowerer fallbacks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4232") == std::string::npos);
  CHECK(todo.find("TODO-4232") == std::string::npos);
  CHECK(todoFinished.find("TODO-4232: Close binding/type/effect/layout lowerer fallbacks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4233") == std::string::npos);
  CHECK(todo.find("TODO-4233") == std::string::npos);
  CHECK(todoFinished.find("TODO-4233: Close backend-adapter and source-composition fallbacks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4220") == std::string::npos);
  CHECK(todo.find("TODO-4220") == std::string::npos);
  CHECK(todoFinished.find("TODO-4220: Add semantic phase handoff conformance gates") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4234") == std::string::npos);
  CHECK(todo.find("TODO-4234") == std::string::npos);
  CHECK(todoFinished.find("TODO-4234: Add semantic budget and worker-parity release gates") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4221") == std::string::npos);
  CHECK(todo.find("TODO-4221") == std::string::npos);
  CHECK(todoFinished.find("TODO-4221: Retire stale semantic validator source locks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4235") == std::string::npos);
  CHECK(todo.find("TODO-4235") == std::string::npos);
  CHECK(todoFinished.find("TODO-4235: Retire remaining semantic/lowerer architecture source locks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4236") == std::string::npos);
  CHECK(todo.find("TODO-4236") == std::string::npos);
  CHECK(todoFinished.find("TODO-4236: Define graph invalidation contracts by edit family") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4237") == std::string::npos);
  CHECK(todo.find("TODO-4237") == std::string::npos);
  CHECK(todoFinished.find("TODO-4237: Add graph invalidation fan-out regression tests") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4238") == std::string::npos);
  CHECK(todo.find("TODO-4238") == std::string::npos);
  CHECK(todoFinished.find("TODO-4238: Pin the CT-eval graph and semantic-product boundary") !=
        std::string::npos);
  CHECK(todo.find("TODO-4239") == std::string::npos);
  CHECK(todoFinished.find("TODO-4239: Migrate helper-routing template inference onto graph facts") !=
        std::string::npos);
  CHECK(todo.find("TODO-4216") == std::string::npos);
  CHECK(todoFinished.find("TODO-4216: Split semantic rewrites into an explicit pass manifest") !=
        std::string::npos);
  CHECK(todo.find("TODO-4228") == std::string::npos);
  CHECK(todoFinished.find("TODO-4228: Factor and version the semantic-product boundary API") !=
        std::string::npos);
  CHECK(todo.find("TODO-4240") == std::string::npos);
  CHECK(todoFinished.find("TODO-4240: Add backend semantic-product conformance coverage") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4240") == std::string::npos);
  CHECK(todo.find("TODO-4241") == std::string::npos);
  CHECK(todoFinished.find("TODO-4241: Retire semantic-product output compatibility callers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4279") == std::string::npos);
  CHECK(todoFinished.find("TODO-4279: Retire compile-pipeline helper compatibility callers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4242") == std::string::npos);
  CHECK(todoFinished.find("TODO-4242: Inventory repo-wide source-lock replacement candidates") !=
        std::string::npos);
  CHECK(todo.find("TODO-4243") == std::string::npos);
  CHECK(todoFinished.find("TODO-4243: Improve focused backend rerun ergonomics") !=
        std::string::npos);
  CHECK(todo.find("TODO-4244") == std::string::npos);
  CHECK(todoFinished.find("TODO-4244: Decide the `soa_vector` maturity exit") !=
        std::string::npos);
  CHECK(todo.find("TODO-4245") == std::string::npos);
  CHECK(todoFinished.find("TODO-4245: Plan dynamic vector growth and runtime storage support") !=
        std::string::npos);
  CHECK(todo.find("TODO-4253") == std::string::npos);
  CHECK(todoFinished.find("TODO-4253: Add brace constructor argument-list path") !=
        std::string::npos);
  CHECK(todo.find("TODO-4246") == std::string::npos);
  CHECK(todoFinished.find("TODO-4246: Define final `soa_vector` promotion contract") !=
        std::string::npos);
  CHECK(todo.find("TODO-4247") == std::string::npos);
  CHECK(todoFinished.find("TODO-4247: Move canonical SoA wrapper off experimental implementation imports") !=
        std::string::npos);
  CHECK(todo.find("TODO-4248") == std::string::npos);
  CHECK(todoFinished.find("TODO-4248: Move canonical SoA conversions off experimental conversion imports") !=
        std::string::npos);
  CHECK(todo.find("TODO-4249") == std::string::npos);
  CHECK(todoFinished.find("TODO-4249: Retire direct experimental SoA public imports") !=
        std::string::npos);
  CHECK(todo.find("TODO-4250") == std::string::npos);
  CHECK(todoFinished.find("TODO-4250: Normalize raw builtin `soa_vector` bridges onto canonical wrappers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4251") == std::string::npos);
  CHECK(todoFinished.find("TODO-4251: Add full cross-backend SoA parity coverage") !=
        std::string::npos);
  CHECK(todo.find("TODO-4252") == std::string::npos);
  CHECK(todoFinished.find("TODO-4252: Promote `soa_vector` docs after compatibility cleanup") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4244") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4246") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4247") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4248") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4249") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4250") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4251") == std::string::npos);
  CHECK(todo.find("- TODO-4162") == std::string::npos);
  CHECK(todo.find("- TODO-4165") == std::string::npos);
  CHECK(todo.find("- TODO-4159") == std::string::npos);
  CHECK(todo.find("- TODO-4160") == std::string::npos);
  CHECK(todo.find("- TODO-4161") == std::string::npos);
  CHECK(todo.find("- TODO-4163") == std::string::npos);
  CHECK(todo.find("- Skipped doctest debt: TODO-4107") ==
        std::string::npos);
  CHECK(todo.find("- Release-gate stability and test-suite audit follow-up:") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4158") == std::string::npos);
  CHECK(todo.find("- TODO-4169") == std::string::npos);
  CHECK(todo.find("- TODO-4167") == std::string::npos);
  CHECK(todo.find("- TODO-4152") == std::string::npos);
  CHECK(todo.find("- TODO-4151") == std::string::npos);
  CHECK(todo.find("- TODO-4147") == std::string::npos);
  CHECK(todo.find("TODO-4588 added the IR-preparation phase manifest") !=
        std::string::npos);
  CHECK(todo.find("TODO-4580: Replace private source-lock tests with public contracts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4580: Replace private source-lock tests with public contracts") !=
        std::string::npos);
  CHECK(todo.find("TODO-4588: Add pass/phase invalidation manifest beyond semantics") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4588: Add pass/phase invalidation manifest beyond semantics") !=
        std::string::npos);
  CHECK(todo.find("| Semantic ownership boundary and graph/local-auto authority | none |") ==
        std::string::npos);
  CHECK(todo.find("| Compile-pipeline stage and publication-boundary contracts | none |") ==
        std::string::npos);
  CHECK(todo.find("| Compile-time macro hooks and AST transform ownership | none |") ==
        std::string::npos);
  CHECK(todo.find("| Stdlib bridge consolidation and collection/file/gfx surface authority | none |") ==
        std::string::npos);
  CHECK(todo.find("| Vector/map stdlib ownership cutover and collection surface authority | none |") ==
        std::string::npos);
  CHECK(todo.find("| Release benchmark/example suite stability and doctest governance | none |") ==
        std::string::npos);
  CHECK(todo.find("| Focused backend rerun ergonomics and suite partitioning | none |") ==
        std::string::npos);
  CHECK(todo.find("| Test-suite audit follow-up and release-gate stability | none |") ==
        std::string::npos);
  CHECK(todo.find("| Stdlib de-experimentalization and public/internal namespace cleanup | none |") ==
        std::string::npos);
  CHECK(todo.find("| SoA maturity and `soa` public-surface rename | none |") ==
        std::string::npos);
  CHECK(todo.find("| De-experimentalization surface and namespace parity | none |") ==
        std::string::npos);
  CHECK(todo.find("| `soa` maturity and canonical surface parity | none |") ==
        std::string::npos);
  CHECK(todo.find("| Validator entrypoint and benchmark-plumbing split | none |") ==
        std::string::npos);
  CHECK(todo.find("| Semantic-product publication by module and fact family | none |") ==
        std::string::npos);
  CHECK(todo.find("| Semantic-product public API factoring and versioning | none |") ==
        std::string::npos);
  CHECK(todo.find("| IR lowerer compile-unit breakup | none |") ==
        std::string::npos);
  CHECK(todo.find("| Backend validation/build ergonomics | none |") ==
        std::string::npos);
  CHECK(todo.find("| Semantic-product-authority conformance | none |") ==
        std::string::npos);
  CHECK(todo.find("| AST transform hook conformance | none |") ==
        std::string::npos);
  CHECK(todo.find("| Compile-pipeline stage handoff conformance | none |") ==
        std::string::npos);
  CHECK(todo.find("| Architecture contract probe migration | none |") ==
        std::string::npos);
  CHECK(todo.find("| Semantic-product publication parity and deterministic ordering | none |") ==
        std::string::npos);
  CHECK(todo.find("| Lowerer/source-composition contract coverage | none |") ==
        std::string::npos);
  CHECK(todo.find("| Vector/map bridge parity for imports, rewrites, and lowering | none |") ==
        std::string::npos);
  CHECK(todo.find("### Skipped Doctest Debt Summary") == std::string::npos);
  CHECK(todo.find("Retained `doctest::skip(true)` coverage is currently absent from the active") ==
        std::string::npos);
  CHECK(todo.find("queue because no skipped doctest cases remain under `tests/unit`.") ==
        std::string::npos);
  CHECK(todo.find("New skipped doctest coverage must create a new explicit TODO before it lands.") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4146: Split or optimize slow serialization shards") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4145: Reconcile stale wrapper-helper audit expectations") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4117:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4118:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4110:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4106: Re-enable or prune skipped collection compatibility suites") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4123: Split validator core from publication and benchmark shadows") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4122: Publish indexed graph-backed facts for lowering") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4107: Re-enable or prune residual skipped IR, docs, and smoke suites") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4132: Add `==` support for reflected `Equal` helpers") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4133: Let `generate(...)` imply reflection enablement") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4134: Accept type-qualified dot-call syntax for static helpers") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4105:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4109:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4112:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4114:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4116:") == std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4142: Retire brittle architecture source locks after contract") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4123: Split validator core from publication and benchmark shadows.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4118: Re-enable or prune remaining VM non-i32 map key runtime skips.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4107: Re-enable or prune residual skipped IR, docs, and smoke suites.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4132: Add `==` support for reflected `Equal` helpers.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4122: Publish indexed graph-backed facts for lowering.") !=
        std::string::npos);
  CHECK(todoFinished.find("- [x] TODO-4158: Introduce a narrow lowerer syntax and provenance view") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4133: Let `generate(...)` imply reflection enablement.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4134: Accept type-qualified dot-call syntax for static helpers.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4117: Re-enable or prune remaining VM numeric map indexing sugar skip.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4110: Re-enable or prune remaining VM support-matrix math skips.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4116: Narrow remaining VM numeric-key map skip debt into explicit blocker-owned leaves.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4115: Prune stale skipped VM argv-key map indexing duplicate.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4113: Prune stale skipped VM string-key map duplicates.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4111: Prune stale skipped legacy VM map helper duplicates.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4108: Prune stale skipped VM scalar math helper coverage.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4104: Restore skipped doctest debt tracking in the active queue.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4106: Re-enable or prune skipped collection compatibility suites.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4197: Lock skipped doctest debt policy.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4212: Introduce semantic validation plan prepass.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4213: Route definition workers through the shared validation plan.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4222: Route execution validation through the shared plan.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4226: Add a structured semantic diagnostic/result sink.") !=
        std::string::npos);

  CHECK(vmMath.find("TEST_CASE(\"runs vm with qualified math names\")") != std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"rejects vm support-matrix math nominal helpers\")") !=
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm quaternion multiply and rotation helpers\")") !=
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm matrix composition order helpers\")") !=
        std::string::npos);
  CHECK(vmMath.find("doctest::skip(true)") == std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math abs/sign/min/max\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math saturate/lerp\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math clamp\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math pow\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math trig helpers\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math exp/log\" * doctest::skip(true))") ==
        std::string::npos);

  CHECK(vmMaps.find("TEST_CASE(\"rejects vm map indexing sugar without canonical helper\")") !=
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"rejects vm bool map access helpers without canonical helper\")") !=
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"rejects vm u64 map access helpers without canonical helper\")") !=
        std::string::npos);
  CHECK(vmMaps.find("doctest::skip(true)") == std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"rejects vm map indexing with argv key\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"runs vm with string-valued map literals\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"runs vm with string-keyed map literals\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"runs vm with string-keyed map binding lookup\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"runs vm with map at helper\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"vm map at rejects missing key\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"rejects vm map lookup with argv string key\" * doctest::skip(true))") ==
        std::string::npos);

  CHECK(examplesDocs.find("TEST_CASE(\"collection docs snippets stay code-examples style and executable\")") !=
        std::string::npos);
  CHECK(examplesDocs.find(
            "TEST_CASE(\"collection docs snippets stay code-examples style and executable\" * doctest::skip(true))") ==
        std::string::npos);
}


TEST_SUITE_END();
