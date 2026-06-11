# Test Inventory

Generated from `tests/unit/` on 2026-06-11.

Total: 10022 test cases across 459 files.

## ast (30 tests, 3 files)

### test_ast_ir_dump.cpp

- ast dump matches expected format
- ir dump matches expected format
- ast dump prints void return
- ir dump prints void return
- ast dump prints string literals
- ir dump prints string literals
- ast dump prints method call template args
- ir dump prints method call template args
- ast dump prints float literals
- ir dump prints float literals
- ast dump prints leading dot float literals
- ir dump prints leading dot float literals
- ir dump prints bool return type
- ir dump prints array return type
- ast dump prints transform arguments
- ast dump prints struct definition
- ir dump prints struct definition
- ir dump prints pod definition
- ast dump prints named arguments
- ir dump prints named arguments

### test_ast_ir_dump_inference.cpp

- ir dump infers return type without transform
- ir dump infers return type from builtin plus
- ir dump infers return type from builtin clamp
- ir dump prints transformed execution return inference
- ir dump prints local bindings before control flow
- ir dump prints nested block return inference
- ir dump infers return type from named helper call
- ir dump keeps inferred type on local return variable

### test_ast_ir_dump_namespace.cpp

- ast dump includes namespace paths
- ir dump includes namespace paths

## compile_run/bindings (12 tests, 1 files)

### test_compile_run_bindings_basic.cpp

- compiles and runs empty void main
- compiles and runs local binding
- compiles and runs bare zero-arg calls
- compiles and runs typeof type locals
- compiles and runs local generated struct
- emits stable IR names for specialized local generated struct
- compiles with struct definition
- compiles and runs assign to mutable binding
- compiles and runs assign to reference
- compiles and runs location on reference
- compiles and runs reference arithmetic
- compiles and runs array reference helpers

## compile_run/emitters (622 tests, 27 files)

### test_compile_run_emitters_canonical_map_helper_calls.cpp

- rejects canonical slash-method map access without helper on canonical at first in C++ emitter
- compiles and runs bare map count through canonical helper in C++ emitter
- rejects bare map count without imported canonical helper in C++ emitter
- rejects bare map count through compatibility alias when canonical helper is absent in C++ emitter
- compiles and runs bare map at through canonical helper in C++ emitter
- compiles and runs bare map at_unsafe through canonical helper in C++ emitter
- rejects bare map at call without helper in C++ emitter with unknown-target diagnostics
- rejects bare map at through compatibility alias in C++ emitter with unknown-target diagnostics
- rejects bare map at_unsafe call without helper in C++ emitter with unknown-target diagnostics
- rejects bare map at_unsafe through compatibility alias in C++ emitter with unknown-target diagnostics
- compiles and runs canonical vector at through imported stdlib helper in C++ emitter
- rejects bare vector at without imported helper in C++ emitter
- compiles and runs canonical vector at_unsafe through imported stdlib helper in C++ emitter
- rejects bare vector at_unsafe without imported helper in C++ emitter
- compiles and runs map unnamespaced contains through canonical helper in C++ emitter
- rejects map unnamespaced contains through compatibility helper when canonical helper is absent in C++ emitter
- compiles and runs map unnamespaced contains preferring canonical helper over compatibility alias in C++ emitter
- rejects map unnamespaced contains without helper in C++ emitter with unknown-target diagnostics
- rejects map unnamespaced tryAt through canonical helper in C++ emitter without on_error
- rejects map unnamespaced tryAt through compatibility helper in C++ emitter without on_error
- rejects map unnamespaced tryAt preferring canonical helper over compatibility alias in C++ emitter without on_error
- compiles and runs explicit map helper calls while canonical map access stays authoritative in C++ emitter
- runs explicit canonical map helper calls through same-path helpers in C++ emitter
- rejects bare map tryAt call without imported canonical helper in C++ emitter with unknown-target diagnostics
- rejects map tryAt compatibility call struct method chain canonical forwarding in C++ emitter
- compiles and runs same-path direct map tryAt struct method chain through alias helper in C++ emitter
- compiles and runs canonical direct map tryAt struct method chain in C++ emitter
- compiles same-path direct map at_unsafe struct method chain through alias helper in C++ emitter
- compiles canonical direct map at_unsafe struct method chain in C++ emitter
- compiles namespaced map count method through canonical helper in C++ emitter
- rejects bare map count method without imported canonical helper in C++ emitter
- C++ emitter rejects bare map contains method without imported canonical helper
- compiles and runs bare map contains struct method chain through canonical helper in C++ emitter
- rejects bare map contains struct method chain through alias helper in C++ emitter
- C++ emitter rejects bare map tryAt method without imported canonical helper
- compiles and runs bare map tryAt struct method chain through canonical helper in C++ emitter
- rejects bare map tryAt struct method chain through alias helper in C++ emitter
- C++ emitter rejects bare map access methods without imported canonical helpers
- uses canonical helper for namespaced map at method alias in C++ emitter
- C++ emitter runs extra-argument stdlib canonical map count method-call sugar

### test_compile_run_emitters_canonical_map_method_forwarding.cpp

- compiles canonical map slash-method unsafe struct helper receiver forwarding in C++ emitter
- rejects wrapper-returned map method alias primitive receiver fallback in C++ emitter
- rejects wrapper-returned canonical map slash-method struct helper receiver forwarding in C++ emitter
- rejects std-namespaced vector method alias access struct helper receiver mismatch in C++ emitter
- rejects std-namespaced vector method alias access receiver fallback without helper in C++ emitter
- rejects std-namespaced vector method alias access struct method chain with helper missing-method diagnostics in C++ emitter
- rejects std-namespaced vector unsafe method alias access receiver fallback without helper in C++ emitter
- C++ emitter forwards explicit-template vector count wrappers through canonical return kinds
- C++ emitter keeps canonical diagnostics for explicit-template vector count wrappers
- rejects namespaced access method chain non-collection target in C++ emitter
- rejects namespaced map capacity method chain target in C++ emitter
- C++ emitter keeps canonical direct-call vector capacity same-path helper on map receiver
- C++ emitter keeps alias direct-call vector capacity same-path helper on map receiver

### test_compile_run_emitters_compatibility_chain_forwarding_rejections.cpp

- rejects map access compatibility call struct method chain canonical diagnostics forwarding in C++ emitter
- rejects map unsafe compatibility call struct method chain canonical forwarding in C++ emitter
- rejects map unsafe compatibility call struct method chain primitive argument diagnostics in C++ emitter
- rejects vector alias access auto wrapper canonical struct-return forwarding in C++ emitter
- rejects vector alias access auto wrapper canonical diagnostics forwarding in C++ emitter
- C++ emitter rejects vector method alias access struct method forwarding
- rejects vector method alias access canonical-only helper routing with array receiver diagnostics in C++ emitter
- rejects vector method alias access struct method chain with Marker receiver diagnostics in C++ emitter
- runs vector method alias access field expression with struct receiver in C++ emitter
- C++ emitter rejects vector unsafe method alias access struct method forwarding
- rejects vector method alias access receiver fallback without helper in C++ emitter
- accepts vector unsafe method alias access field expression with struct receiver in C++ emitter
- rejects vector unsafe method alias access receiver fallback without helper in C++ emitter
- runs vector method alias struct-return precedence in C++ emitter
- rejects canonical vector method access struct forwarding in C++ emitter
- runs canonical vector unsafe method field forwarding in C++ emitter
- runs canonical map slash-method struct method chain forwarding in C++ emitter

### test_compile_run_emitters_core_behaviors.cpp

- compiles and runs array slice count and indexed access in C++ emitter
- C++ emitter serializes scene model source deterministically
- compiles and runs chained method calls in C++ emitter
- C++ emitter preserves explicit Result constructor payloads
- C++ emitter compiles Result.why on ok bridge values
- C++ emitter packs single-field error sum constructor payloads
- C++ emitter packs single-field Result.ok payloads
- C++ emitter packs single-field ok sum constructor payloads
- C++ emitter packs single-field Result.map payloads
- C++ emitter rejects experimental map custom comparable struct keys
- executions are ignored by C++ emitter
- struct Create/Destroy helpers do not trip C++ emitter stack underflow at runtime
- C++ emitter returns structs from functions
- C++ emitter uses struct field defaults
- C++ emitter supports file io
- C++ emitter supports file read_byte with deterministic eof
- C++ emitter supports custom Result.why hooks
- C++ emitter compiles nested Result combinators
- C++ emitter compiles Result.and_then direct Result.ok lambda
- C++ emitter supports image api contract deterministically
- C++ emitter runs software renderer command serialization deterministically
- C++ emitter runs software renderer clip stack serialization deterministically
- C++ emitter runs two-pass layout tree serialization deterministically
- C++ emitter runs two-pass layout empty root deterministically
- C++ emitter runs basic widget controls through layout deterministically
- C++ emitter runs panel container widget deterministically
- C++ emitter runs empty panel container stays balanced deterministically
- C++ emitter runs composite login form deterministically
- C++ emitter runs html adapter login form deterministically
- C++ emitter runs ui event stream deterministically
- C++ emitter runs ui ime event stream deterministically
- C++ emitter runs ui resize and focus event stream deterministically
- C++ emitter supports graphics-style int return propagation with on_error
- C++ emitter renders static fields and visibility
- C++ emitter uses copy to force by-value params
- C++ emitter renders lambda captures
- C++ emitter preserves explicit lambda captures

### test_compile_run_emitters_explicit_vector_count_capacity_helpers.cpp

- rejects wrapper explicit vector capacity calls without helper in C++ emitter
- compiles and runs wrapper explicit vector count capacity aliases through same-path helpers in C++ emitter
- compiles and runs local explicit vector count capacity calls through same-path helpers in C++ emitter
- C++ emitter rejects local explicit vector count capacity calls without helper before emission
- rejects local explicit vector count capacity calls without helper in C++ emitter
- rejects namespaced wrapper vector capacity vector target without helper in C++ emitter
- rejects wrapper bare vector capacity imported override as duplicate definition in C++ emitter
- C++ emitter rejects wrapper bare vector capacity calls without helper before emission
- rejects wrapper bare vector capacity calls without helper in C++ emitter
- C++ emitter compiles bare vector capacity methods without helper
- runs bare vector capacity methods without helper in C++ emitter
- rejects wrapper vector capacity methods without helper in C++ emitter
- rejects wrapper vector capacity methods without helper before emission in C++ emitter
- C++ emitter rejects wrapper vector capacity slash-method chains before receiver typing
- C++ emitter rejects duplicate local canonical slash-method vector capacity overloads
- C++ emitter rejects local canonical slash-method vector capacity on map receiver before emission
- rejects local canonical slash-method vector capacity on string receiver in C++ emitter
- rejects local alias slash-method vector capacity on string receiver in C++ emitter
- rejects local alias slash-method vector capacity on array receiver in C++ emitter
- compiles and runs vector alias count capacity slash methods in C++ emitter
- compiles and runs stdlib namespaced vector count capacity slash methods in C++ emitter
- C++ emitter rejects vector namespaced count capacity slash methods without same-path helper
- C++ emitter rejects stdlib namespaced vector count capacity slash methods without same-path helper
- rejects vector namespaced count capacity slash methods without same-path helper in C++ emitter
- rejects stdlib namespaced vector count capacity slash methods without same-path helper in C++ emitter
- C++ emitter rejects cross-path vector count capacity slash methods before builtin fallback
- rejects cross-path vector count capacity slash helper routing in C++ emitter
- compiles and runs stdlib namespaced vector access slash methods in C++ emitter
- C++ emitter rejects stdlib namespaced vector access slash methods without helper before lowering
- rejects stdlib namespaced vector access slash methods without helper in C++ emitter

### test_compile_run_emitters_explicit_vector_mutator_statement_helpers.cpp

- C++ emitter rejects canonical vector mutator methods with alias-only helper before emission
- rejects explicit canonical vector mutator statement helper without vector access helper in C++ emitter
- rejects reordered explicit canonical vector mutator statement helper without vector access helper in C++ emitter
- C++ emitter rejects alias vector mutator statements with canonical-only helper before emission
- C++ emitter rejects alias reordered vector mutator statements with canonical-only helper before emission
- rejects alias vector mutator statements with canonical-only helper in C++ emitter
- C++ emitter rejects canonical vector mutator statements with alias-only helper before emission
- C++ emitter rejects canonical reordered vector mutator statements with alias-only helper before emission
- rejects canonical vector mutator statements with alias-only helper in C++ emitter
- rejects explicit canonical vector remove methods without imported helper in C++ emitter
- C++ emitter infers wrapper access builtin fallback
- rejects inferred wrapper access key mismatch in C++ emitter
- C++ emitter infers wrapper string access builtin fallback
- C++ emitter infers wrapper string count builtin fallback
- rejects builtin count on canonical map reference string access without helper in C++ emitter
- C++ emitter rejects bare builtin count on wrapper-returned canonical map access before lowering
- C++ emitter keeps canonical map unknown-target diagnostics on wrapper-returned map indexing
- C++ emitter keeps imported wrapper-returned canonical map reference alias diagnostics
- C++ emitter rejects direct wrapper-returned canonical map access count shadow
- C++ emitter rejects wrapper-returned canonical map method access string receiver typing
- C++ emitter keeps canonical map unknown-target diagnostics on direct-call wrapper-returned canonical map reference access

### test_compile_run_emitters_lambda_mutator_resolution.cpp

- C++ emitter lambda mutators honor user vector helpers
- C++ emitter lambda mutator positional call resolves user helper
- rejects lambda std namespaced reordered mutator compatibility helper in C++ emitter
- C++ emitter lambda mutator bool positional call resolves user helper
- C++ emitter lambda mutator named call prefers values receiver
- C++ emitter lambda mutator rewrite keeps known vector receiver leading names
- C++ emitter rejects lambda explicit vector mutator statements without helper before emission
- C++ emitter rejects lambda cross-path explicit vector mutator statements before emission
- C++ emitter rejects lambda reordered cross-path explicit vector mutator statements before emission
- C++ emitter rejects lambda explicit vector mutator methods without helper before emission
- C++ emitter rejects lambda cross-path explicit vector mutator methods before emission
- C++ emitter lambda mutator mismatch rejects user helper signatures
- C++ emitter lambda mutator mismatch rejects call-form helper
- compiles and runs import alias in C++ emitter
- compiles and runs array method calls in C++ emitter
- compiles and runs array index sugar
- compiles and runs argv helpers in C++ emitter
- compiles and runs array index sugar with u64
- compiles and runs vector helpers in C++ emitter
- compiles and runs canonical vector mutators over imported user shadow helpers in C++ emitter
- C++ emitter statement mutator call-form rejects shadow helper
- compiles and runs canonical vector mutator named calls over imported user shadow helpers in C++ emitter
- C++ emitter statement mutator named call rejects shadow helper without import
- rejects imported user vector mutator positional call shadow in C++ emitter

### test_compile_run_emitters_local_vector_count_receiver_resolution.cpp

- rejects local canonical slash-method vector count on string receiver in C++ emitter
- C++ emitter keeps local canonical slash-method vector count same-path helper on string receiver
- C++ emitter keeps local canonical slash-method vector count same-path helper on map receiver
- C++ emitter keeps local canonical slash-method vector count same-path helper on array receiver
- rejects local alias slash-method vector count on string receiver in C++ emitter
- rejects local alias slash-method vector count on array receiver in C++ emitter
- C++ emitter keeps rooted vector count same-path helper on string receiver
- C++ emitter rejects local alias slash-method vector count same-path helper on map receiver
- C++ emitter keeps rooted vector count same-path helper on array receiver
- C++ emitter keeps rooted wrapper vector count same-path helper on string receiver
- C++ emitter rejects alias slash-method vector count on string receiver before emission
- rejects alias slash-method vector count on string receiver in C++ emitter
- C++ emitter rejects alias slash-method vector count same-path helper on map receiver
- C++ emitter rejects canonical slash-method vector count same-path helper on map receiver
- rejects wrapper-returned canonical vector capacity slash-method on map receiver in C++ emitter
- C++ emitter rejects alias slash-method vector count on map receiver before emission
- rejects alias slash-method vector count on map receiver in C++ emitter
- C++ emitter keeps rooted wrapper vector count same-path helper on array receiver
- C++ emitter keeps canonical slash-method vector count same-path helper on array receiver
- C++ emitter keeps canonical slash-method vector count same-path helper on string receiver
- rejects wrapper-returned canonical vector capacity slash-method on array receiver in C++ emitter
- C++ emitter rejects alias slash-method vector count on array receiver with rooted target before emission
- rejects alias slash-method vector count on array receiver with rooted target in C++ emitter
- C++ emitter keeps canonical direct-call vector count same-path helper on string receiver
- C++ emitter rejects canonical direct-call vector count on string receiver before emission
- rejects canonical direct-call vector count on string receiver in C++ emitter
- C++ emitter keeps alias direct-call vector count same-path helper on string receiver
- C++ emitter rejects alias direct-call vector count on string receiver before emission
- rejects alias direct-call vector count on string receiver in C++ emitter
- C++ emitter keeps alias direct-call vector count same-path helper on array receiver
- C++ emitter rejects alias direct-call vector count on array receiver before emission
- rejects alias direct-call vector count on array receiver in C++ emitter
- C++ emitter keeps canonical direct-call vector count same-path helper on array receiver
- C++ emitter rejects canonical direct-call vector count on array receiver before emission
- rejects canonical direct-call vector count on array receiver in C++ emitter
- rejects namespaced access method chain compatibility fallback in C++ emitter
- rejects namespaced wrapper string access method chain compatibility fallback in C++ emitter

### test_compile_run_emitters_loop_sugar_runtime.cpp

- compiles and runs repeat loop
- compiles and runs loop while for sugar
- compiles and runs for binding condition
- compiles and runs shared_scope loops
- compiles and runs shared_scope for binding condition
- compiles and runs shared_scope while loop
- compiles and runs increment decrement sugar
- compiles and runs brace constructor value
- compiles and runs nested definition call
- compiles and runs paired map constructor
- C++ emitter runs variadic args body access and indexed method calls
- C++ emitter forwards variadic args packs with explicit prefix values
- C++ emitter materializes variadic Result value packs with spread forwarding
- C++ emitter materializes variadic status-only Result value packs with spread forwarding
- C++ emitter materializes variadic borrowed Result value packs with indexed dereference helpers
- C++ emitter materializes variadic borrowed status-only Result packs with indexed dereference helpers
- C++ emitter materializes variadic FileError value packs with indexed why methods
- C++ emitter rejects retired variadic map value pack count methods
- C++ emitter materializes variadic Buffer value packs with indexed count helpers
- C++ emitter materializes variadic File handle packs with indexed file methods
- C++ emitter materializes variadic borrowed FileError packs with indexed dereference why methods

### test_compile_run_emitters_map_access_and_collection_rewrites.cpp

- rejects user map access unsafe string positional call shadow in C++ emitter
- C++ emitter rejects canonical map access positional reorder
- C++ emitter keeps canonical map access key diagnostics on positional reorder
- C++ emitter access rewrite keeps known collection receiver leading names
- rejects user vector mutator shadow arg mismatch in C++ emitter
- rejects user vector mutator call-form arg mismatch in C++ emitter
- compiles and runs stdlib namespaced vector helpers in C++ emitter
- rejects array namespaced vector constructor alias in C++ emitter
- rejects array namespaced vector at alias in C++ emitter
- rejects array namespaced vector at_unsafe alias in C++ emitter
- rejects wrapper array namespaced vector at alias in C++ emitter
- rejects wrapper array namespaced vector at_unsafe alias in C++ emitter
- rejects array namespaced vector count builtin alias in C++ emitter
- rejects array namespaced vector count method alias in C++ emitter
- rejects array namespaced vector capacity alias in C++ emitter
- rejects array namespaced vector capacity method alias in C++ emitter
- rejects array namespaced vector mutator alias in C++ emitter
- C++ emitter helper rejects full-path array mutator aliases
- C++ emitter helper rejects rooted vector mutator aliases
- C++ emitter helper accepts canonical vector mutators without alias bridge
- C++ emitter helper rejects array namespaced vector constructor alias builtin
- C++ emitter helper rejects rooted map constructor alias builtin
- C++ emitter helper rejects bare vector count methods without helper metadata
- C++ emitter helper resolves bare vector count methods when helper metadata exists
- C++ emitter helper rejects bare vector capacity methods without helper metadata
- C++ emitter helper resolves bare vector capacity methods when helper metadata exists
- C++ emitter helper keeps explicit stdlib vector count and capacity methods canonical
- C++ emitter helper prefers stdlib File flush helper when present
- C++ emitter helper prefers stdlib File close helper when present
- C++ emitter helper prefers stdlib File multi-value helpers when present

### test_compile_run_emitters_map_count_and_wrapper_capacity.cpp

- C++ emitter keeps canonical diagnostics on direct-call map count receivers
- rejects stdlib namespaced vector capacity on map target in C++ emitter
- C++ emitter rejects user wrapper count/capacity shadow precedence on map count first
- rejects user wrapper temporary count capacity shadow value mismatch in C++ emitter
- C++ emitter rejects wrapper count/capacity builtin fallback on map count first
- rejects namespaced wrapper vector count map target in C++ emitter
- rejects namespaced wrapper vector count vector target without helper in C++ emitter
- C++ emitter rejects mixed array-count wrapper fallback on array count first
- C++ emitter keeps canonical direct-call vector count same-path helper on map receiver
- C++ emitter keeps alias direct-call vector count same-path helper on map receiver
- C++ emitter keeps vector-count diagnostic for alias direct-call vector count on map receiver
- C++ emitter rejects array namespaced wrapper capacity builtin fallback
- rejects namespaced count/capacity method chain fallback on vector count first in C++ emitter
- C++ emitter keeps string-count diagnostic for slash-method vector count on string receiver
- C++ emitter keeps map receiver method diagnostic for slash-method vector count on map receiver
- C++ emitter keeps array-count diagnostic for slash-method vector count on array receiver
- C++ emitter rejects duplicate local canonical slash-method vector count overloads
- C++ emitter rejects local canonical slash-method vector count on map receiver before emission

### test_compile_run_emitters_map_metadata_resolution.cpp

- C++ emitter helper resolves direct slashless canonical map return metadata
- C++ emitter helper resolves parser-shaped canonical map return metadata
- C++ emitter helper keeps canonical map access method unresolved without metadata
- C++ emitter helper rejects removed full-path map method aliases
- C++ emitter helper rejects full-path map aliases without receiver type
- C++ emitter helper normalizes slashless map type import alias method targets
- C++ emitter helper prefers canonical map method sugar over compatibility aliases
- C++ emitter helper rejects canonical metadata fallback for explicit map slash methods
- C++ emitter helper rejects compatibility metadata fallback for canonical map slash methods
- C++ emitter helper rejects same-path map contains and tryAt slash-method metadata
- C++ emitter helper keeps same-path map access slash-method metadata precedence
- C++ emitter helper rejects explicit map slash-method count receiver fallback
- C++ emitter helper rejects cross-path direct map count and contains receiver metadata fallback
- C++ emitter helper rejects canonical return-struct fallback for direct map tryAt compatibility calls
- C++ emitter helper rejects rooted map contains and tryAt direct-call return metadata
- C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata
- C++ emitter helper rejects rooted map access direct-call return metadata
- C++ emitter helper keeps canonical map access direct-call return metadata
- C++ emitter helper keeps cross-path vector alias access struct-return metadata

### test_compile_run_emitters_matrix_quaternion_support.cpp

- rejects C++ matrix arithmetic helpers with unsupported divide lowering
- rejects C++ quaternion arithmetic helpers with unsupported divide lowering
- C++ emitter keeps support-matrix plus mismatch diagnostics
- C++ emitter keeps support-matrix implicit conversion diagnostics
- rejects string-keyed map constructor in C++ emitter
- compiles and runs lerp in C++ emitter
- compiles and runs math-qualified clamp in C++ emitter
- compiles and runs math-qualified trig in C++ emitter
- compiles and runs math-qualified min/max in C++ emitter
- compiles and runs math-qualified constants in C++ emitter
- compiles and runs imported math constants in C++ emitter
- compiles and runs rounding builtins in C++ emitter
- compiles and runs convert<bool> from float in C++ emitter
- compiles and runs string comparisons in C++ emitter
- compiles and runs string map values in C++ emitter
- compiles and runs power/log builtins in C++ emitter
- compiles and runs integer pow negative exponent in C++ emitter
- compiles and runs trig builtins in C++ emitter
- compiles and runs hyperbolic builtins in C++ emitter
- compiles and runs float utils in C++ emitter
- compiles and runs float predicates in C++ emitter
- compiles and runs import aliases in C++ emitter
- compiles and runs math constants in C++ emitter
- compiles and runs array unsafe access in C++ emitter
- compiles and runs array unsafe access with u64 index in C++ emitter
- compiles and runs canonical vector discard helpers with owned elements in C++ emitter
- compiles and runs canonical vector indexed removal helpers with owned elements in C++ emitter
- rejects vector reserve with non-relocation-trivial elements in C++ emitter
- rejects vector constructor with non-relocation-trivial elements in C++ emitter
- supports indexed vector removals with ownership semantics in C++ emitter

### test_compile_run_emitters_namespaced_vector_map_helper_resolution.cpp

- rejects vector alias templated forwarding past non-templated compatibility helper in C++ emitter
- rejects vector namespaced count capacity access aliases without helpers in C++ emitter
- C++ emitter lowers stdlib namespaced vector mutator statement through imported helper
- C++ emitter rejects vector namespaced mutator alias statement without helper before emission
- C++ emitter keeps stdlib namespaced vector access helper emission
- C++ emitter compiles stdlib namespaced vector at map target without import
- C++ emitter compiles stdlib namespaced map access and count helpers
- C++ emitter keeps wrapper-returned canonical map references through reference binding
- C++ emitter keeps canonical diagnostics on wrapper-returned canonical map reference method sugar
- C++ emitter runs canonical map reference string access
- C++ emitter keeps non-string diagnostics on canonical map reference access receivers
- rejects map namespaced count compatibility alias in C++ emitter
- rejects map namespaced contains compatibility alias in C++ emitter
- rejects map namespaced tryAt compatibility alias in C++ emitter
- rejects stdlib namespaced map count alias fallback without import in C++ emitter
- rejects map namespaced at compatibility alias in C++ emitter without explicit alias
- rejects map namespaced at unsafe compatibility alias in C++ emitter without explicit alias
- C++ emitter rejects canonical direct map access before emission
- rejects canonical direct map access without helper in C++ emitter
- C++ emitter rejects direct builtin count on canonical map access without helper before lowering
- C++ emitter rejects direct builtin contains on canonical map access before deleted stubs
- rejects direct builtin contains on canonical map access without helper in C++ emitter
- C++ emitter rejects wrapper-returned map access contains receivers before deleted stubs
- rejects wrapper-returned slash-method map access contains without helper in C++ emitter
- C++ emitter rejects canonical slash-method map access before emission

### test_compile_run_emitters_namespaced_vector_push_and_count_helpers.cpp

- rejects reordered namespaced vector push call expression compatibility alias in C++ emitter
- compiles and runs auto-inferred std namespaced vector push canonical precedence in C++ emitter
- compiles and runs auto-inferred std namespaced vector push canonical definition in C++ emitter
- rejects auto-inferred std namespaced count helper return mismatch in C++ emitter
- compiles std namespaced count helper canonical fallback in C++ emitter
- compiles and runs std namespaced count expression canonical precedence in C++ emitter
- compiles std namespaced count expression canonical fallback in C++ emitter
- rejects std namespaced count non-builtin compatibility fallback in C++ emitter
- rejects std namespaced count non-builtin compatibility fallback type mismatch in C++ emitter
- rejects vector namespaced count non-builtin array fallback in C++ emitter
- compiles and runs std namespaced capacity expression canonical precedence in C++ emitter
- compiles and runs std namespaced capacity expression canonical fallback in C++ emitter
- rejects user vector mutator bool positional call shadow in C++ emitter
- C++ emitter mutator rewrite keeps known vector receiver leading names
- rejects user vector access named call shadow in C++ emitter
- compiles and runs auto-inferred std namespaced access helper canonical precedence in C++ emitter
- compiles and runs auto-inferred std namespaced access helper canonical definition in C++ emitter
- compiles and runs wrapper std namespaced access helper named receiver in C++ emitter
- compiles and runs std collections /std/collections/vector/at wrapper in C++ emitter
- rejects wrapper std namespaced access helper named receiver without helper in C++ emitter
- rejects removed vector access alias named arguments in C++ emitter
- rejects removed vector access alias at_unsafe named arguments in C++ emitter
- rejects user vector access positional call shadow in C++ emitter
- rejects user map access string positional call shadow in C++ emitter
- C++ emitter rejects later map receiver positional shadow without canonical reorder

### test_compile_run_emitters_string_receiver_vector_access.cpp

- C++ emitter rejects direct-call string vector access helpers before lowering
- rejects direct-call string vector access builtin fallback in C++ emitter
- rejects slash-method wrapper string access method chain compatibility fallback in C++ emitter
- rejects slash-method wrapper string access method chain i32 diagnostics in C++ emitter
- C++ emitter rejects slash-method string vector access helpers before lowering
- rejects slash-method string vector access builtin fallback in C++ emitter
- rejects alias slash-method vector access on array receiver in C++ emitter
- rejects vector alias access struct method chain without same-path helper in C++ emitter
- rejects vector alias access struct method chain canonical receiver diagnostics in C++ emitter
- rejects vector alias access field expression without same-path helper in C++ emitter
- keeps canonical vector access call struct method chain forwarding in C++ emitter
- C++ emitter keeps canonical vector unsafe access field expression forwarding
- keeps canonical direct-call map access struct method chain forwarding in C++ emitter
- keeps canonical direct-call map unsafe struct method chain forwarding in C++ emitter
- keeps canonical direct-call map access primitive diagnostics in C++ emitter
- prefers canonical bare map method struct chain forwarding in C++ emitter
- prefers canonical bare map unsafe method struct chain forwarding in C++ emitter
- keeps canonical bare map method non-struct diagnostics in C++ emitter
- rejects map access compatibility call struct method chain canonical forwarding in C++ emitter

### test_compile_run_emitters_variadic_file_packs.cpp

- C++ emitter materializes variadic pointer FileError packs with indexed dereference why methods
- C++ emitter materializes variadic borrowed File handle packs with indexed dereference file methods
- C++ emitter materializes variadic pointer File handle packs with indexed dereference file methods
- C++ emitter rejects variadic pointer string packs
- C++ emitter rejects variadic reference string packs
- C++ emitter rejects variadic reference packs without location forwarding
- C++ emitter rejects variadic pointer packs without location forwarding
- C++ emitter materializes variadic borrowed map packs with indexed count_ref calls
- C++ emitter materializes variadic scalar pointer packs from borrowed locations
- C++ emitter materializes variadic struct pointer packs from borrowed locations
- C++ emitter materializes variadic scalar pointer packs from imported helper references
- C++ emitter materializes variadic struct pointer packs from imported helper references

### test_compile_run_emitters_variadic_pointer_pack_access.cpp

- C++ emitter materializes variadic scalar pointer packs from borrowed pack access
- C++ emitter materializes variadic struct pointer packs from borrowed pack access
- C++ emitter materializes variadic scalar pointer packs from borrowed pack field access
- C++ emitter materializes variadic struct pointer packs from borrowed pack field access
- C++ emitter materializes variadic scalar pointer packs from borrowed pack reference fields
- C++ emitter materializes variadic scalar pointer packs from indexed dereference receiver reference fields
- C++ emitter materializes variadic struct pointer packs from borrowed pack reference fields
- C++ emitter materializes variadic scalar reference packs from borrowed pack reference fields

### test_compile_run_emitters_variadic_reference_pack_access.cpp

- C++ emitter materializes variadic struct reference packs from borrowed pack reference fields
- C++ emitter materializes variadic pointer uninitialized scalar packs with indexed init and take
- C++ emitter materializes variadic pointer uninitialized struct packs from borrowed helper references
- C++ emitter materializes variadic borrowed uninitialized scalar packs with indexed init and take
- C++ emitter materializes variadic borrowed uninitialized struct packs with indexed init and take

### test_compile_run_emitters_vector_access_metadata_resolution.cpp

- C++ emitter helper rejects bare vector access methods without helper metadata
- C++ emitter helper resolves bare vector access methods through canonical helper metadata
- C++ emitter helper keeps bare vector access canonical metadata precedence
- C++ emitter helper rejects canonical metadata fallback for explicit vector count capacity aliases
- C++ emitter helper keeps /vector/count while rejecting removed full-path vector aliases
- C++ emitter helper keeps canonical receiver metadata for namespaced map access
- C++ emitter helper falls back to canonical map receiver metadata when alias missing
- C++ emitter helper rejects bare map access metadata-only struct forwarding
- C++ emitter helper rejects direct map access compatibility metadata fallback
- C++ emitter helper keeps slash-path map access struct forwarding
- C++ emitter helper normalizes slashless map import receiver metadata paths
- C++ emitter helper resolves map method via import-alias return metadata
- C++ emitter helper normalizes slashless map metadata lookup targets
- C++ emitter helper resolves direct slashless canonical map receiver metadata

### test_compile_run_emitters_vector_access_slash_alias_helpers.cpp

- compiles and runs vector namespaced access slash methods through explicit alias helpers in C++ emitter
- C++ emitter rejects vector namespaced access slash methods without alias helper before lowering
- rejects vector namespaced access slash methods without alias helper in C++ emitter
- rejects stdlib vector namespaced access slash methods without canonical helper in C++ emitter
- rejects array compatibility access slash methods on vector receiver in C++ emitter
- rejects array compatibility access slash method chain before receiver typing in C++ emitter
- rejects wrapper-returned array compatibility access slash method chains before receiver typing in C++ emitter
- C++ emitter rejects bare vector at methods without helper before emission
- rejects bare vector at methods without helper in C++ emitter
- C++ emitter rejects bare vector at_unsafe methods without helper before emission
- rejects wrapper vector at_unsafe methods without helper in C++ emitter
- C++ emitter rejects wrapper bare vector at calls before deleted stubs
- rejects wrapper bare vector at calls without helper in C++ emitter
- C++ emitter rejects wrapper bare vector at_unsafe calls before deleted stubs
- C++ emitter rejects wrapper explicit vector access calls before deleted stubs
- rejects wrapper bare vector at_unsafe calls without helper in C++ emitter
- rejects wrapper explicit vector access calls without helper in C++ emitter
- C++ emitter rejects bare vector mutator calls without helper before emission
- C++ emitter rejects bare vector mutator methods without helper before emission
- C++ emitter rejects explicit vector mutator alias methods without helper before emission
- rejects bare vector mutator call statements without helper in C++ emitter
- rejects bare vector mutator method statements without helper in C++ emitter
- rejects explicit vector mutator alias method statements without helper in C++ emitter
- rejects explicit canonical vector clear method without imported helper in C++ emitter
- rejects explicit canonical vector push method without imported helper in C++ emitter
- rejects explicit canonical vector pop method without imported helper in C++ emitter
- rejects explicit canonical vector reserve method without imported helper in C++ emitter
- rejects explicit canonical vector mutator method helper without access helper in C++ emitter
- C++ emitter rejects alias vector mutator methods with canonical-only helper before emission
- rejects alias vector mutator methods with canonical-only helper in C++ emitter
- C++ emitter rejects canonical vector mutator methods with alias-only helper before emission

### test_compile_run_emitters_vector_alias_template_forwarding.cpp

- rejects vector alias arity-mismatch compatibility template forwarding in C++ emitter
- compiles vector alias compatibility template forwarding on bool->i32 conversion in C++ emitter
- rejects vector alias compatibility template forwarding on non-bool type mismatch in C++ emitter
- rejects vector alias compatibility template forwarding on struct type mismatch in C++ emitter
- rejects vector alias compatibility template forwarding on constructor temporary struct mismatch in C++ emitter
- rejects vector alias compatibility template forwarding on method-call temporary struct mismatch in C++ emitter
- rejects vector alias compatibility template forwarding on chained method-call temporary struct mismatch in C++ emitter
- rejects vector alias compatibility template forwarding on array envelope element mismatch in C++ emitter
- rejects vector alias compatibility template forwarding on map envelope mismatch in C++ emitter
- rejects vector alias compatibility template forwarding on map envelope mismatch from call return in C++ emitter
- rejects vector alias compatibility template forwarding on primitive mismatch from call return in C++ emitter
- rejects vector alias compatibility template forwarding when unknown expected meets primitive call return in C++ emitter
- rejects vector alias compatibility template forwarding when unknown expected meets primitive binding in C++ emitter
- rejects vector alias compatibility template forwarding when unknown expected meets vector envelope binding in C++ emitter
- rejects vector helper method expression legacy alias forwarding in C++ emitter
- rejects vector alias named-argument compatibility template forwarding in C++ emitter
- rejects wrapper temporary templated vector method canonical forwarding in C++ emitter
- rejects array alias templated forwarding to canonical vector helper in C++ emitter
- rejects stdlib templated vector count fallback to array alias in C++ emitter
- compiles and runs array alias count through same-path helper in C++ emitter
- compiles and runs array alias capacity through same-path helper in C++ emitter
- compiles and runs array alias at through same-path helper in C++ emitter
- compiles and runs array alias at_unsafe through same-path helper in C++ emitter
- rejects array alias slash-method helper chains on vector receivers in C++ emitter

### test_compile_run_emitters_vector_capacity_receiver_resolution.cpp

- C++ emitter rejects local canonical slash-method vector capacity same-path helper on string receiver
- C++ emitter rejects local canonical slash-method vector capacity same-path helper on map receiver
- C++ emitter rejects local canonical slash-method vector capacity same-path helper on array receiver
- C++ emitter keeps alias direct-call vector capacity same-path helper on array receiver
- rejects canonical direct-call vector capacity on map receiver without helper in C++ emitter
- rejects alias direct-call vector capacity on map receiver without helper in C++ emitter
- rejects alias direct-call vector capacity on array receiver without helper in C++ emitter
- rejects canonical direct-call vector capacity builtin fallback on map receiver in C++ emitter
- C++ emitter rejects canonical slash-method vector capacity same-path helper on map receiver
- C++ emitter rejects canonical slash-method vector capacity same-path helper on array receiver
- C++ emitter rejects canonical slash-method vector capacity same-path helper on string receiver
- C++ emitter rejects canonical slash-method vector capacity on map receiver before emission
- rejects canonical slash-method vector capacity on map receiver in C++ emitter
- C++ emitter rejects alias slash-method vector capacity same-path helper on map receiver
- C++ emitter rejects alias slash-method vector capacity on map receiver before emission
- rejects alias slash-method vector capacity on map receiver in C++ emitter
- C++ emitter infers wrapper collection builtin fallback
- C++ emitter keeps bare vector count methods on same-path helper
- C++ emitter keeps bare vector capacity methods on same-path helper
- C++ emitter keeps bare vector count methods on emitter fallback
- keeps bare vector count methods without helper in C++ emitter
- C++ emitter rejects bare vector capacity methods before emitter fallback
- rejects bare vector capacity methods without helper in C++ emitter
- rejects wrapper vector count methods without helper in C++ emitter
- rejects wrapper vector count methods without helper before emission in C++ emitter
- C++ emitter rejects wrapper vector count slash-method chains before receiver typing
- compiles and runs wrapper bare vector count through imported stdlib helper in C++ emitter
- C++ emitter rejects wrapper bare vector count calls without helper before emission
- C++ emitter rejects namespaced vector count on wrapper map target before emission
- rejects wrapper explicit vector count alias calls without helper in C++ emitter
- C++ emitter rejects wrapper explicit vector count capacity calls on count first before emission
- rejects wrapper explicit vector count capacity aliases on count first when only canonical helpers exist in C++ emitter
- rejects wrapper bare vector count calls without helper in C++ emitter

### test_compile_run_emitters_vector_receiver_metadata_resolution.cpp

- C++ emitter helper keeps cross-path vector access return-kind metadata
- C++ emitter helper keeps vector alias direct-call method receiver without metadata
- C++ emitter helper rejects canonical direct-call method receiver without metadata
- C++ emitter helper resolves canonical direct-call method receiver through same-path metadata
- C++ emitter helper resolves parser-shaped canonical direct-call method receiver through same-path metadata
- C++ emitter helper resolves explicit vector slash-method receivers through builtin receiver typing
- C++ emitter helper prefers same-path vector slash-method access return-kind metadata
- C++ emitter helper handles cross-path vector slash-method access metadata fallback
- C++ emitter helper handles explicit vector slash-method receivers without metadata
- C++ emitter helper handles explicit removed vector slash count capacity helpers
- C++ emitter helper handles parser-shaped canonical vector count capacity methods
- C++ emitter helper resolves borrowed soa_vector receiver methods to canonical ref helpers
- C++ emitter helper resolves borrowed local soa_vector field methods through concrete specialization metadata
- C++ emitter helper resolves location and dereference soa_vector field methods through concrete specialization metadata
- C++ emitter helper resolves helper-return soa_vector borrow methods to canonical ref helpers
- C++ emitter helper resolves helper-return concrete soa_vector field methods through return-struct metadata
- C++ emitter helper keeps helper-return soa_vector mutator shadows on wrapper paths
- C++ emitter helper keeps direct helper-return soa_vector mutator shadows on wrapper paths
- C++ emitter helper keeps nested helper-return soa_vector mutator shadows on wrapper paths
- C++ emitter helper handles cross-path vector slash count capacity fallback
- rejects stdlib canonical vector helper method-precedence forwarding in C++ emitter
- rejects templated stdlib canonical vector helper method template args in C++ emitter
- runs vector namespaced count capacity aliases through explicit alias helpers in C++ emitter
- rejects vector namespaced count capacity aliases with only canonical helpers in C++ emitter
- rejects vector namespaced templated canonical helper alias call without alias definition in C++ emitter

### test_compile_run_emitters_wrapper_direct_call_receiver_fallbacks.cpp

- compiles and runs wrapper canonical direct-call struct method chain forwarding in C++ emitter
- rejects wrapper canonical direct-call method receiver fallback without helper in C++ emitter
- rejects wrapper canonical direct-call map method receiver fallback without helper in C++ emitter
- rejects wrapper compatibility direct-call map receiver fallback without helper in C++ emitter
- C++ emitter rejects wrapper-returned vector direct-call string count forwarding
- C++ emitter rejects vector alias direct-call count canonical wrapper return forwarding
- C++ emitter keeps primitive diagnostics on vector alias access count with canonical non-string wrapper return
- rejects inferred wrapper string count arg mismatch in C++ emitter
- rejects inferred wrapper string access index mismatch in C++ emitter
- rejects user wrapper temporary access shadow precedence in C++ emitter
- rejects user wrapper temporary access shadow value mismatch in C++ emitter
- rejects non-vector capacity call target in C++ emitter
- rejects non-vector capacity method target in C++ emitter
- rejects user array capacity helper shadow in C++ emitter
- compiles and runs std math vector and color types
- compiles and runs std math matrix types
- compiles and runs std math quaternion type
- compiles and runs std math quat_to_mat3 helper
- compiles and runs std math quat_to_mat4 helper
- compiles and runs std math mat3_to_quat helper
- compiles and runs C++ support-matrix math nominal helpers
- compiles and runs C++ quaternion reference multiply and rotation
- compiles and runs C++ matrix composition order references with tolerance

### test_compile_run_emitters_wrapper_map_count_and_string_fallback.cpp

- C++ emitter rejects wrapper-returned slash-method map access count with same-path diagnostics
- C++ emitter rejects wrapper-returned slash-method map access count before deleted stubs
- C++ emitter rejects explicit canonical map count slash-method receiver fallback
- C++ emitter keeps stdlib namespaced vector string access count fallback
- rejects canonical vector access direct-call string count fallback in C++ emitter
- C++ emitter rejects wrapper vector direct-call count receivers before deleted access stubs
- C++ emitter keeps slash-method vector access count through builtin string length
- C++ emitter rejects slash-method vector count receivers before deleted access stubs
- rejects slash-method vector count receivers without helper in C++ emitter
- rejects wrapper vector alias direct-call count without helper in C++ emitter
- rejects stdlib namespaced vector access count for non-string element in C++ emitter
- C++ emitter keeps canonical vector unsafe direct-call count via builtin string length
- C++ emitter keeps canonical vector method helper return precedence over string count shadow at runtime
- C++ emitter rejects canonical vector unsafe method string count forwarding
- rejects slash-method vector access element-type count fallback in C++ emitter
- C++ emitter rejects slash-method vector access helper return-kind count forwarding
- rejects wrapper-returned vector alias direct-call string count fallback in C++ emitter
- rejects bare vector method access receiver fallback without helper in C++ emitter
- rejects bare map method access receiver fallback without helper in C++ emitter
- rejects wrapper bare vector unsafe method access receiver fallback in C++ emitter
- rejects wrapper vector alias direct-call struct method chain canonical forwarding in C++ emitter
- rejects wrapper vector alias direct-call method receiver fallback without helper in C++ emitter

### test_compile_run_emitters_wrapper_map_count_sugar.cpp

- C++ emitter resolves canonical map count helper on wrapper slash return method sugar
- C++ emitter keeps canonical map count diagnostics on wrapper slash return method sugar
- C++ emitter rejects templated canonical map count helper on wrapper slash return method sugar
- C++ emitter keeps canonical diagnostics on templated wrapper slash return map count sugar
- C++ emitter resolves direct canonical map count wrappers on map references
- C++ emitter keeps canonical diagnostics on direct canonical map count reference wrappers
- C++ emitter keeps canonical map return arity diagnostics for stdlib envelopes
- C++ emitter rejects explicit canonical map typed bindings for builtin helpers
- C++ emitter keeps canonical map sugar before compatibility aliases
- C++ emitter rejects explicit-template map count method with non-templated alias helper
- C++ emitter resolves alias explicit-template map count method precedence
- C++ emitter keeps builtin map diagnostics on explicit canonical typed bindings
- C++ emitter rejects canonical stdlib namespaced map access helpers in expressions
- C++ emitter rejects canonical precedence for stdlib namespaced map at in expressions
- C++ emitter keeps canonical stdlib namespaced map count helpers in expressions
- C++ emitter keeps canonical diagnostics for stdlib namespaced map count
- C++ emitter rejects canonical unknown map helper with canonical diagnostics
- C++ emitter rejects canonical direct-call map access string receivers at runtime
- C++ emitter keeps canonical diagnostics on direct-call map access receivers

## compile_run/examples (124 tests, 12 files)

### test_compile_run_examples_browser_smoke.cpp

- spinning cube integration artifact matrix stays valid
- spinning cube optional startup visual smoke checks
- spinning cube browser startup smoke proves wasm bootstrap

### test_compile_run_examples_demo_script_args.cpp

- spinning cube demo script rejects invalid port-base values
- spinning cube demo script rejects missing primec values
- spinning cube demo script rejects missing work-dir values
- spinning cube demo script rejects missing port-base values
- spinning cube demo script rejects explicit primec paths that are not executable files
- spinning cube demo script rejects invalid default primec paths
- spinning cube demo script rejects unknown tokens

### test_compile_run_examples_demo_script_core.cpp

- spinning cube demo script emits deterministic summary
- spinning cube demo script skips known native backend limitation
- spinning cube demo script accepts primec path with spaces
- spinning cube demo script accepts work-dir path with spaces
- spinning cube demo script skips when browser command is unavailable
- spinning cube demo script reports fail when native compile fails

### test_compile_run_examples_docs.cpp

- compiles concrete examples to IR
- compiles template and inference examples to IR
- compiles surface examples to IR
- compiles web examples to IR
- checked-in ast transform example runs in VM
- procedural generic docs example runs in VM and native
- generic design examples stay documented and executable
- collection docs snippets stay code-examples style and executable
- sum docs snippets stay public style and executable
- collection docs snippets reject mutators in value position
- spinning cube shared source reflects current profile support

### test_compile_run_examples_docs_commands.cpp

- spinning cube docs command snippets stay executable

### test_compile_run_examples_docs_locks.cpp

- contributor doctest guardrails stay source locked
- focused backend rerun helper stays documented
- skipped doctest debt stays absent from unit shards
- spinning cube native-window status avoids inactive TODO pointers
- vector dynamic-storage docs lock completed first slice
- semantic-product docs avoid inactive Group 12 pointers
- reflection metadata docs avoid inactive roadmap pointers
- result payload docs avoid inactive follow-up pointers
- graphics UI docs avoid inactive follow-up pointers
- coding guidelines avoid inactive surface status pointers
- stdlib style boundary docs stay source locked
- vector map bridge boundary docs stay source locked
- stdlib de-experimentalization policy docs stay source locked
- generic contiguous buffer substrate docs and coverage stay source locked
- soa public collection docs stay source locked
- legacy soa_vector compatibility rejection matrix stays source locked
- soa compatibility fixture migration boundary stays source locked
- arg-pack docs do not point at inactive TODO slices
- generic soa substrate boundary stays source locked
- canonical soa example stays source locked
- source lock inventory keeps replacement surfaces explicit
- status-only result bridge docs stay source locked
- Result helper compatibility adapter inventory stays source locked
- generic requirement predicate surface stays source locked
- safe pointer optionality docs stay source locked
- capability parameterized views docs stay source locked
- task spawn wait prototype docs stay source locked
- todo queue and skipped doctest debt stay source locked
- constructor-shaped compatibility inventory stays source locked
- scene renderer ui producer contract stays source locked
- ui command list adapter docs stay source locked
- image api docs and stdlib stay source locked
- file readByte docs and helpers stay source locked
- container error docs and helpers stay source locked
- maybe stdlib control flow stays source locked to surface if syntax
- small stdlib wrappers stay source locked to inferred locals
- ppm image workflows keep explicit read locals
- png prelude image workflows keep explicit read locals
- png scanline bitstream and inflate helpers stay source locked to inferred locals
- png top-level read write workflows stay source locked to inferred locals
- gfx stdlib wrappers stay source locked to parser-safe locals
- ui stdlib workflows stay source locked to inferred locals
- surface examples stay source locked to lowering-compatible helper forms
- gfx stdlib compatibility shim stays source locked
- ui stdlib arithmetic and assignment stay source locked to surface operators
- ui scene producer composite widgets stay locked to basic widgets
- ui html adapter stays source locked to shared widgets
- ui event stream stays source locked to normalized helpers

### test_compile_run_examples_metal_and_browser_hosts.cpp

- spinning cube metal shader path compiles and enforces profile gating
- spinning cube metal host pipeline config locks vertex descriptor wiring
- spinning cube metal host software surface bridge stays source locked
- shared metal offscreen host helper stays source locked
- shared spinning cube simulation reference helper stays source locked
- shared browser runtime helper stays source locked
- browser spinning cube launcher wrapper stays thin over shared helper
- browser launcher skips smoke before wasm compile when python3 is unavailable
- browser launcher compile run coverage validates shared helper path
- metal spinning cube launcher wrapper stays thin over shared helper
- metal launcher compile run coverage validates shared helper path
- spinning cube vertexcolored snippets stay source locked

### test_compile_run_examples_metal_smoke_and_borrows.cpp

- spinning cube metal host missing metallib diagnostics stay stable
- spinning cube metal host pipeline creation regression stays fixed
- spinning cube metal full-path smoke renders frame
- spinning cube metal software surface bridge smoke
- borrow checker negative examples fail with expected diagnostics

### test_compile_run_examples_native_launcher.cpp

- native window launcher wrapper delegates to shared gfx helper
- native window launcher script builds and launches with preflight
- native window launcher script rejects missing primec binary path
- native window launcher script rejects unknown args
- native window launcher script rejects incompatible smoke flags
- native window launcher defaults to ten-second bounded run

### test_compile_run_examples_native_visual.cpp

- native window launcher visual smoke skips on non-macOS runners
- native window launcher visual smoke skips without GUI session
- native window launcher visual smoke validates criteria
- native window launcher visual smoke fails when rotation does not change
- native window launcher compile run coverage validates host build and visual smoke
- native window preflight script validates required tools and GUI session
- native window preflight script fails when xcrun metal is unavailable
- native window preflight script fails when xcrun metallib is unavailable
- native window preflight script fails when GUI session is unavailable
- native window preflight script rejects unknown args

### test_compile_run_examples_spinning_cube_native.cpp

- spinning cube native flat frame entrypoints compile and run deterministically
- spinning cube stdlib gfx frame stream entry stays source locked
- spinning cube stdlib gfx frame stream entry stays deterministic
- spinning cube browser host assets pass pipeline smoke checks
- spinning cube browser host assets stay source locked
- spinning cube browser profile rules gate unsupported code
- spinning cube native host runtime smoke emits success marker
- spinning cube native window host locks indexed cube pipeline resources
- spinning cube native window host software surface bridge stays source locked
- shared gfx contract header stays source locked
- shared native metal window helper stays source locked

### test_compile_run_examples_spinning_cube_runtime.cpp

- spinning cube native window host sample compiles and validates args deterministically
- spinning cube native window host software surface bridge visual smoke
- spinning cube fixed-step snapshots stay deterministic
- spinning cube transform rotation parity stays aligned across backends

## compile_run/imports (248 tests, 4 files)

### test_compile_run_imports_blocks.cpp

- compiles and runs import inside namespace
- compiles and runs import with import aliases
- compiles and runs block expression with multiline body
- compiles and runs block expression return value
- compiles and runs block expression early return
- compiles and runs block binding inference for method calls
- compiles and runs block binding inference for mixed if numeric types
- compiles and runs operator rewrite
- compiles and runs operator rewrite with calls
- compiles and runs operator rewrite with parentheses
- compiles and runs operator rewrite with unary minus operand
- rejects negate on u64
- compiles and runs short-circuit and
- compiles and runs short-circuit or
- compiles and runs numeric boolean ops
- compiles and runs convert<bool>
- compiles and runs convert<i64>
- compiles and runs convert<u64>
- compiles and runs convert<bool> from u64
- compiles and runs convert<bool> from negative i64
- compiles and runs pointer helpers
- compiles and runs pointer plus helper
- compiles and runs pointer plus on reference
- compiles and runs pointer minus helper
- compiles and runs pointer minus u64 offset

### test_compile_run_imports_operations.cpp

- runs collection literals with map at in C++ emitter
- query-local auto vector helpers run in C++ emitter
- exact vector import runs explicit stdlib surface in C++ emitter
- map wildcard import rejects stdlib-owned surface in C++ emitter
- concise vector binding example runs in C++ emitter
- concise vector binding example runs in VM
- rejects experimental soa_vector stdlib helpers in C++ emitter
- rejects raw soa_vector type spelling in C++ emitter
- compiles and runs public soa count helper on public wrapper in C++ emitter
- compiles and runs public soa get helper in C++ emitter
- public soa get helper rejects template arguments on non-soa receiver in C++ emitter
- public soa get slash-method keeps canonical reject in C++ emitter
- public soa to_aos slash-method keeps canonical reject in C++ emitter
- compiles and runs public soa ref helper in C++ emitter
- compiles and runs public soa mutator helpers in C++ emitter
- public soa to_aos helper lowers in C++ emitter
- public soa to_aos temporaries route through canonical vector capacity in C++ emitter
- public soa to_aos explicit helper is a vector target in C++ emitter
- legacy soa_vector compatibility helpers reject in C++ emitter
- rejects graph-solved direct local-auto vector helper shadows in C++ emitter compatibility
- rejects experimental soa_vector stdlib wide structs on pending width boundary across imported/direct/helper-return forms in C++ emitter
- rejects experimental soa_vector stdlib from-aos helper in C++ emitter before typed bindings support
- rejects root non-struct non-empty soa_vector literal with semantic/emit parity in C++ emitter
- runs experimental soa_vector stdlib to-aos helper in C++ emitter
- rejects experimental soa_vector stdlib to-aos method on wrapper surface in C++ emitter
- no-import root soa_vector to_aos bare and direct helper forms reject SoaVector-only canonical helper contract in C++ emitter
- no-import root soa_vector to_aos method helper forms reject during semantics in C++ emitter
- no-import root soa_vector canonical to_aos_ref helper form rejects in C++ emitter
- experimental SoaVector canonical to_aos_ref helper form rejects in C++ emitter
- direct experimental soaVectorToAos helpers on builtin soa_vector reject in C++ emitter
- runs experimental soa_vector stdlib non-empty to-aos helper in C++ emitter
- rejects experimental soa_vector stdlib non-empty to-aos method on wrapper state in C++ emitter
- rejects experimental soa_vector stdlib get helper in C++ emitter
- rejects experimental soa_vector stdlib get method in C++ emitter
- rejects bare soa_vector get helper through helper return in C++ emitter compatibility
- rejects global helper-return soa_vector method shadows in C++ emitter compatibility
- rejects method-like helper-return soa_vector method shadows in C++ emitter compatibility
- compiles and runs vector-target old-explicit soa mutator shadows in C++ emitter
- compiles and runs vector-target method soa mutator shadows in C++ emitter
- rejects vector-target to_aos helper shadows in C++ emitter
- rejects nested struct-body soa_vector constructor-bearing helper returns in C++ emitter compatibility
- rejects nested struct-body soa_vector direct and bound helper expressions in C++ emitter compatibility
- rejects nested struct-body soa_vector method shadows in C++ emitter compatibility
- rejects explicit method-like helper-return experimental soa_vector to_aos shadow in C++ emitter
- rejects experimental soa_vector stdlib ref helper in C++ emitter
- rejects experimental soa_vector stdlib ref method in C++ emitter
- rejects experimental soa_vector ref pass-through and return in C++ emitter
- rejects experimental soa_vector stdlib push and reserve helpers in C++ emitter
- rejects experimental soa_vector stdlib push and reserve methods in C++ emitter
- runs experimental soa_vector single-field index syntax in C++ emitter
- runs experimental soa_vector reflected multi-field index syntax in C++ emitter
- rejects experimental soa_vector mutating indexed field writes in C++ emitter
- runs richer borrowed experimental soa_vector mutating indexed field writes in C++ emitter
- runs method-like borrowed experimental soa_vector mutating indexed field writes in C++ emitter
- runs borrowed experimental soa_vector reflected index syntax in C++ emitter
- runs borrowed local experimental soa_vector reflected index syntax in C++ emitter
- runs borrowed helper-return experimental soa_vector reflected index syntax in C++ emitter
- rejects experimental soa_vector bare get and ref field access in C++ emitter
- rejects experimental soa_vector reflected call-form index syntax in C++ emitter
- rejects experimental soa_vector inline location borrow index syntax in C++ emitter
- rejects dereferenced borrowed helper-return experimental soa_vector reflected index syntax in C++ emitter
- rejects borrowed helper-return experimental soa_vector get/ref methods in C++ emitter
- compiles and runs borrowed helper-return soa_vector ref_ref same-path helper in C++ emitter compatibility
- rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter
- rejects helper-return experimental soa_vector method shadows in C++ emitter
- rejects helper-return soa_vector shadows with explicit canonical fallbacks in C++ emitter compatibility
- rejects borrowed local experimental soa_vector read-only methods in C++ emitter
- rejects inline location experimental soa_vector read-only methods in C++ emitter
- rejects borrowed helper-return experimental soa_vector helper surfaces in C++ emitter
- rejects method-like borrowed helper-return experimental soa_vector helper surfaces in C++ emitter
- rejects direct return borrowed helper-return experimental soa_vector reads in C++ emitter
- rejects direct return method-like borrowed helper-return experimental soa_vector reads in C++ emitter
- rejects direct return inline location borrowed helper-return experimental soa_vector reads in C++ emitter
- rejects inline location method-like borrowed helper-return experimental soa_vector helpers in C++ emitter
- rejects direct return inline location method-like borrowed helper-return experimental soa_vector reads in C++ emitter
- rejects inline location borrowed helper-return experimental soa_vector helpers in C++ emitter
- compiles and runs experimental soa storage helpers in C++ emitter
- compiles and runs experimental soa storage borrowed ref helper in C++ emitter
- compiles and runs experimental soa storage borrowed view helper in C++ emitter
- rejects experimental soa storage reserve overflow in C++ emitter
- compiles and runs experimental two-column soa storage helpers in C++ emitter
- compiles and runs experimental three-column soa storage helpers in C++ emitter
- compiles and runs experimental four-column soa storage helpers in C++ emitter
- compiles and runs experimental five-column soa storage helpers in C++ emitter
- compiles and runs experimental six-column soa storage helpers in C++ emitter
- compiles and runs experimental seven-column soa storage helpers in C++ emitter
- compiles and runs experimental eight-column soa storage helpers in C++ emitter
- compiles and runs experimental nine-column soa storage helpers in C++ emitter
- compiles and runs experimental ten-column soa storage helpers in C++ emitter
- emits experimental eleven-column soa storage helpers in C++ emitter
- emits experimental twelve-column soa storage helpers in C++ emitter
- emits experimental thirteen-column soa storage helpers in C++ emitter
- emits experimental fourteen-column soa storage helpers in C++ emitter
- emits experimental fifteen-column soa storage helpers in C++ emitter
- emits experimental sixteen-column soa storage helpers in C++ emitter
- rejects string-keyed map constructors in C++ emitter
- rejects string-keyed map constructor indexing sugar in C++ emitter
- compiles and runs canonical namespaced map helpers on experimental map values in C++ emitter
- compiles and runs wrapper map helpers on experimental map values in C++ emitter
- compiles and runs ownership-sensitive experimental map value methods in C++ emitter
- compiles and runs helper-wrapped inferred experimental map returns in C++ emitter
- compiles and runs helper-wrapped experimental map parameters in C++ emitter
- compiles and runs helper-wrapped experimental map bindings in C++ emitter
- compiles and runs helper-wrapped experimental map assignment RHS values in C++ emitter
- compiles and runs canonical namespaced map constructors on explicit experimental map bindings in C++ emitter
- compiles and runs canonical namespaced map constructors through explicit experimental map returns in C++ emitter
- compiles and runs canonical namespaced map constructors through explicit experimental map parameters in C++ emitter
- compiles and runs wrapper map constructors on explicit experimental map bindings in C++ emitter
- compiles and runs wrapper map constructors through explicit experimental map returns in C++ emitter
- compiles and runs wrapper map constructors through explicit experimental map parameters in C++ emitter
- compiles and runs experimental map constructor assignments in C++ emitter
- compiles and runs implicit map auto constructor inference in C++ emitter
- compiles and runs inferred experimental map returns in C++ emitter
- compiles and runs block inferred experimental map returns in C++ emitter
- compiles and runs auto block inferred experimental map returns in C++ emitter
- compiles and runs inferred experimental map call receivers in C++ emitter
- rejects explicit experimental map struct field constructors in C++ emitter
- compiles and runs inferred experimental map struct fields in C++ emitter
- compiles and runs helper-wrapped inferred experimental map struct fields in C++ emitter
- compiles and runs experimental map method parameters in C++ emitter
- compiles and runs inferred experimental map parameters in C++ emitter
- compiles and runs inferred experimental map default parameters in C++ emitter
- compiles and runs helper-wrapped inferred experimental map default parameters in C++ emitter
- compiles and runs experimental map helper receivers in C++ emitter
- compiles and runs helper-wrapped experimental map helper receivers in C++ emitter
- runs direct-constructor experimental map method receivers in C++ emitter
- runs helper-wrapped experimental map method receivers in C++ emitter
- compiles and runs experimental map field assignments through canonical helper access in C++ emitter
- compiles and runs dereferenced experimental map storage references in C++ emitter
- compiles and runs helper-wrapped Result.ok experimental map result struct fields in C++ emitter
- compiles and runs helper-wrapped dereferenced Result.ok experimental map result struct fields in C++ emitter
- compiles and runs helper-wrapped experimental map struct storage fields in C++ emitter
- compiles and runs helper-wrapped dereferenced experimental map struct storage fields in C++ emitter
- rejects canonical namespaced map helpers on borrowed experimental map values in C++ emitter
- compiles and runs canonical namespaced map _ref helpers on borrowed experimental map values in C++ emitter
- compiles and runs experimental map methods on bound map values in C++ emitter
- compiles and runs borrowed experimental map helpers in C++ emitter
- compiles and runs public borrowed map wrappers in C++ emitter
- compiles and runs borrowed experimental map methods in C++ emitter
- compiles and runs experimental map inserts in C++ emitter
- compiles and runs experimental map ownership-sensitive values in C++ emitter
- compiles and runs canonical namespaced map inserts on explicit experimental map bindings in C++ emitter
- compiles and runs builtin canonical map first-growth inserts in C++ emitter
- compiles and runs builtin canonical map repeated-growth inserts in C++ emitter
- compiles and runs builtin canonical map insert overwrites in C++ emitter
- compiles and runs builtin canonical map non-local growth in C++ emitter
- compiles and runs builtin canonical map nested non-local growth in C++ emitter
- compiles and runs builtin canonical map helper-return borrowed method inserts in C++ emitter
- compiles and runs builtin canonical map struct-field initializer in C++ emitter
- compiles and runs builtin canonical map direct insert on helper-return value receivers in C++ emitter
- compiles and runs builtin canonical map method insert on helper-return value receivers in C++ emitter
- compiles and runs builtin canonical map direct insert on borrowed holder field receivers in C++ emitter
- rejects canonical map constructor ownership growth in C++ emitter
- rejects experimental map bracket access in C++ emitter
- compiles and runs shared vector conformance harness in C++ emitter
- compiles and runs canonical namespaced vector helpers in C++ emitter
- compiles and runs canonical namespaced vector helpers on explicit Vector bindings in C++ emitter
- compiles and runs stdlib wrapper vector helpers on explicit Vector bindings in C++ emitter
- rejects stdlib wrapper vector helper explicit Vector mismatch in C++ emitter
- compiles and runs stdlib wrapper vector constructors on explicit Vector bindings in C++ emitter
- keeps stdlib wrapper vector constructor explicit Vector mismatch contract in C++ emitter
- compiles and runs stdlib wrapper vector constructors on inferred auto bindings in C++ emitter
- rejects stdlib wrapper vector constructor auto inference mismatch in C++ emitter
- rejects stdlib wrapper vector constructor receivers in C++ emitter
- rejects stdlib wrapper vector helper receiver mismatch in C++ emitter
- rejects stdlib wrapper vector method receiver mismatch in C++ emitter
- rejects canonical namespaced vector constructor temporaries in C++ emitter
- rejects canonical namespaced vector explicit builtin bindings in C++ emitter
- rejects canonical namespaced vector named-argument temporaries in C++ emitter
- rejects canonical namespaced vector named-argument explicit builtin bindings in C++ emitter
- rejects canonical namespaced vector mutators without imported helpers in C++ emitter
- compiles and runs bare vector count and capacity through imported stdlib helpers in C++ emitter
- compiles and runs bare vector access through imported stdlib helpers in C++ emitter
- rejects bare vector count without imported helper in C++ emitter
- rejects bare vector capacity without imported helper in C++ emitter
- bare vector mutators reject without imported helpers in C++ emitter
- compiles and runs experimental vector helper runtime contracts in C++ emitter
- compiles and runs experimental vector ownership-sensitive helpers in C++ emitter
- compiles and runs canonical vector helpers on experimental vector receivers in C++ emitter
- compiles and runs vector pop empty runtime contract in C++ emitter
- compiles and runs vector index runtime contract in C++ emitter
- compiles and runs container error contract conformance in C++ emitter
- compiles and runs checked pointer conformance harness in C++ emitter
- compiles and runs unchecked pointer conformance harness in C++ emitter
- compiles with executions using collection arguments
- compile run rejects execution body arguments
- compiles and runs pointer plus u64 offset
- compiles and runs i64 literals
- compiles and runs u64 literals
- compiles and runs assignment operator rewrite
- compiles and runs comparison operator rewrite
- compiles and runs less_than operator rewrite
- compiles and runs greater_equal operator rewrite
- compiles and runs less_equal operator rewrite
- compiles and runs and operator rewrite
- compiles and runs or operator rewrite
- compiles and runs not operator rewrite
- compiles and runs not operator with parentheses
- compiles and runs unary minus operator rewrite
- compiles and runs equality operator rewrite
- compiles and runs not_equal operator rewrite

### test_compile_run_imports_versions.cpp

- compiles and runs versioned import expansion with relative import entry
- compiles and runs exact versioned import expansion
- rejects versioned legacy include alias expansion
- rejects version-first legacy include alias expansion
- compiles and runs versioned import expansion with quoted import entries
- compiles and runs import expansion with comments
- compiles and runs duplicate imports once
- compiles and runs versioned import with single quotes
- compiles and runs versioned import with major-only selector
- compiles and runs versioned import with minor selector
- compiles and runs versioned import expansion with multiple import entries
- rejects versioned import mismatch across roots
- rejects missing versioned import in compile
- compiles and runs versioned import expansion with mixed quoted and relative entries

### test_compile_run_imports_versions_archive.h

- compiles and runs archive import expansion
- compiles and runs exact versioned archive import expansion
- compiles and runs newest archive import expansion
- conformance: versioned import selects latest for wildcard import exposure
- conformance: duplicate versioned imports are deduplicated before import aliasing
- conformance: versioned import rejects underscore-private paths
- conformance: wildcard import does not expose private members from imported source
- conformance: versioned import directory expansion order is deterministic

## compile_run/native_backend (1082 tests, 47 files)

### test_compile_run_native_backend_argv.cpp

- compiles and runs native argv count
- compiles and runs native argv count helper
- compiles and runs native argv error output
- compiles and runs native argv error output without newline
- compiles and runs native argv error output u64 index
- compiles and runs native argv unsafe error output
- compiles and runs native argv unsafe line error output
- compiles and runs native argv access helpers
- compiles and runs native argv line output
- compiles and runs native argv inline string binding
- native argv rejects returning entry arg string directly
- native argv rejects passing entry arg string to helpers
- native argv rejects assigning entry arg string into aggregates
- native argv rejects helper forwarding entry arg string
- native argv rejects returning unsafe helper string directly
- native argv rejects string comparisons on entry arg strings
- native argv rejects echoing entry args from nested helper calls
- native argv rejects string results in call arguments

### test_compile_run_native_backend_collections.cpp

- compiles and runs native array slice count and indexed access

### test_compile_run_native_backend_collections_alias_diagnostics_a.cpp

- rejects native templated stdlib vector wrapper temporary unsafe method index mismatch
- rejects native vector alias access auto wrapper primitive receiver diagnostics
- rejects native vector alias access auto wrapper canonical diagnostics forwarding
- rejects native vector alias access struct method chain with rooted helper diagnostics
- rejects native vector alias access struct method chain with primitive receiver diagnostics
- rejects native vector alias access field expression with struct receiver diagnostics
- keeps native canonical vector access call struct method chain forwarding
- rejects native canonical vector unsafe access field expression forwarding
- rejects native map access compatibility call struct method chain with primitive receiver diagnostics
- rejects native map access compatibility call struct method chain with primitive argument diagnostics
- rejects native map unsafe compatibility call struct method chain with primitive receiver diagnostics
- rejects native map unsafe compatibility call struct method chain with primitive argument diagnostics
- rejects native vector method alias access struct method chain with array receiver diagnostics
- rejects native vector namespaced access slash methods without alias helper on vector receiver
- rejects native array compatibility access slash methods on vector receiver
- rejects native array compatibility access slash method chain before receiver typing
- rejects native wrapper-returned array compatibility access slash method chains before receiver typing

### test_compile_run_native_backend_collections_alias_diagnostics_b.cpp

- rejects native vector method alias access with current array receiver chain diagnostics
- rejects native vector method alias field expression without alias helper
- rejects native vector unsafe method alias access with current array receiver chain diagnostics
- rejects native vector unsafe method alias field expression without alias helper
- native map method alias access forwards helper receiver chains
- keeps canonical native map method access field expression forwarding
- native vector method alias struct-return field access uses canonical helper typing
- native keeps primitive diagnostics for canonical vector method access
- native forwards canonical vector unsafe method struct field access
- rejects native map method alias access struct method chain with primitive argument diagnostics
- rejects native wrapper-returned map method alias primitive receiver fallback
- rejects native wrapper-returned canonical map slash-method struct receiver forwarding
- rejects native wrapper-returned canonical direct-call map receiver fallback
- rejects native wrapper-returned compatibility direct-call map receiver fallback
- native keeps wrapper-returned map method alias primitive argument diagnostics

### test_compile_run_native_backend_collections_arrays_and_aliases.cpp

- compiles and runs native array literals
- compiles and runs native array access with u64 index
- compiles and runs native array access rejects negative index
- compiles and runs native array unsafe access
- compiles and runs native array unsafe access with u64 index
- compiles and runs native array literal count method
- compiles and runs native array literal unsafe access
- compiles and runs native array count helper
- compiles and runs native array literal count helper
- compiles and runs native vector literals
- compiles and runs native stdlib namespaced vector builtin aliases
- compiles and runs native namespaced wrapper string access method chain compatibility fallback
- rejects native slash-method wrapper string access method chain compatibility fallback
- native keeps slash-method wrapper string access i32 diagnostics
- rejects native alias slash-method vector access on array receiver
- compiles and runs native unchecked pointer conformance harness for imported .prime helpers
- rejects native array namespaced vector constructor alias
- rejects native array namespaced vector at alias
- rejects native array namespaced vector at_unsafe alias
- rejects native wrapper array namespaced vector at alias
- rejects native wrapper array namespaced vector at_unsafe alias
- rejects native array namespaced vector count builtin alias
- rejects native array namespaced vector count method alias
- rejects native array namespaced vector capacity method alias
- rejects native map namespaced count compatibility alias
- rejects native map namespaced contains compatibility alias
- rejects native map namespaced tryAt compatibility alias
- native map namespaced at compatibility alias rejects without explicit alias
- native map namespaced at unsafe compatibility alias rejects without explicit alias

### test_compile_run_native_backend_collections_auto_inferred_helper_precedence.cpp

- rejects native named vector push expression receiver precedence in semantics
- rejects native auto-inferred named vector push expression receiver precedence in semantics
- rejects native auto-inferred std namespaced vector push compatibility alias precedence
- compiles and runs native auto-inferred std namespaced vector push canonical definition
- native auto-inferred std namespaced count uses compatibility alias precedence
- native auto-inferred std namespaced count canonical fallback uses builtin count
- native std namespaced count expression uses compatibility alias precedence
- rejects native std namespaced count without imported helper
- rejects native std namespaced count map target without helper
- rejects native alias count map target without helper
- rejects native std namespaced capacity map target without helper
- rejects native alias capacity map target without helper
- native alias capacity array target accepts same-path helper
- rejects native alias capacity array target without helper
- native std namespaced count expression canonical fallback uses builtin count
- native std namespaced count non-builtin compatibility fallback resolves alias
- rejects native vector namespaced count non-builtin array fallback
- native std namespaced capacity expression uses compatibility alias precedence
- compiles and runs native std namespaced capacity expression canonical fallback

### test_compile_run_native_backend_collections_canonical_count_shadows.cpp

- native keeps imported wrapper-returned canonical map reference access lowering diagnostics
- native keeps canonical map unknown-target diagnostics on wrapper-returned map reference access
- rejects native map method sugar on wrapper-returned canonical map references without imported helpers
- native keeps non-string diagnostics on canonical map reference access count shadow
- native keeps canonical map unknown-target diagnostics on wrapper-returned map reference method sugar
- native keeps non-string diagnostics on wrapper-returned canonical map access count shadow
- compiles native canonical vector access builtin string count shadow
- rejects native canonical vector unsafe access count shadow with expression-call diagnostics
- rejects native canonical vector method access builtin string count shadow
- rejects native canonical vector unsafe method access count shadow
- rejects native direct wrapper-returned canonical map access count shadow
- rejects native wrapper-returned canonical map method access string receiver typing
- rejects native wrapper-returned slash-method map access count shadow
- rejects native slash-method vector access string count fallback
- native keeps slash-method vector access unknown-method diagnostics
- compiles and runs native wrapper-returned vector access string count fallback
- native keeps wrapper-returned vector access primitive count diagnostics
- compiles and runs native user vector count method shadow
- rejects native canonical slash vector count same-path helper on map receiver
- rejects native wrapper-returned canonical vector count slash-method on map receiver
- rejects native wrapper-returned canonical vector capacity slash-method on map receiver
- compiles and runs native canonical slash vector count same-path helper on array receiver
- rejects native wrapper-returned canonical vector count slash-method on array receiver
- compiles and runs native canonical slash vector count same-path helper on string receiver
- rejects native wrapper-returned canonical vector count slash-method on string receiver
- rejects native wrapper-returned canonical vector capacity slash-method on array receiver
- compiles and runs native user vector capacity method shadow
- compiles and runs native user vector count call shadow
- compiles and runs native user vector capacity call shadow
- rejects native user array capacity call shadow
- compiles and runs native user array capacity method shadow
- compiles and runs native user array at call shadow

### test_compile_run_native_backend_collections_canonical_map_helpers.cpp

- compiles and runs native stdlib namespaced map reference access helpers
- rejects native canonical map method with slash return type receiver
- rejects native canonical map access helpers on wrapper slash return receiver
- rejects native canonical map access helper key mismatch on wrapper slash return receiver
- compiles and runs native explicit canonical map typed bindings with builtin helpers
- rejects native explicit canonical map typed binding key mismatch
- rejects native stdlib map constructor alias fallback without import
- rejects native stdlib map at alias fallback without import
- rejects native stdlib map at unsafe alias fallback without import
- compiles native bare map count through canonical helper
- rejects native bare map count without imported canonical helper
- compiles native bare map at through canonical helper
- compiles native bare map at_unsafe through canonical helper
- rejects native bare map at call without helper
- rejects native bare map at_unsafe call without helper
- native map namespaced count method runs through canonical helper
- rejects native bare map count method without imported canonical helper
- compiles and runs native bare map contains through canonical helper
- rejects native bare map contains call without imported canonical helper
- rejects native bare map contains method without imported canonical helper
- rejects native bare map tryAt method without imported canonical helper
- rejects native bare map access methods without imported canonical helpers
- compiles and runs native map namespaced at method compatibility alias

### test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp

- rejects native templated stdlib return wrapper temporaries in expressions
- native query-local auto vector helpers run through lowering
- rejects native experimental soa_vector stdlib helpers
- rejects native raw soa_vector type spelling
- compiles and runs native public soa count helper on public wrapper
- compiles and runs native public soa get helper
- native public soa get helper rejects template arguments on non-soa receiver
- native public soa get slash-method keeps canonical reject
- native public soa to_aos slash-method keeps canonical reject
- compiles and runs native public soa ref helper
- compiles and runs native public soa mutator helpers
- native public soa to_aos helper lowers
- native public soa to_aos temporaries route through canonical vector capacity
- native legacy soa_vector compatibility helpers reject
- native wildcard-imported canonical soa helpers reject current metadata inference gap
- native public soa type spelling keeps generated identity rejection
- native public soa read helpers reject current metadata inference gap
- native public soa construction and mutators reject current metadata inference gap
- native public soa from-aos rejects current metadata inference gap
- native public soa field-view wrappers reject current metadata inference gap
- native compiles and runs graph-solved direct local-auto vector helper shadows compatibility
- native rejects experimental soa_vector stdlib wide structs on pending width boundary across imported/direct/helper-return forms
- native rejects experimental soa_vector stdlib from-aos helper before typed bindings support
- native runs experimental soa_vector stdlib to-aos helper
- native rejects direct experimental soa_vector to-aos helper on builtin soa_vector
- native rejects experimental soa_vector stdlib to-aos method on wrapper surface
- native no-import root soa_vector to_aos bare and direct helper forms reject SoaVector-only canonical helper contract
- native no-import root soa_vector to_aos method helper forms reject during semantics
- native rejects non-empty root soa_vector struct literals
- native rejects non-empty root soa_vector literals with unsupported element envelopes
- native rejects non-empty root soa_vector literals above former local capacity limit
- native runs experimental soa_vector stdlib non-empty to-aos helper
- native rejects experimental soa_vector stdlib non-empty to-aos method on wrapper state
- native rejects experimental soa_vector stdlib get helper through retired import path
- native rejects experimental soa_vector stdlib get method through retired import path
- native rejects bare soa_vector get helper through helper return compatibility
- native rejects global helper-return soa_vector method shadows compatibility
- native rejects method-like helper-return soa_vector method shadows compatibility
- native runs vector-target old-explicit soa mutator shadows
- native runs vector-target method soa mutator shadows
- native rejects vector-target to_aos helper shadows
- native rejects nested struct-body soa_vector constructor-bearing helper returns compatibility
- native rejects nested struct-body soa_vector direct and bound helper expressions compatibility
- native rejects nested struct-body soa_vector method shadows compatibility
- native rejects explicit method-like helper-return experimental soa_vector to_aos shadow
- native rejects experimental soa_vector stdlib ref helper through retired import path
- native rejects experimental soa_vector stdlib ref method through retired import path
- native rejects experimental soa_vector ref pass-through and return
- native rejects experimental soa_vector stdlib push and reserve helpers
- native rejects experimental soa_vector stdlib push and reserve methods
- native rejects experimental soa_vector single-field index syntax
- native rejects experimental soa_vector reflected multi-field index syntax
- native rejects experimental soa_vector mutating indexed field writes
- native rejects richer borrowed experimental soa_vector mutating indexed field writes
- native rejects method-like borrowed experimental soa_vector mutating indexed field writes
- native rejects borrowed experimental soa_vector reflected index syntax
- native rejects experimental soa_vector bare get and ref field access
- native rejects borrowed local experimental soa_vector reflected index syntax
- native rejects borrowed helper-return experimental soa_vector reflected index syntax
- native rejects experimental soa_vector reflected call-form index syntax
- native rejects experimental soa_vector inline location borrow index syntax
- native rejects dereferenced borrowed helper-return experimental soa_vector reflected index syntax
- native rejects borrowed helper-return experimental soa_vector get/ref methods
- native compiles and runs borrowed helper-return soa_vector ref_ref same-path helper compatibility
- native rejects builtin helper-return soa_vector ref_ref same-path helper
- native rejects helper-return experimental soa_vector method shadows
- native rejects helper-return soa_vector shadows with explicit canonical fallbacks compatibility
- native rejects borrowed local experimental soa_vector read-only methods
- native rejects inline location experimental soa_vector read-only methods
- native rejects borrowed helper-return experimental soa_vector helper surfaces
- native rejects method-like borrowed helper-return experimental soa_vector helper surfaces
- native rejects direct return borrowed helper-return experimental soa_vector reads
- native rejects direct return method-like borrowed helper-return experimental soa_vector reads
- native rejects direct return inline location borrowed helper-return experimental soa_vector reads
- native rejects inline location method-like borrowed helper-return experimental soa_vector helpers
- native rejects direct return inline location method-like borrowed helper-return experimental soa_vector reads
- native rejects inline location borrowed helper-return experimental soa_vector helpers
- compiles and runs native experimental soa storage helpers
- compiles and runs native experimental soa storage borrowed ref helper
- compiles and runs native experimental soa storage borrowed view helper
- rejects native experimental soa storage reserve overflow
- compiles and runs native experimental two-column soa storage helpers
- compiles and runs native experimental three-column soa storage helpers
- compiles and runs native experimental four-column soa storage helpers
- compiles and runs native experimental five-column soa storage helpers
- compiles and runs native experimental six-column soa storage helpers
- compiles and runs native experimental seven-column soa storage helpers
- compiles and runs native experimental eight-column soa storage helpers
- compiles and runs native experimental nine-column soa storage helpers
- compiles and runs native experimental ten-column soa storage helpers
- compiles and runs native experimental eleven-column soa storage helpers
- compiles and runs native experimental twelve-column soa storage helpers
- compiles and runs native experimental thirteen-column soa storage helpers
- compiles and runs native experimental fourteen-column soa storage helpers
- compiles and runs native experimental fifteen-column soa storage helpers
- compiles and runs native experimental sixteen-column soa storage helpers
- rejects native templated stdlib wrapper temporary call forms
- rejects native canonical namespaced map helpers on experimental map values
- rejects native wrapper map helpers on experimental map values
- rejects native ownership-sensitive experimental map value methods
- compiles and runs native helper-wrapped inferred experimental map returns
- compiles and runs native helper-wrapped experimental map parameters
- compiles and runs native helper-wrapped experimental map bindings
- compiles and runs native helper-wrapped experimental map assignment RHS values
- rejects native canonical namespaced map constructors on explicit experimental map bindings
- rejects native canonical namespaced map constructors through explicit experimental map returns
- rejects native canonical namespaced map constructors through explicit experimental map parameters
- compiles and runs native wrapper map constructors on explicit experimental map bindings
- compiles and runs native wrapper map constructors through explicit experimental map returns
- compiles and runs native wrapper map constructors through explicit experimental map parameters
- rejects native experimental map variadic constructors
- rejects native experimental map variadic constructor type mismatch
- compiles and runs native experimental map constructor assignments
- compiles and runs native implicit map auto constructor inference
- rejects native inferred experimental map returns
- compiles and runs native block inferred experimental map returns
- compiles and runs native auto block inferred experimental map returns
- rejects native inferred experimental map call receivers
- rejects native experimental map struct fields
- rejects native inferred experimental map struct field constructor expressions
- rejects native helper-wrapped inferred experimental map struct field constructor expressions
- rejects native experimental map method parameter constructor expressions
- rejects native inferred experimental map parameter call expressions
- rejects native inferred experimental map default parameter call expressions
- rejects native helper-wrapped inferred experimental map default parameter call expressions
- compiles and runs native experimental map helper receivers
- compiles and runs native helper-wrapped experimental map helper receivers
- rejects native experimental map method receivers
- rejects native helper-wrapped experimental map method receivers
- compiles and runs native experimental map field assignments
- compiles and runs native dereferenced experimental map storage references
- compiles and runs native helper-wrapped Result.ok experimental map result struct fields
- compiles and runs native helper-wrapped dereferenced Result.ok experimental map result struct fields
- compiles and runs native helper-wrapped experimental map struct storage fields
- compiles and runs native helper-wrapped dereferenced experimental map struct storage fields
- rejects native canonical namespaced map helpers on borrowed experimental map values
- rejects native canonical namespaced map _ref helpers on borrowed experimental map values
- rejects native experimental map methods
- compiles and runs native borrowed experimental map helpers
- compiles and runs native public borrowed map wrappers
- rejects native borrowed experimental map methods
- compiles and runs native experimental map inserts
- rejects native experimental map ownership-sensitive values
- rejects native canonical namespaced map inserts on explicit experimental map bindings
- rejects native builtin canonical map first-growth inserts
- rejects native builtin canonical map repeated-growth inserts
- rejects native builtin canonical map insert overwrites
- compiles and runs native builtin canonical map non-local growth
- compiles and runs native builtin canonical map nested non-local growth
- compiles and runs native builtin canonical map helper-return borrowed method inserts
- compiles and runs native builtin canonical map struct-field initializer
- compiles and runs native builtin canonical map direct insert on helper-return value receivers
- compiles and runs native builtin canonical map method insert on helper-return value receivers
- compiles and runs native builtin canonical map direct insert on borrowed holder field receivers
- rejects native canonical map constructor ownership growth
- compiles and runs native experimental map bracket access
- rejects native experimental map custom comparable struct keys
- covers native shared vector harness contracts
- rejects native canonical namespaced vector helpers
- compiles and runs native canonical namespaced vector helpers on explicit Vector bindings
- compiles and runs native stdlib wrapper vector helpers on explicit Vector bindings
- rejects native stdlib wrapper vector helper explicit Vector mismatch
- compiles and runs native stdlib wrapper vector constructors on explicit Vector bindings
- keeps native stdlib wrapper vector constructor explicit Vector mismatch contract
- compiles and runs native stdlib wrapper vector constructors on inferred auto bindings
- rejects native stdlib wrapper vector constructor auto inference mismatch
- rejects native stdlib wrapper vector constructor receivers
- rejects native stdlib wrapper vector helper receiver mismatch
- rejects native stdlib wrapper vector method receiver mismatch
- rejects native canonical namespaced vector constructor temporaries
- compiles and runs native canonical namespaced vector explicit builtin bindings
- rejects native canonical namespaced vector named-argument temporaries
- compiles and runs native canonical namespaced vector named-argument explicit builtin bindings
- rejects native canonical namespaced vector mutators without imported helpers
- compiles and runs native experimental vector helper runtime contracts
- compiles and runs native experimental vector ownership-sensitive helpers
- compiles and runs native canonical vector helpers on experimental vector receivers
- compiles and runs native vector pop empty runtime contract
- compiles and runs native vector index runtime contract
- compiles and runs native imported container error contract conformance
- compiles and runs native checked pointer conformance harness for imported .prime helpers
- compiles and runs native templated stdlib vector wrapper temporary call forms
- compiles and runs native templated stdlib vector wrapper temporary methods in expressions
- rejects native templated stdlib wrapper temporary index forms
- rejects native templated stdlib wrapper temporary syntax parity
- rejects native templated stdlib wrapper temporary unsafe parity
- rejects native templated stdlib wrapper temporary count capacity parity

### test_compile_run_native_backend_collections_map_literals_and_string_keys.cpp

- compiles and runs native collection syntax parity expression access forms
- compiles and runs native vector literal count helper
- compiles and runs native collection constructor parity expression access
- rejects native map constructor call access expressions
- compiles and runs native map count helper
- compiles and runs native map method call
- compiles and runs native map at helper
- compiles and runs native map indexing sugar
- compiles and runs native map at_unsafe helper
- compiles and runs native bool map access helpers
- compiles and runs native u64 map access helpers
- compiles and runs native map at missing key
- compiles and runs native typed map binding
- rejects native map constructor odd args
- rejects native map constructor type mismatch
- compiles native string-valued map constructors on stdlib path
- rejects native string-keyed map constructor access expressions
- rejects native map constructor string binding key access expressions
- rejects native string-keyed map indexing sugar
- rejects native string-keyed map indexing binding key
- rejects native map indexing with argv key
- rejects native string-keyed map binding lookup
- rejects native map lookup with argv string key
- rejects native map constructor string key from argv binding

### test_compile_run_native_backend_collections_mutators_and_limits_a.cpp

- rejects native auto-inferred named access helper receiver precedence before lowering
- rejects native auto-inferred std namespaced access helper compatibility alias precedence
- compiles and runs native auto-inferred std namespaced access helper canonical definition
- rejects native user vector pop call shadow on immutable call-form receiver
- compiles and runs native user vector pop method shadow
- compiles and runs native user vector reserve call shadow
- compiles and runs native user vector reserve method shadow
- rejects native user vector clear call shadow on immutable call-form receiver
- compiles and runs native user vector clear method shadow
- rejects native user vector remove_at call shadow on immutable call-form receiver
- compiles and runs native user vector remove_at method shadow
- rejects native user vector remove_swap call shadow on immutable call-form receiver
- compiles and runs native indexed vector assign
- compiles and runs native user vector remove_swap method shadow
- grows native vector reserve beyond initial capacity
- preserves native vector values across reserve growth
- grows native vector push beyond initial capacity
- preserves native vector values across push growth
- compiles and runs native vector literal at local dynamic limit

### test_compile_run_native_backend_collections_mutators_and_limits_b.cpp

- compiles and runs native vector reserve past former local dynamic limit
- rejects native vector literal above local dynamic limit
- rejects native vector reserve beyond local dynamic limit
- compiles and runs native vector push past former local dynamic limit
- rejects native vector reserve negative literal at lowering
- rejects native vector reserve folded expression beyond local dynamic limit
- rejects native vector reserve folded negative expression at lowering
- rejects native vector reserve folded signed overflow at lowering
- rejects native vector reserve folded negate negative at lowering
- rejects native vector reserve folded negate overflow at lowering
- rejects native vector reserve folded unsigned expression beyond local dynamic limit
- rejects native vector reserve folded unsigned wraparound at lowering
- rejects native vector reserve folded unsigned add overflow at lowering
- rejects native vector reserve dynamic value beyond local dynamic limit
- rejects native vector push beyond local dynamic limit
- compiles and runs native vector shrink helpers

### test_compile_run_native_backend_collections_shadow_precedence_and_counts.cpp

- native bare vector mutator methods reject without imported helpers
- compiles and runs native builtin array count before user method shadow
- compiles and runs native builtin array count before user call shadow
- rejects native user map count call shadow without imported canonical helper
- rejects native user map count method shadow without imported canonical helper
- compiles and runs native canonical map sugar with current precedence before compatibility aliases
- rejects native canonical unknown map helper with canonical diagnostics
- rejects native canonical map access string shadow before compatibility aliases during lowering
- compiles and runs native canonical map access non-string shadow before compatibility aliases
- compiles and runs native explicit map helper calls through same-path aliases
- rejects native explicit canonical map helper calls through same-path helpers with current lowering diagnostics
- rejects native map compatibility count call mismatch with canonical templated helper present
- compiles and runs native map compatibility explicit-template count call with canonical templated helper present
- rejects native map compatibility explicit-template count call with non-templated alias helper
- rejects native map compatibility explicit-template count method with non-templated alias helper
- rejects native canonical explicit-template map count call with non-templated canonical helper
- rejects native canonical implicit-template map count call with canonical argument-shape diagnostics
- compiles and runs native canonical implicit-template map count call with wrapper slash return envelope
- compiles and runs native builtin string count before user call shadow
- compiles and runs native user string count method shadow
- rejects native canonical map reference string access without imported canonical helper
- rejects native builtin count on canonical map reference string access without imported helper
- native rejects bare builtin count on wrapper-returned canonical map access before lowering
- compiles and runs native user string count method shadow on wrapper-returned canonical map access

### test_compile_run_native_backend_collections_shims_maps_a.cpp

- rejects native std-namespaced vector method alias access struct method chain with helper receiver diagnostics
- rejects native std-namespaced vector access slash methods without canonical helper on vector receiver
- rejects native std-namespaced vector method alias access struct method chain with helper missing-method diagnostics
- rejects native templated stdlib map wrapper temporary unsafe key mismatch
- rejects templated stdlib map return envelope unsupported key arg
- rejects templated stdlib map return envelope unsupported value arg
- rejects native templated stdlib vector return envelope nested arg
- rejects native templated stdlib map return envelope nested arg
- rejects native templated stdlib vector return envelope wrong arity
- rejects native templated stdlib map return envelope wrong arity
- compiles and runs native stdlib collection shim vector single
- compiles and runs native stdlib collection shim vector new
- rejects native stdlib collection shim vector new type mismatch
- rejects native stdlib collection shim vector single type mismatch
- compiles and runs native stdlib collection shim vector pair
- rejects native stdlib collection shim vector pair type mismatch
- compiles and runs native stdlib collection shim vector triple
- rejects native stdlib collection shim vector triple type mismatch
- compiles and runs native stdlib collection shim vector quad
- rejects native stdlib collection shim vector quad type mismatch
- compiles and runs native stdlib collection shim map single
- rejects native stdlib collection shim map single type mismatch
- rejects native stdlib collection shim map single key type mismatch
- compiles and runs native stdlib collection shim map new
- compiles and runs native stdlib collection shim map new string key envelope
- rejects native stdlib collection shim map new type mismatch
- rejects native stdlib collection shim map new string key type mismatch
- compiles and runs native stdlib collection shim map count
- compiles and runs native stdlib collection shim map count string keys
- rejects native stdlib collection shim map count type mismatch
- rejects native stdlib collection shim map count string key type mismatch

### test_compile_run_native_backend_collections_shims_maps_b.cpp

- rejects retired native stdlib collection shim map at constructor
- rejects retired native stdlib collection shim map at string-key constructor
- rejects native stdlib collection shim map at type mismatch
- rejects native stdlib collection shim map at string key type mismatch
- rejects retired native stdlib collection shim map at unsafe constructor
- rejects retired native stdlib collection shim map at unsafe string-key constructor
- rejects native stdlib collection shim map at unsafe type mismatch
- rejects native stdlib collection shim map at unsafe string key type mismatch
- rejects retired native stdlib collection shim map method access string-key constructor
- rejects native stdlib collection shim map method at string key type mismatch
- rejects native stdlib collection shim map method at unsafe string key type mismatch
- rejects retired native stdlib collection shim map method call parity string-key constructor
- rejects native stdlib collection shim map method call parity key type mismatch
- rejects native stdlib collection shim map method call parity unsafe key type mismatch
- rejects retired native stdlib collection shim map single standalone string keys
- rejects native stdlib collection shim map single standalone key type mismatch
- rejects retired native stdlib collection shim map pair standalone
- rejects native stdlib collection shim map pair standalone type mismatch
- rejects retired native stdlib collection shim map pair standalone string keys
- rejects native stdlib collection shim map pair standalone key type mismatch
- rejects retired native stdlib collection shim map double standalone string keys
- rejects native stdlib collection shim map double standalone key type mismatch
- rejects retired native stdlib collection shim map triple standalone string keys
- rejects native stdlib collection shim map triple standalone key type mismatch
- rejects retired native stdlib collection shim map quad standalone
- rejects retired native stdlib collection shim map quad standalone string keys
- rejects native stdlib collection shim map quad standalone type mismatch
- rejects native stdlib collection shim map quad standalone key type mismatch
- rejects retired native stdlib collection shim map quint standalone
- rejects retired native stdlib collection shim map quint standalone string keys
- rejects native stdlib collection shim map quint standalone type mismatch
- rejects native stdlib collection shim map quint standalone key type mismatch
- rejects retired native stdlib collection shim map sext standalone
- rejects retired native stdlib collection shim map sext standalone string keys
- rejects native stdlib collection shim map sext standalone type mismatch
- rejects native stdlib collection shim map sext standalone key type mismatch

### test_compile_run_native_backend_collections_shims_maps_c.cpp

- rejects retired native stdlib collection shim map sept standalone
- rejects retired native stdlib collection shim map sept standalone string keys
- rejects native stdlib collection shim map sept standalone type mismatch
- rejects native stdlib collection shim map sept standalone key type mismatch
- rejects retired native stdlib collection shim map oct standalone
- rejects retired native stdlib collection shim map oct standalone string keys
- rejects native stdlib collection shim map oct standalone type mismatch
- rejects native stdlib collection shim map oct standalone key type mismatch
- rejects retired native stdlib collection shim map double
- rejects native stdlib collection shim map double type mismatch
- rejects retired native stdlib collection shim map triple
- rejects native stdlib collection shim map triple type mismatch
- rejects retired native stdlib collection shim extended map constructor
- rejects native stdlib collection shim extended constructor type mismatch
- compiles and runs native stdlib collection shim vector quint constructor
- rejects native stdlib collection shim vector quint type mismatch
- compiles and runs native stdlib collection shim vector sext constructor
- rejects native stdlib collection shim vector sext type mismatch
- compiles and runs native stdlib collection shim vector sept constructor
- rejects native stdlib collection shim vector sept type mismatch
- compiles and runs native stdlib collection shim vector oct constructor
- rejects native stdlib collection shim vector oct type mismatch
- rejects retired native stdlib collection shim map pair string keys
- rejects native stdlib collection shim map pair type mismatch
- rejects retired native stdlib collection shim map quad
- rejects native stdlib collection shim map quad type mismatch
- rejects retired native stdlib collection shim map quint
- rejects native stdlib collection shim map quint type mismatch
- rejects retired native stdlib collection shim map sext
- rejects native stdlib collection shim map sext type mismatch
- rejects retired native stdlib collection shim map sept
- rejects native stdlib collection shim map sept type mismatch
- rejects retired native stdlib collection shim map oct
- rejects native stdlib collection shim map oct type mismatch

### test_compile_run_native_backend_collections_shims_vectors.cpp

- compiles and runs native stdlib collection shim access helpers
- compiles and runs native stdlib collection shim capacity helper
- compiles and runs native stdlib collection shim vector capacity
- rejects native stdlib collection shim vector capacity type mismatch
- compiles and runs native stdlib collection shim vector count
- rejects native stdlib collection shim vector count type mismatch
- compiles and runs native stdlib collection shim vector at
- rejects native stdlib collection shim vector at type mismatch
- compiles and runs native stdlib collection shim vector at unsafe
- rejects native stdlib collection shim vector at unsafe type mismatch
- compiles and runs native stdlib collection shim vector push
- rejects native stdlib collection shim vector push type mismatch
- compiles and runs native stdlib collection shim vector pop
- rejects native stdlib collection shim vector pop type mismatch
- compiles and runs native stdlib collection shim vector reserve
- rejects native stdlib collection shim vector reserve type mismatch
- compiles and runs native stdlib collection shim vector clear
- rejects native stdlib collection shim vector clear type mismatch
- compiles and runs native stdlib collection shim vector remove at
- rejects native stdlib collection shim vector remove at type mismatch
- compiles and runs native stdlib collection shim vector remove swap
- rejects native stdlib collection shim vector remove swap type mismatch
- compiles and runs native stdlib collection shim vector mutators
- compiles and runs native bare vector capacity through imported stdlib helper
- rejects native bare vector capacity without imported helper
- compiles and runs native bare vector capacity method without imported helper
- rejects native wrapper temporary vector capacity method without helper
- compiles and runs native bare vector capacity after pop through imported stdlib helper
- native bare vector mutators compile without imported helpers

### test_compile_run_native_backend_collections_user_shadow_methods_a.cpp

- compiles and runs native user array at method shadow
- compiles and runs native user array at_unsafe call shadow
- compiles and runs native user array at_unsafe method shadow
- compiles and runs native user map at_unsafe call shadow
- compiles and runs native user map at_unsafe method shadow
- compiles and runs native user map at call shadow
- compiles and runs native user map at string positional call shadow
- compiles and runs native map access preferring later map receiver over string
- compiles and runs native user map at_unsafe string positional call shadow
- compiles and runs native user map at method shadow
- rejects native user vector at call shadow
- compiles and runs canonical vector discard helpers with owned elements in native backend
- compiles and runs canonical vector indexed removal helpers with owned elements in native backend
- rejects native vector reserve with non-relocation-trivial elements
- rejects native vector constructor with non-relocation-trivial elements
- runs native indexed vector removals with ownership semantics
- rejects native named vector at expression receiver precedence
- compiles and runs native user vector at method shadow
- compiles and runs native user string at_unsafe call shadow
- compiles and runs native user string at_unsafe method shadow
- rejects native user vector at_unsafe call shadow
- compiles and runs native user vector at_unsafe method shadow
- compiles and runs native user string at call shadow
- compiles and runs native user string at method shadow

### test_compile_run_native_backend_collections_user_shadow_methods_b.cpp

- compiles and runs native vector push helper
- compiles and runs native vector mutator method calls
- compiles and runs native user push helper shadow
- compiles and runs native user vector constructor shadow
- compiles and runs native user array constructor shadow
- compiles and runs native user map constructor shadow
- rejects native builtin vector constructor named arguments
- rejects native builtin array constructor named arguments
- rejects native builtin map constructor named arguments
- compiles and runs native namespaced vector count named arguments through imported stdlib helper
- compiles and runs native namespaced vector capacity named arguments through imported stdlib helper
- rejects native removed vector access alias named arguments
- rejects native removed vector access alias at_unsafe named arguments
- rejects native namespaced vector count with soa_vector literal target
- compiles and runs native user map constructor block shadow
- compiles and runs native user vector constructor block shadow
- compiles and runs native user array constructor block shadow
- compiles and runs native user vector push call shadow
- rejects native reordered namespaced vector push call compatibility alias
- compiles and runs native std namespaced reordered mutator compatibility helper shadow
- compiles and runs native user vector push bool positional call shadow
- compiles and runs native user vector push call named shadow
- compiles and runs native user vector push method shadow
- compiles and runs native user vector push call expression shadow
- rejects native reordered namespaced vector push call expression compatibility alias

### test_compile_run_native_backend_collections_vector_alias_rejects_a.cpp

- rejects native array namespaced vector capacity alias
- rejects native array namespaced wrapper vector capacity alias
- rejects native array namespaced wrapper vector count alias
- rejects native array namespaced vector mutator alias
- rejects native stdlib canonical vector helper method-precedence forwarding
- rejects native templated stdlib canonical vector helper method template args
- rejects native vector namespaced call aliases without alias helpers
- rejects native vector namespaced templated canonical helper alias call without alias definition
- rejects native vector alias arity-mismatch compatibility template forwarding
- rejects native vector alias compatibility template forwarding on bool type mismatch
- rejects native vector alias compatibility template forwarding on non-bool type mismatch
- rejects native vector alias compatibility template forwarding on struct type mismatch
- rejects native vector alias compatibility template forwarding on constructor temporary struct mismatch
- rejects native vector alias compatibility template forwarding on method-call temporary struct mismatch
- rejects native vector alias compatibility template forwarding on chained method-call temporary struct mismatch
- rejects native vector alias compatibility template forwarding on array envelope element mismatch
- rejects native vector alias compatibility template forwarding on map envelope mismatch
- rejects native vector alias compatibility template forwarding on map envelope mismatch from call return
- rejects native vector alias compatibility template forwarding on primitive mismatch from call return
- rejects native vector alias compatibility template forwarding when unknown expected meets primitive call return
- rejects native vector alias compatibility template forwarding when unknown expected meets primitive binding
- rejects native vector alias compatibility template forwarding when unknown expected meets vector envelope binding
- rejects native local alias slash-method vector count on string receiver during lowering
- rejects native local alias slash-method vector count on array receiver

### test_compile_run_native_backend_collections_vector_alias_rejects_b.cpp

- rejects native vector helper method expression legacy alias forwarding
- rejects native vector alias named-argument compatibility template forwarding
- rejects native wrapper temporary templated vector method compatibility template forwarding
- rejects native array alias templated forwarding to canonical vector helper
- rejects native stdlib templated vector count fallback to array alias
- compiles and runs native array alias count through same-path helper
- compiles and runs native array alias capacity through same-path helper
- compiles and runs native array alias at through same-path helper
- compiles and runs native array alias at_unsafe through same-path helper
- compiles and runs native array alias slash-method helpers through same-path helpers
- rejects native vector alias templated forwarding past non-templated compatibility helper
- compiles and runs native vector namespaced mutator alias
- rejects native vector namespaced count capacity access aliases without alias helpers
- compiles and runs native vector access checks bounds
- compiles and runs native vector access rejects negative index
- runs native vector literal count method without imported helper
- compiles and runs native vector method call
- rejects native bare vector literal unsafe access through imported stdlib helper
- compiles and runs native bare vector at through imported stdlib helper
- rejects native bare vector at without imported helper
- rejects native bare vector at method without imported helper
- rejects native wrapper temporary vector at method without helper
- compiles and runs native bare vector at_unsafe through imported stdlib helper
- rejects native bare vector at_unsafe without imported helper
- rejects native bare vector at_unsafe method without imported helper
- rejects native wrapper temporary vector at_unsafe method without helper
- compiles and runs native bare vector count through imported stdlib helper
- rejects native bare vector count without imported helper
- rejects native bare vector count method without imported helper
- rejects native wrapper temporary vector count method without helper
- rejects native wrapper vector count slash-method chains before receiver typing
- rejects native wrapper vector capacity slash-method chains before receiver typing
- rejects native local alias slash-method vector capacity on string receiver
- rejects native local alias slash-method vector capacity on array receiver
- compiles and runs native stdlib collection shim helpers
- compiles and runs native stdlib collection shim multi constructors
- compiles and runs native templated stdlib collection return envelopes

### test_compile_run_native_backend_collections_wrapper_rejects_maps.cpp

- rejects native templated stdlib collection return envelope unsupported arg
- rejects native templated stdlib map wrapper temporary key mismatch
- rejects native templated stdlib map wrapper temporary index key mismatch
- rejects native templated stdlib wrapper temporary syntax parity key mismatch
- rejects native templated stdlib wrapper temporary syntax parity value mismatch
- rejects native templated stdlib wrapper temporary unsafe parity mismatch
- rejects native templated stdlib wrapper temporary unsafe parity value mismatch
- rejects native templated stdlib wrapper temporary unsafe parity arity mismatch
- rejects native templated stdlib wrapper temporary unsafe parity missing arguments
- rejects native templated stdlib wrapper temporary count capacity parity mismatch
- rejects native templated stdlib map wrapper temporary index value mismatch
- rejects native templated stdlib map wrapper temporary call key mismatch
- rejects native templated stdlib map wrapper temporary call value mismatch
- rejects native templated stdlib map wrapper temporary unsafe call key mismatch
- rejects native templated stdlib map wrapper temporary unsafe call value mismatch
- rejects native templated stdlib map wrapper temporary count key mismatch
- rejects native templated stdlib map wrapper temporary count value mismatch
- rejects native templated stdlib map wrapper temporary call arity mismatch
- rejects native templated stdlib map wrapper temporary call missing key argument
- rejects native templated stdlib map wrapper temporary unsafe call arity mismatch
- rejects native templated stdlib map wrapper temporary unsafe call missing key argument
- rejects native templated stdlib map wrapper temporary count call arity mismatch
- rejects native templated stdlib map wrapper temporary count method arity mismatch
- rejects native templated stdlib map wrapper temporary method arity mismatch
- rejects native templated stdlib map wrapper temporary method missing key argument
- rejects native templated stdlib map wrapper temporary unsafe method arity mismatch
- rejects native templated stdlib map wrapper temporary unsafe method missing key argument

### test_compile_run_native_backend_collections_wrapper_rejects_vectors.cpp

- rejects native templated stdlib vector wrapper temporary call type mismatch
- rejects native templated stdlib vector wrapper temporary call index mismatch
- rejects native templated stdlib vector wrapper temporary call arity mismatch
- rejects native templated stdlib vector wrapper temporary call missing index
- rejects native templated stdlib vector wrapper temporary unsafe call type mismatch
- rejects native templated stdlib vector wrapper temporary unsafe call index mismatch
- rejects native templated stdlib vector wrapper temporary unsafe call arity mismatch
- rejects native templated stdlib vector wrapper temporary unsafe call missing index
- rejects native templated stdlib vector wrapper temporary count type mismatch
- rejects native templated stdlib vector wrapper temporary count call arity mismatch
- rejects native templated stdlib vector wrapper temporary count method arity mismatch
- rejects native templated stdlib vector wrapper temporary method arity mismatch
- rejects native templated stdlib vector wrapper temporary method missing index
- rejects native templated stdlib vector wrapper temporary unsafe method arity mismatch
- rejects native templated stdlib vector wrapper temporary unsafe method missing index
- rejects native templated stdlib vector wrapper temporary capacity type mismatch
- rejects native templated stdlib vector wrapper temporary capacity call arity mismatch
- rejects native templated stdlib vector wrapper temporary capacity method arity mismatch
- rejects native templated stdlib vector wrapper temporary method index mismatch
- rejects native templated stdlib vector wrapper temporary index mismatch
- rejects native templated stdlib vector wrapper temporary index value mismatch

### test_compile_run_native_backend_collections_wrapper_shadow_precedence.cpp

- compiles and runs native user wrapper temporary at_unsafe shadow precedence
- rejects native user wrapper temporary unsafe parity shadow mismatch
- rejects native user wrapper temporary unsafe parity shadow value mismatch
- rejects native user wrapper temporary unsafe parity shadow arity mismatch
- rejects native user wrapper temporary unsafe parity shadow missing arguments
- compiles and runs native user wrapper temporary at shadow precedence
- compiles and runs native user wrapper temporary count capacity shadow precedence
- rejects native user wrapper temporary count capacity shadow value mismatch
- compiles and runs native user wrapper temporary index shadow precedence
- compiles and runs native user wrapper temporary syntax parity shadow precedence
- rejects native user wrapper temporary syntax parity shadow mismatch
- rejects native user wrapper temporary syntax parity shadow value mismatch

### test_compile_run_native_backend_control.cpp

- compiles and runs native void executable
- compiles and runs native explicit void return
- compiles and runs native locals
- compiles and runs native if/else
- compiles and runs native repeat loop
- compiles and runs native for binding condition
- compiles and runs native shared_scope for binding condition
- compiles and runs native shared_scope while loop
- compiles and runs native pointer helpers
- compiles and runs native pointer plus
- compiles and runs native print output
- default effects token enables io output
- default entry effects enable io output
- entry defaults apply to helpers
- default effects token does not enable io_err output
- default effects token enables vm output
- default effects allow capabilities in native
- default effects none requires explicit effects
- compiles and runs native implicit utf8 strings
- compiles and runs native implicit utf8 single-quoted strings
- compiles and runs native escaped utf8 strings
- compiles and runs native raw utf8 single-quoted strings
- compiles and runs native string binding print
- compiles and runs native raw string literal output
- compiles and runs native raw single-quoted string output
- compiles and runs native string binding copy
- compiles and runs native string count and indexing
- compiles and runs native string access checks bounds
- compiles and runs native string access rejects negative index

### test_compile_run_native_backend_core_array_and_pointer_variadics.cpp

- native materializes variadic array packs with indexed count methods
- native materializes variadic borrowed array packs with indexed count methods
- native rejects variadic borrowed array packs with indexed dereference access helpers
- native materializes variadic pointer array packs with indexed count methods
- native rejects variadic pointer array packs with indexed dereference access helpers
- native materializes variadic scalar reference packs with indexed dereference
- native materializes variadic struct reference packs with indexed field and helper access
- native materializes variadic struct pointer packs with indexed field and helper access
- native materializes variadic scalar pointer packs with indexed dereference
- native rejects variadic scalar pointer packs from borrowed pack access

### test_compile_run_native_backend_core_buffer_and_collection_wrappers.cpp

- native materializes variadic Buffer packs with indexed count helpers
- native forwards variadic Reference<Buffer> packs through location/dereference
- native materializes variadic Pointer<Buffer> packs with dereference helpers
- native preserves if expression values in arithmetic context
- compiles and runs native float arithmetic
- compiles and runs native image api contract deterministically
- native keeps std image user helpers distinct from builtin aliases
- native uses stdlib ImageError result helpers
- native uses stdlib ImageError why wrapper
- native uses stdlib ImageError constructor wrappers
- native uses stdlib GfxError result helpers
- native uses canonical stdlib GfxError result helpers
- native uses canonical stdlib GfxError why helpers
- native uses canonical stdlib GfxError constructors
- native uses stdlib experimental Buffer helper methods
- native uses canonical stdlib Buffer helper methods
- native uses stdlib experimental Buffer readback helpers
- native uses canonical stdlib Buffer readback helpers
- native uses stdlib experimental Buffer allocation helpers
- native uses canonical stdlib Buffer allocation helpers
- native uses stdlib experimental Buffer allocation readback path
- native uses canonical stdlib Buffer allocation readback path

### test_compile_run_native_backend_core_error_and_file_variadics.cpp

- native backend supports graphics-style int return propagation with on_error
- native backend supports string Result.ok payloads through try
- native backend supports direct string Result combinator consumers
- native backend supports definition-backed string Result combinator sources
- compiles and runs native FileError.why mapping
- native uses stdlib FileError why wrapper
- native uses stdlib FileError eof wrapper
- native uses stdlib FileError eof constructor wrapper
- native materializes variadic FileError packs with indexed why methods
- native materializes variadic borrowed FileError packs with indexed dereference why methods
- native materializes variadic pointer FileError packs with indexed dereference why methods
- native materializes variadic wrapped FileError packs with named free builtin at receivers
- native materializes variadic File handle packs with indexed file methods
- native materializes variadic borrowed File handle packs with indexed dereference file methods
- native materializes variadic pointer File handle packs with indexed dereference file methods

### test_compile_run_native_backend_core_file_and_struct_variadics.cpp

- compiles and runs native file io
- compiles and runs native file read_byte with deterministic eof
- native uses stdlib File helper wrappers
- native uses stdlib File open helper wrappers
- native stdlib File close helper disarms the original handle
- native uses stdlib File string helper wrappers
- native uses stdlib File helper wrappers and broader fallback arities
- native resolves templated helper overload families by exact arity
- compiles and runs native mutable scalar helper copy-back
- compiles and runs native executable
- native supports support-matrix binding types
- native materializes direct variadic args packs
- native materializes string variadic args packs and pure spread forwarding
- native materializes mixed explicit and spread variadic forwarding
- native materializes direct struct variadic args packs for count
- native materializes pure spread struct variadic packs for count
- native materializes mixed struct spread variadic forwarding for count
- native materializes direct struct variadic pack indexing and method access
- native lowers compile-time type pack index get helper
- native uses imported stdlib tuple get helpers
- native uses tuple bracket indexing sugar
- native reports tuple bracket index diagnostics
- native infers heterogeneous stdlib tuple make_tuple
- native reports stdlib tuple make_tuple diagnostics
- native destructures stdlib tuple values
- native reports tuple destructuring diagnostics
- native compiles empty and borrowed stdlib tuple access
- native reports stdlib tuple get index diagnostics

### test_compile_run_native_backend_core_image_io_a.cpp

- native uses stdlib experimental Buffer upload helpers
- native uses canonical stdlib Buffer upload helpers
- compiles and runs native ppm read for ascii p3 inputs
- compiles and runs native ppm read for binary p6 inputs
- compiles and rejects truncated native binary ppm reads deterministically
- compiles and rejects oversized native image read dimensions before overflow
- compiles and runs native ppm write for ascii p3 outputs
- compiles and rejects invalid native ppm write inputs deterministically
- compiles and runs native png write for deterministic rgb outputs
- compiles and rejects invalid native png write inputs deterministically
- rejects oversized native image write dimensions before overflow
- compiles and runs native png read for stored rgb inputs
- compiles and runs native png read for sub-filter grayscale inputs

### test_compile_run_native_backend_core_image_io_b.cpp

- compiles and runs native png read for sub-filter grayscale-alpha inputs
- compiles and runs native png read for 1-bit grayscale inputs
- compiles and runs native png read for 4-bit grayscale inputs
- compiles and runs native png read for 16-bit grayscale inputs
- compiles and runs native png read for stored sub-filter rgb inputs
- compiles and runs native png read for stored up-filter rgb inputs
- compiles and runs native png read for stored average-filter rgb inputs
- compiles and runs native png read for stored paeth-filter rgb inputs

### test_compile_run_native_backend_core_image_io_c.cpp

- compiles and runs native png read for stored paeth-filter rgba inputs
- compiles and runs native png read for fixed-huffman backreference rgb inputs
- compiles and runs native png read for dynamic-huffman literal rgb inputs
- compiles and runs native png read for dynamic-huffman backreference rgb inputs
- compiles and rejects malformed native png inputs deterministically
- compiles and runs native png read for sub-filter indexed-color inputs
- compiles and rejects indexed native png inputs with out-of-range palette indexes
- compiles and runs native png read for 2-bit indexed-color inputs
- compiles and rejects 1-bit indexed native png inputs with out-of-range palette indexes

### test_compile_run_native_backend_core_image_io_d.cpp

- compiles and runs native png read for 16-bit rgb inputs
- compiles and runs native png read for 16-bit rgba inputs
- compiles and runs native png read for 16-bit grayscale-alpha inputs
- compiles and runs native png read for Adam7 interlaced rgb inputs
- compiles and runs native png read for Adam7 interlaced indexed-color inputs
- compiles and rejects malformed Adam7 interlaced native png inputs
- compiles and runs native png read for optional plte and split idat inputs

### test_compile_run_native_backend_core_map_and_vector_variadics.cpp

- native rejects variadic borrowed map packs with indexed count methods
- native rejects variadic borrowed map packs with indexed dereference count methods
- native rejects variadic borrowed map packs with indexed dereference lookup helpers
- native rejects variadic borrowed map packs with indexed tryAt inference
- native rejects variadic pointer map packs with indexed count methods
- native rejects variadic pointer map packs with indexed dereference count methods
- native rejects variadic pointer map packs with indexed lookup helpers
- native rejects variadic pointer map packs with indexed dereference lookup helpers
- native rejects variadic pointer map packs with indexed tryAt inference
- native materializes variadic pointer vector packs with indexed count methods
- native materializes variadic pointer vector packs with indexed dereference capacity methods
- native runs vector constructor parity with canonical paths
- native rejects map constructor literal parity with canonical entries

### test_compile_run_native_backend_core_reference_and_uninitialized_variadics.cpp

- native rejects variadic struct pointer packs from borrowed pack access
- native rejects variadic scalar reference packs from borrowed pack access
- native rejects variadic struct reference packs from borrowed pack access
- native rejects variadic scalar pointer packs from borrowed pack field access
- native rejects variadic struct pointer packs from borrowed pack field access
- native rejects variadic scalar pointer packs from borrowed pack reference fields
- native rejects variadic struct pointer packs from borrowed pack reference fields
- native materializes variadic pointer uninitialized scalar packs with indexed init and take
- native materializes variadic borrowed uninitialized scalar packs with indexed init and take

### test_compile_run_native_backend_core_result_and_vector_variadics.cpp

- native materializes pure spread struct pack indexing and method access
- native materializes mixed struct pack indexing and method access
- native materializes variadic Result packs with indexed why and try access
- native materializes variadic borrowed Result packs with indexed dereference try and why access
- native materializes variadic pointer Result packs with indexed dereference try and why access
- native materializes variadic status-only Result packs with indexed error and why access
- native materializes variadic borrowed status-only Result packs with indexed dereference error and why access
- native materializes variadic pointer status-only Result packs with indexed dereference error and why access
- native materializes variadic vector packs with indexed count methods
- native materializes variadic vector packs with indexed capacity methods
- native materializes variadic vector packs with indexed statement mutators

### test_compile_run_native_backend_core_result_payloads_and_strings.cpp

- native backend supports imported stdlib Result sum pick
- native backend supports Result.error on imported stdlib Result sum
- native backend supports Result.why on imported stdlib Result sum
- native backend supports Result.ok compatibility on imported stdlib Result sum
- native backend supports Result.map compatibility on imported stdlib Result sum
- native backend supports Result.and_then compatibility on imported stdlib Result sum
- native backend supports Result.map2 compatibility on imported stdlib Result sum
- native backend supports direct stdlib Result sum sources in compatibility combinators
- native backend compiles packed error struct Result combinator payloads on IR-backed paths
- native backend supports direct single-slot struct Result.ok payloads on IR-backed paths
- native backend supports single-slot struct Result combinator payloads on IR-backed paths
- native backend supports direct File Result payloads on IR-backed paths
- native backend supports packed File Result combinator payloads on IR-backed paths
- native backend compiles multi-slot struct Result payloads on IR-backed paths
- native backend compiles direct array Result payloads on IR-backed paths
- native backend supports block-bodied Result.and_then lambdas on IR-backed paths
- native backend supports final-if Result.and_then lambdas on IR-backed paths
- native backend compiles direct map Result payloads on IR-backed paths
- native backend compiles Buffer Result payloads on IR-backed paths
- native backend supports auto-bound direct Result combinator try consumers
- compiles and runs native direct type namespace string helpers
- native backend supports try on imported stdlib Result sum ok
- native backend supports try on imported stdlib Result sum error
- native backend supports postfix question on direct imported stdlib Result sum ok
- native backend supports postfix question on direct imported stdlib Result sum error
- native backend supports dereferenced borrowed stdlib Result sum helpers
- native backend propagates imported stdlib Result sum try ok through Result return
- native backend propagates imported stdlib Result sum try error through Result return

### test_compile_run_native_backend_core_runtime_and_ir_paths.cpp

- compiles and rejects native png inputs with critical chunk crc mismatches
- compiles and rejects native png inputs with plte after idat
- compiles and runs if expression in native backend
- compiles and runs match cases in native backend
- compiles and runs native definition call
- native backend runs single task spawn wait
- native backend returns stdlib tuple from multi task wait
- compiles and runs native method call
- compiles and runs native method count call
- compiles and runs native literal method call
- compiles and runs native chained method calls
- compiles and runs native Result.why hooks
- native supports stdlib FileError result helpers
- native backend supports Result.map on IR-backed path
- native backend supports Result.and_then on IR-backed path
- native backend supports Result.map2 on IR-backed path
- native backend supports f32 Result payloads on IR-backed paths
- native backend supports direct Result.ok combinator sources on IR-backed paths
- native backend compiles direct packed ContainerError and ImageError Result payloads on IR-backed paths

### test_compile_run_native_backend_core_ui_layout_a.cpp

- compiles and runs native void call with string param
- compiles and runs native string indexing
- compiles and runs native string parameter indexing
- compiles and runs native software renderer command serialization deterministically
- compiles and runs native software renderer clip stack serialization deterministically
- compiles and runs native two-pass layout tree serialization deterministically
- compiles and runs native two-pass layout empty root deterministically
- compiles and runs native basic widget controls through layout deterministically
- compiles and runs native panel container widget deterministically
- compiles and runs native empty panel container stays balanced deterministically

### test_compile_run_native_backend_core_ui_layout_b.cpp

- compiles and runs native scene model authoring deterministically
- compiles and runs native ui scene adapter deterministically
- compiles and runs native composite login form deterministically
- compiles and runs native html adapter login form deterministically
- compiles and runs native ui event stream deterministically
- compiles and runs native ui ime event stream deterministically
- compiles and runs native ui resize and focus event stream deterministically
- compiles and runs native large frame
- rejects native recursive calls
- native accepts string pointers
- native rejects variadic pointer string packs
- native rejects variadic reference string packs
- native ignores top-level executions

### test_compile_run_native_backend_core_vector_and_experimental_map_variadics.cpp

- native rejects variadic pointer vector packs with expression access helpers
- native rejects variadic pointer vector packs with indexed dereference statement mutators
- native rejects variadic borrowed vector packs with indexed count methods
- native rejects variadic borrowed vector packs with indexed dereference capacity methods
- native rejects variadic borrowed vector packs with indexed dereference access helpers
- native rejects variadic borrowed vector packs with indexed dereference statement mutators
- native rejects variadic borrowed soa_vector packs with indexed count methods
- native rejects variadic pointer soa_vector packs with indexed count methods
- native rejects variadic soa_vector packs with indexed count methods
- native rejects variadic map packs with indexed count methods
- native rejects variadic map packs with indexed tryAt inference
- native rejects variadic experimental map packs with indexed canonical count calls

### test_compile_run_native_backend_imports.cpp

- rejects unsupported effects in native backend
- rejects unsupported default effects in native backend
- rejects unsupported default effects in vm backend
- rejects unsupported execution effects in native backend
- rejects unsupported execution effects in vm backend
- accepts vm support-matrix effects
- accepts native support-matrix effects
- rejects vm support-matrix effect outside allowlist
- rejects native support-matrix effect outside allowlist
- compiles and runs namespace entry
- compiles and runs native import alias in namespace
- compiles and runs native import alias
- compiles and runs native with multiple imports
- compiles and runs import expansion
- rejects legacy include expansion alias
- emit-diagnostics reports legacy include alias rejection payload
- compiles and runs single-quoted import expansion
- compiles and runs with duplicate imports ignored
- rejects import path with suffix
- compiles and runs unquoted import expansion
- compiles and runs unquoted import expansion with -I
- legacy include-path alias is rejected in primec and primevm
- emit-diagnostics reports argument payload for removed include-path option
- compiles and runs versioned import expansion
- compiles and runs versioned import with version first

### test_compile_run_native_backend_math_numeric.cpp

- compiles and runs native clamp
- compiles and runs native clamp i64
- compiles and runs native math abs/sign/min/max
- compiles and runs native qualified math names
- compiles and runs native math saturate/lerp
- compiles and runs native math clamp
- compiles and runs native math pow
- compiles and runs native math pow rejects negative exponent
- compiles and runs native math constant conversions
- compiles and runs native math constants
- compiles and runs native math predicates
- compiles and runs native math rounding
- compiles and runs native math roots
- compiles and runs native math fma/hypot
- compiles and runs native math copysign
- compiles and runs native math angle helpers
- compiles and runs native math trig helpers
- compiles and runs native sin range reduction
- compiles and runs native sin pi accuracy
- compiles and runs native math arc trig helpers
- compiles and runs native math exp/log
- compiles and runs native math hyperbolic
- native explicit math imports currently reject unsupported stdlib helper returns
- compiles and runs native float pow
- compiles and runs native i64 arithmetic
- compiles and runs native u64 division
- compiles and runs native u64 comparison
- compiles and runs native bool return
- compiles and runs native bool comparison with i32
- compiles and runs native implicit void main
- compiles and runs native boolean ops
- compiles and runs native numeric boolean ops
- compiles and runs native short-circuit and
- compiles and runs native short-circuit or

### test_compile_run_native_backend_math_numeric_types.h

- compiles and runs native convert
- compiles and runs native convert bool
- compiles and runs native convert bool from integers
- compiles and runs native convert i64
- compiles and runs native convert u64
- compiles and runs native integer width convert
- compiles and runs native float literals
- compiles and runs native float bindings
- rejects native non-literal string bindings
- rejects native software numeric types
- compiles and runs native support-matrix math nominal helpers
- compiles and runs native quaternion reference multiply and rotation
- compiles and runs native matrix composition order references with tolerance
- compiles and runs native matrix arithmetic helpers with tolerance
- compiles and runs native quaternion arithmetic helpers with tolerance
- rejects native support-matrix plus mismatch diagnostic
- rejects native support-matrix implicit conversion diagnostic

### test_compile_run_native_backend_maybe.cpp

- compiles and runs native Maybe some and pick
- compiles and runs native Maybe none and helper methods
- compiles and runs native Maybe present variant payload
- rejects retired native Maybe mutable helpers with migration diagnostics

### test_compile_run_native_backend_pointers.cpp

- compiles and runs native hello world example
- compiles and runs native pointer plus offsets
- compiles and runs native pointer plus on reference
- compiles and runs native pointer minus offsets
- compiles and runs native pointer minus u64 offsets
- compiles and runs native pointer minus negative i64 offsets
- compiles and runs native pointer plus u64 offsets
- compiles and runs native pointer plus negative i64 offsets
- compiles and runs native references
- compiles and runs native location on reference
- compiles and runs native heap alloc intrinsic
- compiles and runs native heap free intrinsic
- compiles and runs native heap realloc intrinsic
- compiles and runs native checked memory at intrinsic
- compiles and runs native unchecked memory at intrinsic
- compiles and runs native reference arithmetic

### test_compile_run_native_backend_uninitialized.cpp

- compiles and runs native uninitialized local storage
- compiles and runs native uninitialized string storage
- compiles and runs native pointer-backed uninitialized storage
- compiles and runs native reference-backed uninitialized storage
- compiles and runs native pointer-backed uninitialized struct storage
- compiles and runs native uninitialized struct field

## compile_run/root (204 tests, 6 files)

### test_compile_run_benchmark_harness.cpp

- benchmark baseline artifact includes native allocator coverage
- semantic memory benchmark fixtures are checked in
- semantic memory benchmark helper keeps primary fixture first
- semantic memory primary fixture stays minimal math-star reproducer
- semantic memory non-math include fixture keeps comparable definition scale
- semantic memory baseline report is checked in with fixture phase coverage
- semantic memory ctest targets keep dependency ordering and serialization
- semantic memory budget policy artifacts are checked in
- semantic memory semantic-product index parity evidence artifact is checked in
- semantic memory semantic-product index measured report artifact is checked in
- semantic memory phase-one success criteria artifacts are checked in
- semantic memory benchmark helper accepts benchmark-only collector controls
- semantic memory benchmark helper supports validator-vs-fact A/B mode
- semantic memory benchmark helper supports semantic-product-force A/B mode
- semantic memory benchmark helper keeps memoization deltas mode-scoped
- semantic memory benchmark helper keeps key-mode deltas force-scoped
- semantic memory benchmark helper keeps side-channel deltas force-scoped
- semantic memory benchmark helper keeps dependency deltas force-scoped
- semantic memory benchmark helper toggles fact families independently
- semantic memory benchmark helper reports deterministic worker-mode parity
- semantic memory benchmark helper fails worker parity on fact-family drift
- semantic memory benchmark helper canonicalizes legacy all fact-family rows
- semantic memory benchmark helper canonicalizes blank force mode rows
- semantic memory benchmark helper canonicalizes legacy no-fact rows
- semantic memory benchmark helper canonicalizes mixed-case legacy mode rows
- semantic memory benchmark helper canonicalizes null no-fact rows
- semantic memory benchmark helper canonicalizes null legacy mode rows
- semantic memory benchmark helper canonicalizes boolean-like mode rows
- semantic memory benchmark helper emits wall-rss machine report rows
- semantic memory benchmark helper emits key-cardinality report fields
- semantic memory benchmark helper counts semantic-product index families from dumps
- semantic memory benchmark helper reports semantic-product index family parity across definition workers
- semantic memory benchmark helper defaults to three runs with median-worst rollups
- semantic memory benchmark helper covers no-import and math-vector phases
- semantic memory benchmark helper covers math-vector-matrix and math-star phases
- semantic memory benchmark helper covers inline-vs-import math fixture phases
- semantic memory benchmark helper reports 1x-2x-4x scale slopes
- semantic memory benchmark required fixture-phase matrix fails when tuples are missing
- semantic memory benchmark helper defines method-target memoization delta report fields
- benchmark regression checker passes for in-threshold report
- benchmark regression checker fails for threshold regression
- semantic memory budget checker passes for in-budget report
- semantic memory budget checker fails when report tuple lacks policy entry
- semantic memory budget checker fails for sustained regressions
- semantic memory phase-one checker passes for current and sustained window
- semantic memory phase-one checker fails when sustained window misses target
- semantic memory trend checker passes without history reports
- semantic memory trend checker writes trend summary report
- semantic memory trend checker fails for sustained regressions from history dir
- semantic memory trend checker ignores duplicate current report in history dir
- semantic memory ci artifact wrapper forwards definition worker mode
- semantic memory ci artifact wrapper captures reports on success
- semantic memory ci artifact wrapper benchmark mode runs budget gate
- semantic memory ci artifact wrapper benchmark mode can skip budget gate
- semantic memory ci artifact wrapper benchmark mode fails on budget gate
- semantic memory ci artifact wrapper writes failure artifacts
- semantic memory ci artifact wrapper ignores stale reports on benchmark failure
- semantic benchmark plumbing keeps production validate surface narrow
- tsan semantics smoke is gated behind optional-ci wiring

### test_compile_run_generic_requirements.cpp

- generic same-type and value requirements execute across backends
- constrained overload selection executes the only viable candidate
- selected ct_if branches execute and discarded branches stay inert
- direct requirement failures include call and predicate provenance
- requirement diagnostics improve unconstrained generic failures
- constrained overload diagnostics cover ambiguity and value failures
- compile-time effect rejections surface through primec diagnostics
- runtime count contract admits callers it cannot prove statically
- runtime count contract failure exits with the contract message
- runtime integer parameter contracts check each call boundary
- runtime contracts stay rejected on the program entry definition
- non-checkable runtime require operands stay compile-time errors

### test_compile_run_glsl.cpp

- glsl emitter writes minimal shader
- glsl-ir emitter writes IR-lowered shader for integer subset
- glsl-ir emitter writes IR-lowered shader for f32 literal subset
- glsl emitter uses ir backend for f32 literal subset
- glsl-ir emitter writes IR-lowered shader for f32 arithmetic subset
- glsl emitter uses ir backend for f32 arithmetic subset
- glsl-ir emitter writes IR-lowered shader for f32 to i32 conversion subset
- glsl emitter uses ir backend for f32 to i32 conversion subset
- glsl-ir emitter writes IR-lowered shader for i32 to f32 conversion subset
- glsl emitter uses ir backend for i32 to f32 conversion subset
- glsl-ir emitter writes IR-lowered shader for f32 return subset
- glsl emitter uses ir backend for f32 return subset
- glsl-ir emitter writes IR-lowered shader for helper-call subset
- glsl emitter uses ir backend for helper-call subset
- glsl-ir emitter writes IR-lowered shader for entry args count subset
- glsl emitter uses ir backend for entry args count subset
- glsl emitter matches glsl-ir on shared corpus
- glsl-ir validation rejects out-of-range i64 literals
- glsl emitter surfaces ir validation-stage failures without fallback
- defaults to glsl extension for emit=glsl
- defaults to spv extension for emit=spirv
- glsl emitter allows entry args parameter
- glsl emitter writes spirv when tool available
- spirv-ir emitter writes spirv when tool available
- spirv emit reports missing tool
- glsl emitter writes locals and arithmetic
- spirv emitter surfaces ir validation-stage failures without fallback
- glsl emitter allows assign in expressions
- glsl emitter allows increment/decrement in expressions
- glsl emitter writes if blocks
- glsl emitter writes loops
- glsl emitter handles shared_scope blocks
- glsl emitter handles shared_scope while
- glsl emitter handles block initializers
- glsl emitter handles brace constructor values
- glsl emitter ignores print builtins
- glsl emitter accepts capabilities
- glsl emitter accepts support-matrix effects and capabilities
- glsl emitter supports support-matrix scalar bindings
- glsl emitter lowers quaternion nominal values and quaternion operators
- glsl emitter lowers quaternion conversion helpers
- glsl emitter surfaces quaternion shape diagnostics
- glsl emitter lowers matrix nominal values field access and matrix operators
- glsl emitter accepts vector nominal values and matrix vector multiply
- glsl emitter lowers documented vector arithmetic operators
- glsl emitter handles math constants
- glsl emitter writes integer pow helper
- glsl emitter handles block expressions in arguments
- glsl emitter handles block expression return value
- glsl emitter handles block expression early return
- glsl emitter ignores pathspace builtins
- glsl emitter writes math builtins
- glsl emitter rejects explicit effects
- glsl emitter rejects static bindings
- glsl emitter rejects non-scalar bindings
- glsl emitter rejects mixed signed/unsigned math
- glsl emitter rejects mixed boolean comparisons
- glsl emitter rejects string literals
- glsl emitter rejects unsupported capabilities
- glsl emitter rejects effects on executions
- glsl emitter rejects non-void entry

### test_compile_run_math_conformance.cpp

- math conformance reference printer script
- math conformance PrimeStructc policy docs
- math conformance labeled output allowlist
- math conformance sign and range helpers
- math conformance batch emit helper
- math conformance constants
- math conformance exp log basics
- math conformance roots
- math conformance trig quadrants axes
- math conformance trig quadrants symmetries
- math conformance trig quadrants range reduction
- math conformance float helpers parse tokens
- math conformance float helpers compare tolerance
- math conformance trig basics
- math conformance inverse trig and atan2
- math conformance hyperbolic
- math conformance inverse hyperbolic
- math conformance exp and log
- math conformance exp/log domains
- math conformance float64 basics
- math conformance float64 inverse trig and logs
- math conformance float64 hyperbolic
- math conformance float64 grids
- math conformance float64 rounding
- math conformance roots and pow
- math conformance rounding
- math conformance misc ops
- math conformance stress grid
- math conformance float baseline trigonometric samples
- math conformance float baseline transcendental samples
- math conformance float baseline composition samples
- math conformance float grid sin
- math conformance float grid cos
- math conformance float grid exp_log
- math conformance float grid hypot
- math conformance native approximation limits
- math conformance heavy trig workload
- math conformance heavy exp log workload
- math conformance array math usage
- math conformance dense grids
- math conformance deterministic samples
- math conformance deterministic exp/log samples
- math conformance fixed seed samples
- math conformance conversions and comparisons
- math conformance convert non-finite float to int
- math conformance policy behavior
- math conformance integer pow negative exponent
- math conformance integer edge cases
- math conformance atan2 edges

### test_compile_run_reflection_codegen.cpp

- reflection codegen ast-semantic dump uses canonical helper order
- reflection codegen ir dump keeps generated helper call sites
- reflection compare helper appears in ast-semantic and ir dumps
- reflection hash64 helper appears in ast-semantic and ir dumps
- reflection compare helper rejects unsupported field envelope deterministically
- reflection hash64 helper rejects unsupported field envelope deterministically
- reflection clear helper appears in ast-semantic and ir dumps
- reflection copyfrom helper appears in ast-semantic and ir dumps
- reflection validate helper appears in ast-semantic and ir dumps
- reflection validate helper rejects field hook collisions deterministically
- reflection serialize and deserialize helpers appear in ast-semantic and ir dumps
- reflection serialize helper rejects unsupported field envelope deterministically
- reflection deserialize helper rejects unsupported field envelope deterministically
- reflection ToString generator reports deferred diagnostic deterministically

### test_compile_run_reflection_codegen_runtime.cpp

- reflection codegen helper runtime stays aligned across backends
- reflection compare helper runtime stays aligned across backends
- reflection hash64 helper runtime stays aligned across backends
- reflection clear helper runtime stays aligned across backends
- reflection validate helper runtime stays aligned across backends
- reflection serde helper runtime stays aligned across backends
- reflection SoaSchema helper runtime stays aligned across backends
- reflection SoaSchema chunk helper runtime stays aligned across backends
- reflection SoaSchema storage helper runtime stays aligned across backends

## compile_run/smoke (177 tests, 16 files)

### test_compile_run_smoke_argv.cpp

- compiles and runs binding inference from if expression feeding method call
- compiles and runs binding inference from mixed if branches
- compiles and runs parameter inferring i64 from default initializer
- compiles and runs map constructor preserving assignment value
- C++ emitter array access checks bounds
- C++ emitter string access checks bounds
- runs program in vm
- runs vm with string count and indexing
- runs vm with string literal count
- runs vm with string literal method count
- runs primevm with argv count
- vm string access checks bounds
- vm string access rejects negative index
- runs vm with argv printing
- runs vm with argv printing without newline
- runs vm with forwarded argv
- runs vm with argv count helper
- runs vm with argv i64 index
- runs vm with argv u64 index
- runs vm with argv error output
- runs vm with argv error output without newline
- runs vm with argv error output u64 index
- runs vm with argv line error output u64 index
- runs vm with argv unsafe error output
- runs vm with argv unsafe line error output

### test_compile_run_smoke_collective.cpp

- compiles and runs import after definitions
- rejects import alias for nested definitions
- compiles and runs fully-qualified nested call
- compiles and runs method call with fully-qualified definition
- compiles and runs repeat with bool count
- compiles and runs repeat with non-positive count
- compiles and runs pathspace builtins as no-ops
- compiles and runs binding without explicit type
- compiles and runs binding inferring i64
- compiles and runs binding inferring u64
- compiles and runs binding inferring array type
- compiles and runs array bracket sugar
- compiles and runs indexing into array bracket literal
- compiles and runs binding inferring map type
- compiles and runs map count
- compiles and runs map indexing
- compiles and runs map indexing with u64 keys
- compiles and runs string-keyed map indexing in C++ emitter
- string-keyed map indexing checks missing key in C++ emitter
- map indexing checks missing key
- map indexing rejects mismatched key type in vm/native
- map bindings reject unsupported key type in vm/native
- compiles and runs binding inference feeding method call

### test_compile_run_smoke_core_basic.cpp

- compiles and runs simple main
- compiles and runs float arithmetic in VM
- compiles and runs primitive brace constructors
- default entry path is main
- enum value access lowers across backends
- scalar sum construction and pick lower across backends
- procedural generic local generated struct lowers across backends
- unit sum construction and pick lower across backends
- aggregate sum payloads bind only the active pick branch
- aggregate sum pick results copy active payloads before escape
- sum moves route helpers through the active payload only
- sum drops route destroy through the active payload only
- nested sum payloads compile through VM lowering

### test_compile_run_smoke_core_contracts_and_cli.cpp

- graphics api contract doc-linked constraints stay locked
- canonical gfx helpers remain behind private substrate boundary
- rejects stdlib version flag
- primec and primevm usage prefer text transforms and import flags
- primec and primevm accept ir inline flag
- primevm accepts explicit emit vm compatibility flag
- primevm debug-json emits stable NDJSON schema

### test_compile_run_smoke_core_debug_and_docs.cpp

- primevm debug-json snapshots include payloads across step boundaries
- primevm debug-json snapshots mode requires debug-json
- primevm rejects invalid debug-json snapshots mode
- primec rejects debug-json option
- primevm debug-trace requires path and mode exclusivity
- primevm debug-trace writes deterministic complete event logs
- primevm debug-replay requires trace and mode exclusivity
- primevm debug-replay restores checkpoint snapshots at requested sequence
- primevm debug-replay is deterministic and rejects invalid traces
- primevm debug-replay bypasses source compilation on trace-only path
- primevm debug-replay accepts whitespace and escaped checkpoint fields
- primevm debug-dap rejects incompatible debug-json mode

### test_compile_run_smoke_core_demo_scripts.cpp

- primevm debug-dap emits deterministic framed transcripts
- primevm debug-dap end-to-end process smoke emits exit events
- primevm debug-dap accepts instruction breakpoints and rejects post-exit locals
- primevm rejects primec output flags
- wasm runtime tooling hook executes or reports explicit skip
- defaults to native output with stem name
- emits PSIR bytecode with --emit=ir
- primevm forwards entry args
- primevm supports argv string bindings
- compiles and runs with line comments after expressions
- compiles and runs string count and indexing in C++ emitter
- compiles and runs single-quoted strings in C++ emitter
- compiles and runs method calls via type namespaces
- compiles and runs count forwarding to method
- compiles and runs method call on constructor
- compiles and runs call with body block
- compiles and runs templated method call
- compiles and runs block expression
- compiles and runs boolean ops with conversions
- compiles and runs integer width converts
- compiles and runs convert bool from negative integer
- compiles and runs boolean ops short-circuit

### test_compile_run_smoke_core_gfx_end_to_end.cpp

- gfx compatibility shim end-to-end coverage runs across backends
- canonical gfx end-to-end conformance runs across backends
- gfx imports reject unsupported backend targets with deterministic diagnostics
- gfx compatibility shim static fields import across backends

### test_compile_run_smoke_core_gfx_entrypoints.cpp

- experimental gfx window constructor entry point runs across backends
- experimental gfx device constructor entry point runs across backends
- experimental gfx resource wrapper slice runs across backends
- canonical gfx resource wrapper slice runs across backends
- experimental gfx render pass wrapper slice runs across backends
- canonical gfx render pass wrapper slice runs across backends
- experimental gfx resource wrapper errors stay deterministic across backends
- canonical gfx resource wrapper errors stay deterministic across backends
- experimental gfx pipeline entry point runs across backends
- canonical gfx pipeline entry point runs across backends

### test_compile_run_smoke_core_gfx_imports.cpp

- gfx compatibility shim type surface imports across backends
- canonical gfx type surface imports across backends
- gfx compatibility shim error helper imports across backends
- gfx compatibility shim substrate boundary imports across backends

### test_compile_run_smoke_core_wasm_core.cpp

- count forwards to type method across backends
- semicolons act as separators
- rejects non-argv entry parameter in exe
- rejects unsupported emit kinds
- primec emits wasm bytecode for integer local control-flow subset
- primec emits wasm bytecode for float ops with tolerance-gated conversions
- primec emits wasm bytecode for i64 and u64 conversion opcodes
- primec emits wasm bytecode for support-matrix math nominal helpers
- primec emits wasm bytecode for quaternion reference multiply and rotation
- primec emits wasm bytecode for matrix composition order references with tolerance
- primec emits wasm bytecode for matrix arithmetic helpers with tolerance
- primec emits wasm bytecode for quaternion arithmetic helpers with tolerance
- primec rejects wasm support-matrix plus mismatch with deterministic diagnostic
- primec rejects wasm support-matrix implicit conversion with deterministic diagnostic
- primec emits wasm bytecode for direct callable definitions

### test_compile_run_smoke_core_wasm_limits.cpp

- primec wasm documented limit IDs have conformance coverage

### test_compile_run_smoke_core_wasm_profiles.cpp

- primec wasm wasi rejects malformed png inputs deterministically
- primec wasm parity corpus matches vm outputs and exits deterministically
- primec wasm i64 and u64 conversion edge cases trap in runtime
- primec emits wasm bytecode for repeat while and for loops
- primec options default to wasm extension for emit kind
- primec options parse wasm profile aliases and validate values
- primec rejects removed type resolver option
- primec options parse benchmark semantic definition validation worker count
- primec options parse benchmark semantic phase counters flag
- primec options parse benchmark semantic allocation counters flag
- primec options parse benchmark semantic rss checkpoints flag
- primec options parse benchmark semantic method-target memoization toggle
- primec options parse benchmark semantic graph-local-auto legacy key shadow flag
- primec options parse benchmark semantic graph-local-auto legacy side-channel shadow flag
- primec options parse benchmark semantic graph-local-auto dependency scratch pmr toggle
- primec options parse benchmark semantic repeat count flag
- primec emit-diagnostics reports structured wasm emit payload
- primec emit-diagnostics rejects unsupported wasm IR features with stable payloads
- primec wasm profile matrix gates wasi-only effects and opcodes

### test_compile_run_smoke_core_wasm_wasi_core.cpp

- primec wasm wasi stdout and stderr match vm output
- primec wasm wasi argc path matches vm exit code
- primec wasm wasi supports File<Read>.read_byte with deterministic eof
- primec wasm wasi runs ppm read for ascii p3 inputs
- primec wasm wasi runs binary p6 ppm inputs
- primec wasm wasi rejects truncated binary ppm reads deterministically
- primec wasm wasi ppm write requires heap_alloc for vector literal
- primec wasm wasi invalid ppm write requires heap_alloc for vector literal

### test_compile_run_smoke_core_wasm_wasi_png_decode.cpp

- primec wasm wasi runs stored sub-filter rgb png inputs
- primec wasm wasi runs stored up-filter rgb png inputs
- primec wasm wasi runs stored average-filter rgb png inputs
- primec wasm wasi runs stored paeth-filter rgb png inputs
- primec wasm wasi runs fixed-huffman backreference rgb png inputs
- primec wasm wasi runs dynamic-huffman literal rgb png inputs
- primec wasm wasi runs dynamic-huffman backreference rgb png inputs

### test_compile_run_smoke_core_wasm_wasi_png_interlaced.cpp

- primec wasm wasi runs broader interlaced png decode programs

### test_compile_run_smoke_core_wasm_wasi_png_write.cpp

- primec wasm wasi runs ppm write for deterministic rgb outputs
- primec wasm wasi png write requires heap_alloc for vector literal
- primec wasm wasi runs png write for deterministic rgb outputs
- primec wasm wasi invalid png write requires heap_alloc for vector literal
- primec wasm wasi rejects invalid png write inputs deterministically
- primec wasm wasi runs stored rgb png inputs

## compile_run/text_filters (446 tests, 28 files)

### test_compile_run_text_filters_core_lists.cpp

- compiles and runs implicit i32 suffix
- compiles and runs increment/decrement sugar
- no transforms overrides text transforms
- no transforms accepts canonical syntax
- no transforms accepts brace constructors
- no transforms rejects infix operators
- no transforms rejects float without suffix
- no transforms rejects single-letter float suffix
- no transforms rejects increment sugar
- no transforms rejects if sugar
- no transforms rejects loop sugar
- no transforms rejects while sugar
- no transforms rejects for sugar
- no transforms rejects indexing sugar
- text transforms none disables implicit utf8
- transform list none disables implicit utf8
- transform list default enables implicit i32
- transform list none clears prior defaults
- transform list semicolons split tokens
- transform list whitespace splits tokens
- transform list rejects unknown transform name
- transform list none rejects infix operators
- text transforms none rejects infix operators
- text transforms none still accepts canonical syntax
- legacy text-filters alias forms are rejected in primec and primevm
- compiles and runs implicit i32 via transform list
- compiles and runs implicit i32 via transform list in primevm

### test_compile_run_text_filters_diagnostics_a.cpp

- primec collect-diagnostics emits stable multi-parse payload
- primevm collect-diagnostics emits stable multi-parse payload
- primec collect-diagnostics keeps first duplicate-definition payload
- primevm collect-diagnostics keeps first duplicate-definition payload
- primec collect-diagnostics emits stable semantic import payload for unknown paths
- primec collect-diagnostics maps parse spans through source units
- primevm collect-diagnostics emits stable semantic import payload for unknown paths
- primec collect-diagnostics emits stable multi-semantic payload for invalid transforms
- primevm collect-diagnostics emits stable multi-semantic payload for invalid transforms
- primec collect-diagnostics emits stable multi-semantic payload for return-kind errors
- primevm collect-diagnostics emits stable multi-semantic payload for return-kind errors
- primec collect-diagnostics emits stable multi-semantic payload for definition pass errors
- primevm collect-diagnostics emits stable multi-semantic payload for definition pass errors
- primec collect-diagnostics emits intra-definition multi-semantic payload

### test_compile_run_text_filters_diagnostics_b.cpp

- primevm collect-diagnostics emits intra-definition multi-semantic payload
- primec collect-diagnostics emits intra-definition argument-shape payload
- primevm collect-diagnostics emits intra-definition argument-shape payload
- primec collect-diagnostics keeps user vector arg-shape diagnostics
- primevm collect-diagnostics keeps user vector arg-shape diagnostics
- primec collect-diagnostics keeps user array arg-shape diagnostics
- primevm collect-diagnostics keeps user array arg-shape diagnostics
- primec collect-diagnostics keeps user map arg-shape diagnostics
- primevm collect-diagnostics keeps user map arg-shape diagnostics
- primec collect-diagnostics keeps user wrapper at_unsafe arg-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper at_unsafe arg-shape diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper unsafe arg-type diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper unsafe arg-type diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper at arg-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper at arg-shape diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_c.cpp

- collect-diagnostics keeps user wrapper index arg-type diagnostics in definition scope
- collect-diagnostics keeps user wrapper count capacity arg-shape diagnostics in definition scope
- collect-diagnostics keeps user wrapper count capacity arg-shape reverse diagnostics in definition scope
- collect-diagnostics keeps user wrapper count capacity pair extra-arg diagnostics in definition scope
- collect-diagnostics keeps user wrapper count capacity pair missing-arg diagnostics in definition scope
- collect-diagnostics keeps user wrapper count capacity pair missing-arg reverse diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_d.cpp

- primec collect-diagnostics keeps user wrapper method count capacity pair missing-arg diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count capacity pair missing-arg diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count capacity pair missing-arg reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count capacity pair missing-arg reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count capacity pair extra-arg diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count capacity pair extra-arg diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count capacity pair extra-arg reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count capacity pair extra-arg reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity call-pair arg-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity call-pair arg-shape diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity call-pair arg-shape reverse diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_e.cpp

- primevm collect-diagnostics keeps user wrapper count capacity call-pair arg-shape reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity call-pair extra-arg diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity call-pair extra-arg diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity call-pair extra-arg reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity call-pair extra-arg reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity call-pair arg-type diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity call-pair arg-type diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity call-pair arg-type reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity call-pair arg-type reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity call-pair mixed-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity call-pair mixed-shape diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_f.cpp

- primec collect-diagnostics keeps user wrapper count capacity call-pair mixed-shape reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity call-pair mixed-shape reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity arg-type diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity arg-type diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count capacity arg-type reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count capacity arg-type reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count capacity pair arg-type diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count capacity pair arg-type diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count capacity pair arg-type reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count capacity pair arg-type reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count capacity pair mixed-shape diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_g.cpp

- primevm collect-diagnostics keeps user wrapper method count capacity pair mixed-shape diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count capacity pair mixed-shape reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count capacity pair mixed-shape reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count pair arg-type diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count pair arg-type diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count pair arg-type reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count pair arg-type reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count pair arg-type diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count pair arg-type diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count call-pair arg-type diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count call-pair arg-type diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count call-pair arg-type reverse diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_h.cpp

- primevm collect-diagnostics keeps user wrapper count call-pair arg-type reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count call-pair mixed-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count call-pair mixed-shape diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count call-pair mixed-shape reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count call-pair mixed-shape reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count call-pair arg-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count call-pair arg-shape diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count call-pair arg-shape reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count call-pair arg-shape reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count call-pair extra-arg diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count call-pair extra-arg diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_i.cpp

- primec collect-diagnostics keeps user wrapper count call-pair extra-arg reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count call-pair extra-arg reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count pair arg-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count pair arg-shape diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count pair missing-arg reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count pair missing-arg reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count pair arg-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count pair arg-shape diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count pair missing-arg reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count pair missing-arg reverse diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_j.cpp

- primec collect-diagnostics keeps user wrapper method count pair arg-type reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count pair arg-type reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count pair mixed-shape diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count pair mixed-shape diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count pair mixed-shape reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count pair mixed-shape reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count pair extra-arg diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count pair extra-arg diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper method count pair extra-arg reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper method count pair extra-arg reverse diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count pair extra-arg diagnostics in definition scope

### test_compile_run_text_filters_diagnostics_k.cpp

- primevm collect-diagnostics keeps user wrapper count pair extra-arg diagnostics in definition scope
- primec collect-diagnostics keeps user wrapper count pair extra-arg reverse diagnostics in definition scope
- primevm collect-diagnostics keeps user wrapper count pair extra-arg reverse diagnostics in definition scope
- primec collect-diagnostics reports ordinary map resolution failure before execution
- primevm collect-diagnostics reports ordinary map resolution failure before execution
- primec collect-diagnostics reports builtin vector literal heap-alloc failure before execution
- primevm collect-diagnostics reports builtin vector literal heap-alloc failure before execution
- primec collect-diagnostics reports builtin array literal template-arity failure before execution
- primevm collect-diagnostics reports builtin array literal template-arity failure before execution
- primec collect-diagnostics emits intra-definition argument-type payload
- primevm collect-diagnostics emits intra-definition argument-type payload
- primec collect-diagnostics emits intra-definition flow-effect payload
- primevm collect-diagnostics emits intra-definition flow-effect payload

### test_compile_run_text_filters_diagnostics_l.cpp

- primec collect-diagnostics emits stable multi-semantic payload for execution pass errors
- primec collect-diagnostics emits intra-execution multi-semantic payload
- primevm collect-diagnostics emits stable multi-semantic payload for execution pass errors
- primevm collect-diagnostics emits intra-execution multi-semantic payload
- primec collect-diagnostics emits intra-execution argument-shape payload
- primevm collect-diagnostics emits intra-execution argument-shape payload
- primec collect-diagnostics keeps execution map arg-shape diagnostics
- primevm collect-diagnostics keeps execution map arg-shape diagnostics
- primec collect-diagnostics keeps execution vector arg-shape diagnostics
- primevm collect-diagnostics keeps execution vector arg-shape diagnostics
- primec collect-diagnostics keeps execution array arg-shape diagnostics
- primevm collect-diagnostics keeps execution array arg-shape diagnostics
- primec collect-diagnostics keeps execution wrapper at_unsafe arg-shape diagnostics

### test_compile_run_text_filters_diagnostics_m.cpp

- primevm collect-diagnostics keeps execution wrapper at_unsafe arg-shape diagnostics
- primec collect-diagnostics keeps execution wrapper unsafe arg-type diagnostics
- primevm collect-diagnostics keeps execution wrapper unsafe arg-type diagnostics
- primec collect-diagnostics keeps execution wrapper at arg-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper at arg-shape diagnostics
- primec collect-diagnostics keeps execution wrapper index arg-type diagnostics
- primevm collect-diagnostics keeps execution wrapper index arg-type diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity arg-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity arg-shape diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity arg-shape reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity arg-shape reverse diagnostics

### test_compile_run_text_filters_diagnostics_n.cpp

- primec collect-diagnostics keeps execution wrapper count capacity pair extra-arg diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity pair extra-arg diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity pair missing-arg diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity pair missing-arg diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity pair missing-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity pair missing-arg reverse diagnostics
- primec collect-diagnostics keeps execution wrapper method count capacity pair missing-arg diagnostics
- primevm collect-diagnostics keeps execution wrapper method count capacity pair missing-arg diagnostics
- primec collect-diagnostics keeps execution wrapper method count capacity pair missing-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper method count capacity pair missing-arg reverse diagnostics
- primec collect-diagnostics keeps execution wrapper method count capacity pair extra-arg diagnostics

### test_compile_run_text_filters_diagnostics_o.cpp

- primevm collect-diagnostics keeps execution wrapper method count capacity pair extra-arg diagnostics
- primec collect-diagnostics keeps execution wrapper method count capacity pair extra-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper method count capacity pair extra-arg reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity call-pair arg-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity call-pair arg-shape diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity call-pair arg-shape reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity call-pair arg-shape reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity call-pair extra-arg diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity call-pair extra-arg diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity call-pair extra-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity call-pair extra-arg reverse diagnostics

### test_compile_run_text_filters_diagnostics_p.cpp

- primec collect-diagnostics keeps execution wrapper count capacity call-pair arg-type diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity call-pair arg-type diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity call-pair arg-type reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity call-pair arg-type reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity call-pair mixed-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity call-pair mixed-shape diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity call-pair mixed-shape reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity call-pair mixed-shape reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity arg-type diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity arg-type diagnostics
- primec collect-diagnostics keeps execution wrapper count capacity arg-type reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count capacity arg-type reverse diagnostics

### test_compile_run_text_filters_diagnostics_q.cpp

- primec collect-diagnostics keeps execution wrapper method count capacity pair arg-type diagnostics
- primevm collect-diagnostics keeps execution wrapper method count capacity pair arg-type diagnostics
- primec collect-diagnostics keeps execution wrapper method count capacity pair arg-type reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper method count capacity pair arg-type reverse diagnostics
- primec collect-diagnostics keeps execution wrapper method count capacity pair mixed-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper method count capacity pair mixed-shape diagnostics
- primec collect-diagnostics keeps execution wrapper method count capacity pair mixed-shape reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper method count capacity pair mixed-shape reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count pair arg-type diagnostics
- primevm collect-diagnostics keeps execution wrapper count pair arg-type diagnostics
- primec collect-diagnostics keeps execution wrapper count pair arg-type reverse diagnostics

### test_compile_run_text_filters_diagnostics_r.cpp

- primevm collect-diagnostics keeps execution wrapper count pair arg-type reverse diagnostics
- primec collect-diagnostics keeps execution wrapper method count pair arg-type diagnostics
- primevm collect-diagnostics keeps execution wrapper method count pair arg-type diagnostics
- primec collect-diagnostics keeps execution wrapper count call-pair arg-type diagnostics
- primevm collect-diagnostics keeps execution wrapper count call-pair arg-type diagnostics
- primec collect-diagnostics keeps execution wrapper count call-pair arg-type reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count call-pair arg-type reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count call-pair mixed-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper count call-pair mixed-shape diagnostics
- primec collect-diagnostics keeps execution wrapper count call-pair mixed-shape reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count call-pair mixed-shape reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count call-pair arg-shape diagnostics

### test_compile_run_text_filters_diagnostics_s.cpp

- primevm collect-diagnostics keeps execution wrapper count call-pair arg-shape diagnostics
- primec collect-diagnostics keeps execution wrapper count call-pair arg-shape reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count call-pair arg-shape reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count call-pair extra-arg diagnostics
- primevm collect-diagnostics keeps execution wrapper count call-pair extra-arg diagnostics
- primec collect-diagnostics keeps execution wrapper count call-pair extra-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count call-pair extra-arg reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count pair arg-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper count pair arg-shape diagnostics
- primec collect-diagnostics keeps execution wrapper count pair missing-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count pair missing-arg reverse diagnostics

### test_compile_run_text_filters_diagnostics_t.cpp

- primec collect-diagnostics keeps execution wrapper method count pair arg-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper method count pair arg-shape diagnostics
- primec collect-diagnostics keeps execution wrapper method count pair missing-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper method count pair missing-arg reverse diagnostics
- primec collect-diagnostics keeps execution wrapper method count pair arg-type reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper method count pair arg-type reverse diagnostics
- primec collect-diagnostics keeps execution wrapper method count pair mixed-shape diagnostics
- primevm collect-diagnostics keeps execution wrapper method count pair mixed-shape diagnostics
- primec collect-diagnostics keeps execution wrapper method count pair mixed-shape reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper method count pair mixed-shape reverse diagnostics
- primec collect-diagnostics keeps execution wrapper method count pair extra-arg diagnostics

### test_compile_run_text_filters_diagnostics_u.cpp

- primevm collect-diagnostics keeps execution wrapper method count pair extra-arg diagnostics
- primec collect-diagnostics keeps execution wrapper method count pair extra-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper method count pair extra-arg reverse diagnostics
- primec collect-diagnostics keeps execution wrapper count pair extra-arg diagnostics
- primevm collect-diagnostics keeps execution wrapper count pair extra-arg diagnostics
- primec collect-diagnostics keeps execution wrapper count pair extra-arg reverse diagnostics
- primevm collect-diagnostics keeps execution wrapper count pair extra-arg reverse diagnostics
- primec collect-diagnostics emits intra-execution argument-type payload
- primevm collect-diagnostics emits intra-execution argument-type payload
- primec collect-diagnostics emits intra-execution flow-effect payload
- primevm collect-diagnostics emits intra-execution flow-effect payload

### test_compile_run_text_filters_diagnostics_v.cpp

- primevm emit-diagnostics reports structured semantic payload
- primec emit-diagnostics reports structured lowering payload
- emit-diagnostics reports argument payload for removed text-filters option
- primec list transforms prints metadata
- primec and primevm list transforms match

### test_compile_run_text_filters_dumps.cpp

- dump pre_ast shows imports and text filters
- dump ir prints canonical output
- dump ast ignores semantic errors
- dump ast-semantic shows canonicalized ast
- dump ast-semantic shows experimental map destroy cleanup
- dump ast-semantic shows experimental soa_vector wrapper count runtime
- dump ast-semantic keeps canonical soa_vector get helper path compatibility
- dump ast-semantic rewrites bare soa_vector get helper on helper return compatibility
- dump ast-semantic rewrites global helper-return soa_vector method shadows to same-path helpers compatibility
- dump ast-semantic rewrites method-like helper-return soa_vector method shadows to same-path helpers compatibility
- dump ast-semantic accepts nested struct-body soa_vector constructor-bearing helper returns compatibility
- dump ast-semantic rewrites nested struct-body soa_vector method shadows to same-path helpers compatibility
- dump ast-semantic rewrites experimental soa_vector reflected field index syntax
- dump ast-semantic rewrites experimental soa_vector mutating field index targets to soaVectorRef
- dump ast-semantic rewrites richer borrowed experimental soa_vector mutating field index targets to soaVectorRef
- dump ast-semantic rewrites method-like borrowed experimental soa_vector mutating field index targets to soaVectorRef
- dump ast-semantic rewrites borrowed experimental soa_vector reflected field index syntax
- dump ast-semantic rewrites borrowed local experimental soa_vector reflected field index syntax
- dump ast-semantic rewrites borrowed helper-return experimental soa_vector reflected field index syntax
- dump ast-semantic rewrites experimental soa_vector reflected call-form field index syntax
- dump ast-semantic rewrites experimental soa_vector inline location borrow field index syntax
- dump ast-semantic rewrites dereferenced borrowed helper-return experimental soa_vector reflected field index syntax
- dump ast-semantic rewrites method-like borrowed helper-return experimental soa_vector helpers
- dump ast-semantic rewrites inline location method-like borrowed helper-return experimental soa_vector helpers
- dump ast-semantic rewrites direct return method-like borrowed helper-return experimental soa_vector reads
- dump ast-semantic rewrites direct return borrowed helper-return experimental soa_vector reads
- dump ast-semantic rewrites direct return inline location method-like borrowed helper-return experimental soa_vector reads
- dump ast-semantic rewrites direct return inline location borrowed helper-return experimental soa_vector reads
- dump ast-semantic rewrites builtin soa_vector count forms to canonical helper path
- dump ast-semantic rewrites imported builtin soa_vector to_aos forms to canonical helper path
- dump ast-semantic rewrites no-import builtin soa_vector to_aos forms to canonical helper path
- dump ast-semantic rewrites vector-target helper-shadowed to_aos method forms to direct helper path
- dump ast-semantic rewrites vector-target old-explicit mutator shadows to direct helper path
- dump ast-semantic rewrites vector-target method mutator shadows to direct helper path
- dump ast-semantic keeps direct canonical experimental soa_vector to_aos helper path
- dump ast-semantic canonical soa_vector to_aos helper body uses canonical count/get loop compatibility
- dump ast-semantic canonical soa_vector to_aos_ref helper body uses canonical count_ref/get_ref loop compatibility
- dump ast-semantic keeps imported experimental soa_vector to_aos helper path
- dump ast-semantic rewrites borrowed helper-return experimental soa_vector to_aos
- dump ast-semantic rewrites borrowed helper-return experimental soa_vector to_aos_ref via canonical helper
- dump ast-semantic keeps helper-return experimental soa_vector to_aos with same-path helper
- dump ast-semantic keeps helper-return builtin soa_vector to_aos with same-path helper
- dump ast-semantic rewrites global helper-return builtin soa_vector reads to canonical helpers
- dump ast-semantic rewrites method-like helper-return builtin soa_vector reads to canonical helpers
- dump ast-semantic keeps borrowed soa_vector ref_ref same-path helper shadows compatibility
- dump ast-semantic keeps builtin soa_vector ref_ref same-path helper shadows
- dump ast-semantic rewrites inline location experimental soa_vector read-only methods
- dump ast-semantic rewrites inline location borrowed helper-return experimental soa_vector helpers
- dump ast_semantic alias works
- dump type_graph alias works and prints graph output
- dump semantic_product alias works and prints semantic output
- dump ast-semantic reports semantic errors
- dump stage rejects unknown value
- primec and primevm dump pre_ast match
- primec and primevm dump ast-semantic match
- primec and primevm dump type-graph match
- primec and primevm dump semantic-product match
- semantic-product dump keeps provenance handles while ast-semantic keeps syntax
- pipeline dump surfaces keep inspection order and lowering-facing boundaries
- primevm dump stage rejects unknown value
- primec plain parse diagnostics include file line and caret
- primevm plain semantic diagnostics include file line and note
- primec emit-diagnostics reports structured parse payload

### test_compile_run_text_filters_misc.cpp

- compiles and runs with comments
- compiles and runs block expression with outer scope capture
- compiles and runs greater_than
- compiles and runs less_than
- compiles and runs equal
- compiles and runs not_equal
- compiles and runs min
- compiles and runs max f32
- compiles and runs abs
- compiles and runs sign f32
- compiles and runs saturate f32
- compiles and runs clamp
- compiles and runs clamp i64
- compiles and runs clamp mixed i32/i64
- compiles and runs clamp u64
- compiles and runs clamp f32
- compiles and runs clamp f64
- compiles and runs boolean literal
- compiles and runs bool return
- compiles and runs bool comparison
- compiles and runs bool and signed int comparison
- rejects bool and u64 comparison
- compiles and runs string binding
- compiles and runs two-element array literal
- compiles and runs flat map constructor
- compiles and runs map entry constructor
- compiles and runs canonical map constructor
- compiles and runs named-arg call
- compiles and runs convert builtin
- compiles and runs mixed named args
- compiles and runs interleaved named args
- compiles and runs reordered named args
- compiles and runs map constructor with named-arg value
- compiles and runs if statement sugar
- compiles and runs early return in if

### test_compile_run_text_filters_runtime_if.cpp

- compiles and runs implicit utf8 suffix by default
- compiles and runs implicit hex literal
- compiles and runs float binding
- compiles and runs single-letter float suffix
- compiles and runs float comparison
- compiles and runs string comparison
- rejects mixed int/float arithmetic
- rejects method call on array
- rejects method call on pointer
- rejects method call on reference
- compiles and runs pointer operator sugar
- rejects method call on map
- implicit suffix enabled by default
- compiles and runs if
- compiles and runs if expression
- runs if expression in vm
- compiles and runs if block sugar in return expression
- compiles and runs if expr block statements
- compiles and runs lazy if expression taking then branch
- compiles and runs lazy if expression taking else branch

### test_compile_run_text_filters_semantic_rules.cpp

- text transforms accept whitespace separators
- transform list enables single_type_to_return
- per-definition single_type_to_return marker
- semantic transforms flag enables single_type_to_return
- semantic transform rules apply per path
- semantic transform rules prefer later matching entry
- semantic transform rules clear on none token
- semantic transform rules ignore empty rule tokens
- semantic transform rules prefer later wildcard match
- semantic transform root wildcard applies to top-level definitions
- semantic transform rules reject recurse without wildcard
- semantic transform rules reject empty transform list
- semantic transform rules reject non-trailing wildcard
- semantic transform rules reject path without slash prefix
- semantic transform rules reject missing equals
- semantic transform rules reject empty path
- semantic transform rules reject text-only transform name
- semantic transform rules reject unknown transform name
- semantic transform rules accept recursive suffix alias
- semantic transform wildcard does not match sibling prefix
- semantic transform wildcard does not match base path
- semantic transform rules ignore unrelated exact path
- semantic transform rules ignore unrelated wildcard path
- semantic root wildcard only recurses when requested
- semantic transforms ignore text transforms

### test_compile_run_text_filters_text_rules.cpp

- no transforms disables single_type_to_return
- per-envelope text transforms enable collections
- per-envelope text transforms override operators
- per-envelope text transforms apply to bindings
- per-envelope text transforms apply to executions in arguments
- text transforms can append additional transforms
- transform list auto-deduces append_operators
- text transform rules apply to namespace paths
- text transform rules ignore unrelated wildcard path
- text transform rules ignore unrelated exact path
- text transform wildcard does not match base path
- text transform wildcard does not match sibling prefix
- text transform rules apply without transform lists
- text transform rules ignore empty rule tokens
- text transform rules none token without follow-up keeps rules empty
- text transform rules prefer later matching entry
- text transform rules prefer later wildcard match
- text transform root wildcard applies to top-level definitions
- text transform rules clear on none token
- text transform rules reject missing equals
- text transform rules reject empty transform list
- text transform rules reject empty path
- text transform rules reject path without slash prefix
- text transform rules reject non-trailing wildcard
- text transform rules reject recurse without wildcard
- text transform rules reject semantic-only transform name
- text transform rules reject unknown transform name
- text transform rules apply to nested definitions
- text transform rules ignore nested transform lists
- text transform rules recurse when requested
- text transform rules accept recursive suffix alias
- root wildcard transform rules only recurse when requested

## compile_run/vm (1010 tests, 32 files)

### test_compile_run_vm_bounds.cpp

- vm array access checks bounds
- vm array access with u64 index
- vm array slice count and indexed access use slice extent
- vm array slice checks runtime bounds at construction
- array slice rejects statically out-of-range literal ranges
- vm array access rejects negative index
- vm vector access checks bounds
- vm vector access rejects negative index
- vm experimental vector at_unsafe checks positive out-of-range index
- vm experimental vector method at_unsafe checks positive out-of-range index
- vm experimental vector at checks positive out-of-range index
- vm experimental vector method at checks positive out-of-range index
- vm experimental vector at_unsafe rejects index past capacity even if count is forged
- vm experimental vector method at rejects index past capacity even if count is forged
- vm experimental vector reserve rejects forged count above capacity
- vm experimental vector clear rejects forged count above capacity
- vm experimental vector destroy rejects forged count above capacity
- vm experimental vector count rejects forged count above capacity
- vm experimental vector capacity rejects forged negative capacity
- vm experimental vector capacity rejects forged excessive capacity
- vm experimental vector set_field_count rejects negative count
- vm experimental vector set_field_capacity rejects below-count value
- vm rejects misaligned pointer dereference
- vm array unsafe access reads element
- vm array unsafe access with u64 index
- vm argv access checks bounds
- vm argv access rejects negative index
- vm argv unsafe access skips bounds
- vm argv unsafe access with u64 index
- vm argv unsafe access skips negative index
- vm argv binding checks bounds
- vm argv binding unsafe skips bounds
- vm argv unsafe binding copy skips bounds
- vm argv string binding count fails
- vm argv string binding index fails
- rejects vm argv call argument
- rejects vm argv call argument unsafe

### test_compile_run_vm_collections_array_and_wrapper_shadows.cpp

- runs vm with builtin array count before user call shadow
- runs vm with canonical slash vector count same-path helper on array receiver
- rejects vm wrapper-returned canonical vector count slash-method on array receiver
- rejects vm wrapper-returned canonical vector capacity slash-method on array receiver
- rejects vm alias slash-method vector access on array receiver
- rejects vm user map count call shadow without imported canonical helper
- rejects vm user map count method shadow without imported canonical helper
- runs vm canonical map sugar with current helper precedence
- rejects vm canonical unknown map helper with canonical diagnostics
- runs vm canonical map access string shadow through builtin storage
- rejects vm canonical map access non-string shadow during lowering
- runs vm rooted map count as ordinary user definition
- rejects vm map compatibility count call mismatch with canonical templated helper present
- runs vm map compatibility explicit-template count call with canonical templated helper present
- rejects vm map compatibility explicit-template count call with non-templated alias helper
- rejects vm map compatibility explicit-template count method with non-templated alias helper
- rejects vm canonical explicit-template map count call with non-templated canonical helper
- rejects vm canonical implicit-template map count call with canonical argument-shape diagnostics
- rejects vm canonical implicit-template map count expression call with wrapper slash return envelope
- runs vm with builtin string count before user call shadow
- runs vm with builtin string count before user method shadow
- rejects vm canonical map reference string access without imported canonical helper
- rejects vm builtin count on canonical map reference string access without imported helper
- vm rejects bare builtin count on wrapper-returned canonical map access before lowering
- vm rejects user string count method shadow on wrapper-returned canonical map access
- vm keeps imported wrapper-returned canonical map reference access lowering diagnostics
- vm keeps wrapper-returned canonical map reference primitive receiver diagnostics
- rejects vm user map method sugar on wrapper-returned canonical map references without imported helpers
- vm keeps non-string diagnostics on canonical map reference access count shadow
- vm keeps canonical map unknown-target diagnostics on wrapper-returned map reference method sugar

### test_compile_run_vm_collections_core_aliases.cpp

- runs vm with numeric array literals
- runs vm with numeric vector literals
- runs vm with stdlib namespaced vector builtin aliases
- rejects vm namespaced wrapper string access method chain compatibility fallback
- rejects vm slash-method wrapper string access method chain compatibility fallback
- vm keeps slash-method wrapper string access i32 diagnostics
- rejects vm array namespaced vector constructor alias
- rejects vm array namespaced vector at alias
- rejects vm array namespaced vector at_unsafe alias
- rejects vm wrapper array namespaced vector at alias
- rejects vm wrapper array namespaced vector at_unsafe alias
- rejects vm array namespaced vector count builtin alias
- rejects vm array namespaced vector count method alias
- rejects vm array namespaced vector capacity method alias
- rejects vm map namespaced count compatibility alias
- rejects vm map namespaced contains compatibility alias
- rejects vm map namespaced tryAt compatibility alias
- runs vm unchecked pointer conformance harness for imported .prime helpers
- rejects vm map namespaced at compatibility alias without explicit alias
- rejects vm map namespaced at unsafe compatibility alias without explicit alias
- runs vm explicit map helper count/contains/tryAt through same-path aliases while direct access stays builtin
- runs vm explicit canonical map helpers with current mixed count precedence
- runs vm stdlib namespaced map helpers on canonical map references
- rejects vm canonical map method with slash return type receiver
- rejects vm canonical map access direct calls on wrapper slash return receiver
- rejects vm canonical map access helper key mismatch on wrapper slash return receiver
- runs vm explicit canonical map typed bindings with builtin helpers
- rejects vm explicit canonical map typed binding key mismatch
- rejects vm stdlib namespaced map constructor alias fallback without import
- rejects vm stdlib namespaced map at fallback without import
- rejects vm stdlib namespaced map at unsafe fallback without import
- runs vm bare map count through visible canonical helper
- rejects vm bare map count without imported canonical helper
- runs vm bare map at through explicit canonical helper

### test_compile_run_vm_collections_map_vector_shadows.cpp

- rejects vm user map at string positional call shadow during semantics
- runs vm with map access preferring later map receiver over string
- rejects vm user map at_unsafe string positional call shadow during semantics
- runs vm with user map at method shadow
- rejects vm user vector at call shadow during semantics
- rejects vm named vector at expression receiver precedence
- keeps vm builtin vector at method over user shadow
- keeps vm builtin string at_unsafe call over user shadow
- keeps vm builtin string at_unsafe method over user shadow
- rejects vm user vector at_unsafe call shadow during semantics
- keeps vm builtin vector at_unsafe method over user shadow
- keeps vm builtin string at call over user shadow
- keeps vm builtin string at method over user shadow
- rejects vm vector push helper during lowering
- rejects vm vector mutator method calls during lowering
- compiles and runs canonical vector discard helpers with owned elements in vm backend
- compiles and runs canonical vector indexed removal helpers with owned elements in vm backend
- rejects vm vector push with non-relocation-trivial elements
- rejects vm vector constructor with non-relocation-trivial elements
- runs vm indexed vector removals with ownership semantics
- runs vm with user push helper shadow
- runs vm with user vector constructor shadow
- runs vm with user array constructor shadow
- runs vm with user map constructor shadow
- rejects vm builtin vector constructor named arguments
- rejects vm builtin array constructor named arguments
- rejects vm builtin map constructor named arguments
- runs vm namespaced vector count named arguments through imported stdlib helper
- runs vm namespaced vector capacity named arguments through imported stdlib helper
- rejects vm removed vector access alias named arguments
- rejects vm removed vector access alias at_unsafe named arguments
- runs vm with user map constructor block shadow
- runs vm with user vector constructor block shadow
- runs vm with user array constructor block shadow
- runs vm with user vector push call shadow returning grown count

### test_compile_run_vm_collections_map_wrapper_shadows.cpp

- rejects vm wrapper-returned canonical map access count shadow
- rejects vm direct wrapper-returned canonical map access count shadow
- rejects vm wrapper-returned canonical map method access string receiver typing
- rejects vm builtin string count on wrapper-returned slash-method map access
- runs vm canonical vector access string literal count fallback
- rejects vm canonical vector unsafe access count shadow
- vm keeps primitive diagnostics for canonical vector method access count shadow
- rejects vm canonical vector unsafe method access count shadow
- rejects vm slash-method vector access string count fallback
- vm keeps slash-method vector access unknown-method diagnostics
- rejects vm wrapper-returned vector access string literals
- vm keeps wrapper-returned vector access primitive count diagnostics
- runs vm with user vector count method shadow
- rejects vm canonical slash vector count same-path helper on map receiver
- rejects vm wrapper-returned canonical vector count slash-method on map receiver
- rejects vm wrapper-returned canonical vector capacity slash-method on map receiver
- runs vm with user vector capacity method shadow
- runs vm user vector count call shadow
- runs vm user vector capacity call shadow
- rejects vm user array capacity call shadow with semantic query diagnostic
- runs vm with user array capacity method shadow
- keeps vm builtin array at call over user shadow
- runs vm with user array at method shadow
- keeps vm builtin array at_unsafe call over user shadow
- runs vm with user array at_unsafe method shadow
- runs vm with user map at_unsafe call shadow
- runs vm with user map at_unsafe method shadow
- runs vm with user map at call shadow

### test_compile_run_vm_collections_method_aliases_a.cpp

- rejects vm templated stdlib vector wrapper temporary method missing index
- rejects vm templated stdlib vector wrapper temporary unsafe method arity mismatch
- rejects vm templated stdlib vector wrapper temporary unsafe method missing index
- rejects vm templated stdlib vector wrapper temporary capacity type mismatch
- rejects vm templated stdlib vector wrapper temporary capacity call arity mismatch
- rejects vm templated stdlib vector wrapper temporary capacity method arity mismatch
- rejects vm templated stdlib vector wrapper temporary method index mismatch
- rejects vm templated stdlib vector wrapper temporary index mismatch
- rejects vm templated stdlib vector wrapper temporary index value mismatch
- rejects vm templated stdlib vector wrapper temporary unsafe method index mismatch
- rejects vm vector alias access auto wrapper primitive receiver diagnostics
- rejects vm vector alias access auto wrapper canonical diagnostics forwarding
- rejects vm vector alias access struct method chain with rooted helper diagnostics
- rejects vm vector alias access struct method chain with primitive receiver diagnostics
- rejects vm vector alias access field expression with struct receiver diagnostics
- keeps vm canonical vector access call struct method chain forwarding
- rejects vm canonical vector unsafe access field expression forwarding
- rejects vm map access compatibility call struct method chain with primitive receiver diagnostics
- rejects vm map access compatibility call struct method chain with primitive argument diagnostics
- rejects vm map unsafe compatibility call struct method chain with primitive receiver diagnostics
- rejects vm map unsafe compatibility call struct method chain with primitive argument diagnostics
- rejects vm vector method alias access struct method chain with array receiver diagnostics
- rejects vm vector namespaced access slash methods without alias helper on vector receiver
- rejects vm array compatibility access slash methods on vector receiver
- rejects vm array compatibility access slash method chain before receiver typing
- rejects vm wrapper-returned array compatibility access slash method chains before receiver typing
- rejects vm vector method alias access struct method chain with array receiver diagnostics

### test_compile_run_vm_collections_method_aliases_b.cpp

- rejects vm vector method alias field expression without alias helper
- rejects vm vector unsafe method alias access struct method chain with array receiver diagnostics
- rejects vm vector unsafe method alias field expression without alias helper
- runs vm map method alias access with helper receiver
- keeps canonical vm map method access field expression forwarding
- rejects vm vector method alias struct-return precedence without helper-backed receiver typing
- vm keeps primitive diagnostics for canonical vector method access
- vm keeps struct receiver diagnostics for canonical vector unsafe method access
- rejects vm map method alias access struct method chain with primitive argument diagnostics
- rejects vm wrapper-returned map method alias primitive receiver fallback
- rejects vm wrapper-returned canonical map slash-method struct receiver fallback
- rejects vm wrapper-returned canonical direct-call map receiver fallback
- rejects vm wrapper-returned compatibility direct-call map receiver fallback
- vm keeps wrapper-returned map method alias primitive argument diagnostics
- rejects vm std-namespaced vector method alias access struct method chain with helper receiver diagnostics
- rejects vm std-namespaced vector access slash methods without canonical helper on vector receiver
- rejects vm std-namespaced vector method alias access struct method chain with helper missing-method diagnostics
- rejects vm templated stdlib map wrapper temporary unsafe key mismatch
- rejects vm templated stdlib map return envelope unknown key spelling
- rejects vm templated stdlib map return envelope unsupported value arg
- rejects vm templated stdlib vector return envelope nested arg
- rejects vm templated stdlib map return envelope nested arg
- rejects vm templated stdlib vector return envelope wrong arity
- rejects vm templated stdlib map return envelope wrong arity
- runs vm with stdlib collection shim vector single
- runs vm with stdlib collection shim vector new

### test_compile_run_vm_collections_shim_maps_a.cpp

- runs vm with stdlib collection shim vector new bool element
- rejects vm stdlib collection shim vector single type mismatch
- runs vm with stdlib collection shim vector pair
- rejects vm stdlib collection shim vector pair type mismatch
- runs vm with stdlib collection shim vector triple
- rejects vm stdlib collection shim vector triple type mismatch
- runs vm with stdlib collection shim vector quad
- rejects vm stdlib collection shim vector quad type mismatch
- rejects vm bare stdlib collection shim map single
- rejects vm bare stdlib collection shim map single bool value conversion
- rejects vm stdlib collection shim map single key type mismatch
- rejects vm bare stdlib collection shim map new
- rejects vm bare stdlib collection shim map new string key envelope
- rejects vm bare stdlib collection shim map new bool key envelope
- rejects vm bare stdlib collection shim map new string key envelope mismatch
- rejects vm bare stdlib collection shim map count source
- rejects vm bare stdlib collection shim map count string keys
- rejects vm stdlib collection shim map count type mismatch
- rejects vm stdlib collection shim map count string key type mismatch
- rejects vm bare stdlib collection shim map at source
- rejects vm bare stdlib collection shim map at string keys
- rejects vm stdlib collection shim map at type mismatch
- rejects vm stdlib collection shim map at string key type mismatch
- rejects vm bare stdlib collection shim map at unsafe source
- rejects vm bare stdlib collection shim map at unsafe string keys
- rejects vm stdlib collection shim map at unsafe type mismatch
- rejects vm stdlib collection shim map at unsafe string key type mismatch
- rejects vm bare stdlib collection shim map method access string keys
- rejects vm stdlib collection shim map method at string key type mismatch
- rejects vm stdlib collection shim map method at unsafe string key type mismatch
- rejects vm bare stdlib collection shim map method call parity string keys
- rejects vm stdlib collection shim map method call parity key type mismatch
- rejects vm stdlib collection shim map method call parity unsafe key type mismatch
- rejects vm bare stdlib collection shim map single standalone string keys
- rejects vm stdlib collection shim map single standalone key type mismatch
- rejects vm bare stdlib collection shim map pair standalone
- rejects vm stdlib collection shim map pair standalone type mismatch
- rejects vm bare stdlib collection shim map pair standalone string keys
- rejects vm stdlib collection shim map pair standalone key type mismatch
- rejects vm bare stdlib collection shim map double standalone string keys
- rejects vm stdlib collection shim map double standalone key type mismatch
- rejects vm bare stdlib collection shim map triple standalone string keys

### test_compile_run_vm_collections_shim_maps_b.cpp

- rejects vm stdlib collection shim map triple standalone key type mismatch
- rejects vm bare stdlib collection shim map quad standalone
- rejects vm bare stdlib collection shim map quad standalone string keys
- rejects vm stdlib collection shim map quad standalone type mismatch
- rejects vm stdlib collection shim map quad standalone key type mismatch
- rejects vm bare stdlib collection shim map quint standalone
- rejects vm bare stdlib collection shim map quint standalone string keys
- rejects vm stdlib collection shim map quint standalone type mismatch
- rejects vm stdlib collection shim map quint standalone key type mismatch
- rejects vm bare stdlib collection shim map sext standalone
- rejects vm bare stdlib collection shim map sext standalone string keys
- rejects vm stdlib collection shim map sext standalone type mismatch
- rejects vm stdlib collection shim map sext standalone key type mismatch
- rejects vm bare stdlib collection shim map sept standalone
- rejects vm bare stdlib collection shim map sept standalone string keys
- rejects vm stdlib collection shim map sept standalone type mismatch
- rejects vm stdlib collection shim map sept standalone key type mismatch
- rejects vm bare stdlib collection shim map oct standalone
- rejects vm bare stdlib collection shim map oct standalone string keys
- rejects vm stdlib collection shim map oct standalone type mismatch
- rejects vm stdlib collection shim map oct standalone key type mismatch
- rejects vm bare stdlib collection shim map double
- rejects vm stdlib collection shim map double type mismatch
- rejects vm bare stdlib collection shim map triple
- rejects vm stdlib collection shim map triple type mismatch
- rejects vm bare stdlib collection shim extended constructors
- rejects vm stdlib collection shim extended constructor type mismatch
- runs vm with stdlib collection shim vector quint constructor
- rejects vm stdlib collection shim vector quint type mismatch
- runs vm with stdlib collection shim vector sext constructor
- rejects vm stdlib collection shim vector sext type mismatch
- runs vm with stdlib collection shim vector sept constructor
- rejects vm stdlib collection shim vector sept type mismatch
- runs vm with stdlib collection shim vector oct constructor
- rejects vm stdlib collection shim vector oct type mismatch
- rejects vm bare stdlib collection shim map pair string keys
- rejects vm stdlib collection shim map pair type mismatch
- rejects vm bare stdlib collection shim map quad
- rejects vm stdlib collection shim map quad type mismatch

### test_compile_run_vm_collections_shim_maps_c.cpp

- rejects vm bare stdlib collection shim map quint
- rejects vm stdlib collection shim map quint type mismatch
- rejects vm bare stdlib collection shim map sext
- rejects vm stdlib collection shim map sext type mismatch
- rejects vm bare stdlib collection shim map sept
- rejects vm stdlib collection shim map sept type mismatch
- rejects vm bare stdlib collection shim map oct
- rejects vm stdlib collection shim map oct type mismatch
- rejects vm bare stdlib collection shim access helper map source
- runs vm with stdlib collection shim capacity helper
- runs vm with stdlib collection shim vector capacity
- rejects vm stdlib collection shim vector capacity type mismatch
- runs vm with stdlib collection shim vector count
- runs vm published vector count and capacity on mutable locals
- rejects vm stdlib collection shim vector count type mismatch
- runs vm with stdlib collection shim vector at
- rejects vm stdlib collection shim vector at type mismatch
- runs vm with stdlib collection shim vector at unsafe
- rejects vm stdlib collection shim vector at unsafe type mismatch
- runs vm with stdlib collection shim vector push
- rejects vm stdlib collection shim vector push type mismatch
- runs vm with stdlib collection shim vector pop
- rejects vm stdlib collection shim vector pop type mismatch
- runs vm with stdlib collection shim vector reserve
- rejects vm stdlib collection shim vector reserve type mismatch
- runs vm with stdlib collection shim vector clear
- rejects vm stdlib collection shim vector clear type mismatch
- runs vm with stdlib collection shim vector remove at
- rejects vm stdlib collection shim vector remove at type mismatch
- runs vm with stdlib collection shim vector remove swap
- rejects vm stdlib collection shim vector remove swap type mismatch
- runs vm with stdlib collection shim vector mutators
- runs vm with bare vector capacity through imported stdlib helper
- rejects vm bare vector capacity without imported helper
- runs vm bare vector capacity method without imported helper
- rejects vm wrapper temporary vector capacity method without helper
- runs vm bare vector capacity after pop through imported stdlib helper
- runs vm bare vector mutators without imported helpers
- rejects vm bare vector mutator methods without imported helpers
- runs vm with user array count method shadow

### test_compile_run_vm_collections_vector_aliases_a.cpp

- runs vm bare map at_unsafe through canonical helper
- rejects vm bare map at call without helper
- rejects vm bare map at_unsafe call without helper
- runs vm map namespaced count method through canonical helper
- rejects vm bare map count method without imported canonical helper
- compiles and runs vm bare map contains through canonical helper
- rejects vm bare map contains call without imported canonical helper
- rejects vm bare map contains method without imported canonical helper
- rejects vm bare map tryAt method without imported canonical helper
- rejects vm bare map access methods without imported canonical helpers
- runs vm map namespaced at method through canonical helper
- rejects vm array namespaced vector capacity alias
- rejects vm array namespaced wrapper vector capacity alias
- rejects vm array namespaced wrapper vector count alias
- rejects vm array namespaced vector mutator alias
- rejects vm stdlib canonical vector helper method-precedence forwarding
- rejects vm templated stdlib canonical vector helper method template args
- rejects vm vector namespaced call aliases without alias definitions
- rejects vm vector namespaced templated canonical helper alias call without alias definition
- rejects vm vector alias arity-mismatch compatibility template forwarding
- rejects vm vector alias compatibility template forwarding on bool type mismatch
- rejects vm vector alias compatibility template forwarding on non-bool type mismatch
- rejects vm vector alias compatibility template forwarding on struct type mismatch
- rejects vm vector alias compatibility template forwarding on constructor temporary struct mismatch
- rejects vm vector alias compatibility template forwarding on method-call temporary struct mismatch
- rejects vm vector alias compatibility template forwarding on chained method-call temporary struct mismatch
- rejects vm vector alias compatibility template forwarding on array envelope element mismatch
- rejects vm vector alias compatibility template forwarding on map envelope mismatch
- rejects vm vector alias compatibility template forwarding on map envelope mismatch from call return
- rejects vm local alias slash-method vector count on string receiver
- rejects vm local alias slash-method vector count on array receiver

### test_compile_run_vm_collections_vector_aliases_b.cpp

- rejects vm vector alias compatibility template forwarding on primitive mismatch from call return
- rejects vm vector alias compatibility template forwarding when unknown expected meets primitive call return
- rejects vm vector alias compatibility template forwarding when unknown expected meets primitive binding
- rejects vm vector alias compatibility template forwarding when unknown expected meets vector envelope binding
- rejects vm vector helper method expression legacy alias forwarding
- rejects vm vector alias named-argument compatibility template forwarding
- rejects vm wrapper temporary templated vector method compatibility template forwarding
- rejects vm array alias templated forwarding to canonical vector helper
- rejects vm stdlib templated vector count fallback to array alias
- compiles and runs vm array alias count through same-path helper
- compiles and runs vm array alias capacity through same-path helper
- compiles and runs vm array alias at through same-path helper
- compiles and runs vm array alias at_unsafe through same-path helper
- compiles and runs vm array alias slash-method helpers through same-path helpers
- rejects vm vector alias templated forwarding past non-templated compatibility helper
- runs vm vector namespaced mutator alias
- rejects vm vector namespaced count capacity access aliases without alias definitions
- runs vm with array vector bracket literals and map constructor
- runs vm with array literal count method
- runs vm vector literal count method without imported helper
- runs vm vector method call
- runs vm with array literal unsafe access
- runs vm with bare vector literal unsafe access through imported stdlib helper
- runs vm bare vector at through imported stdlib helper
- rejects vm bare vector at without imported helper
- rejects vm bare vector at method without imported helper
- rejects vm wrapper temporary vector at method without helper
- runs vm bare vector at_unsafe through imported stdlib helper
- rejects vm bare vector at_unsafe without imported helper
- rejects vm bare vector at_unsafe method without imported helper
- rejects vm wrapper temporary vector at_unsafe method without helper
- runs vm with map at helper
- runs vm with map at_unsafe helper
- runs vm with array count helper
- runs vm with array literal count helper
- runs vm with bare vector count through imported stdlib helper
- runs vm with bare vector access through imported stdlib helper
- rejects vm bare vector count without imported helper
- rejects vm bare vector count method without imported helper
- rejects vm wrapper vector count slash-method chains before receiver typing
- rejects vm wrapper vector capacity slash-method chains before receiver typing
- rejects vm local alias slash-method vector capacity on string receiver
- rejects vm local alias slash-method vector capacity on array receiver

### test_compile_run_vm_collections_vector_limits_a.cpp

- runs vm with user vector pop call shadow
- runs vm with user vector pop method canonical precedence
- rejects vm user vector pop call expression shadow
- runs vm with user vector reserve call shadow
- runs vm with user vector reserve method shadow
- rejects vm user vector reserve call expression shadow during lowering
- runs vm with user vector clear call shadow
- rejects vm user vector clear call expression shadow
- rejects vm user vector remove_at call expression shadow during lowering
- rejects vm user vector remove_swap call expression shadow during lowering
- runs vm with user vector clear method canonical precedence
- runs vm with user vector remove_at call shadow
- runs vm with user vector remove_at method canonical precedence
- runs vm with user vector remove_swap call canonical precedence
- runs vm with user vector remove_swap method canonical precedence
- runs vm vector reserve growth through count and capacity helpers
- preserves vm vector values across reserve growth
- runs vm vector push growth through count and capacity helpers
- preserves vm vector values across push growth
- runs vm vector literal at local dynamic limit
- runs vm vector reserve past former local dynamic limit
- rejects vm vector literal above local dynamic limit
- rejects vm vector reserve beyond local dynamic limit
- rejects vm vector reserve negative literal at lowering
- rejects vm vector reserve folded expression beyond local dynamic limit
- rejects vm vector reserve folded negative expression at lowering
- rejects vm vector reserve folded signed overflow at lowering
- rejects vm vector reserve folded negate negative at lowering
- rejects vm vector reserve folded negate overflow at lowering
- rejects vm vector reserve folded unsigned expression beyond local dynamic limit
- rejects vm vector reserve folded unsigned wraparound at lowering
- rejects vm vector reserve folded unsigned add overflow at lowering
- rejects vm vector reserve dynamic value beyond local dynamic limit

### test_compile_run_vm_collections_vector_limits_b.cpp

- runs vm vector push past former local dynamic limit
- rejects vm vector push beyond local dynamic limit
- rejects vm vector shrink helpers during lowering
- rejects vm collection syntax parity helpers during lowering
- rejects vm vector literal count helper during lowering

### test_compile_run_vm_collections_vector_shadow_access.cpp

- rejects vm reordered namespaced vector push call compatibility alias
- rejects vm std namespaced reordered mutator compatibility helper shadow
- rejects vm user vector push bool positional call shadow during lowering
- runs vm with user vector push named call canonical precedence
- runs vm with user vector push method canonical precedence
- rejects vm user vector push call expression shadow during lowering
- rejects vm reordered namespaced vector push call expression compatibility alias
- rejects vm named vector push expression receiver precedence during semantics
- rejects vm auto-inferred named vector push expression receiver precedence during semantics
- rejects vm auto-inferred std namespaced vector push compatibility alias precedence
- runs vm with auto-inferred std namespaced vector push canonical definition
- rejects vm auto-inferred std namespaced count helper compatibility alias precedence
- runs vm with auto-inferred std namespaced count helper canonical fallback
- rejects vm std namespaced count expression compatibility alias precedence
- rejects vm std namespaced count without imported helper
- rejects vm std namespaced count map target without helper
- rejects vm alias count map target without helper
- rejects vm std namespaced capacity map target without helper
- rejects vm alias capacity map target without helper
- vm alias capacity array target accepts same-path helper
- rejects vm alias capacity array target without helper
- runs vm with std namespaced count expression canonical fallback
- rejects vm std namespaced count non-builtin compatibility fallback
- rejects vm vector namespaced count non-builtin array fallback
- rejects vm std namespaced capacity expression compatibility alias precedence
- runs vm with std namespaced capacity expression canonical fallback
- rejects vm auto-inferred named access helper receiver precedence before lowering
- rejects vm auto-inferred std namespaced access helper compatibility alias precedence
- runs vm with auto-inferred std namespaced access helper canonical definition

### test_compile_run_vm_collections_wrapper_temporaries_a.cpp

- rejects vm wrapper temporary vector count method without helper
- vm query-local auto vector helpers run through lowering
- rejects vm experimental soa_vector stdlib helpers
- rejects vm raw soa_vector type spelling
- runs vm public soa count helper on public wrapper
- runs vm public soa get helper
- vm public soa get helper rejects template arguments on non-soa receiver
- vm public soa get slash-method reaches field access reject
- vm public soa to_aos slash-method keeps canonical reject
- runs vm public soa ref helper
- runs vm public soa mutator helpers
- vm public soa to_aos helper lowers
- vm public soa to_aos temporaries route through canonical vector capacity
- vm public soa to_aos explicit helper is a vector target
- vm public soa read helpers route through wrapper paths
- vm public soa construction and mutators use wrappers
- vm public soa from-aos uses wrapper
- vm public soa field-view wrappers use public reads
- vm legacy soa_vector compatibility helpers reject
- vm runs graph-solved direct local-auto vector helper shadows compatibility
- vm rejects experimental soa_vector stdlib wide structs on pending width boundary across imported/direct/helper-return forms
- vm rejects experimental soa_vector stdlib from-aos helper before typed bindings support
- vm runs experimental soa_vector stdlib to-aos helper
- vm runs experimental soa_vector stdlib to-aos method on wrapper surface
- vm no-import root soa_vector to_aos bare and direct helper forms reject SoaVector-only canonical helper contract
- vm no-import root soa_vector to_aos method helper forms reject SoaVector-only canonical helper contract
- vm materializes non-empty root soa_vector struct literals
- vm rejects non-empty root soa_vector literals with unsupported element envelopes
- vm materializes non-empty root soa_vector literals above former local capacity limit
- vm runs experimental soa_vector stdlib non-empty to-aos helper
- vm runs experimental soa_vector stdlib non-empty to-aos method on wrapper state
- runs vm experimental soa_vector stdlib get helper
- runs vm experimental soa_vector stdlib get method
- runs vm bare soa_vector get helper through helper return compatibility
- runs vm global helper-return soa_vector method shadows compatibility
- runs vm method-like helper-return soa_vector method shadows compatibility
- runs vm vector-target old-explicit soa mutator shadows
- runs vm vector-target method soa mutator shadows
- runs vm vector-target to_aos helper shadows
- runs vm nested struct-body soa_vector constructor-bearing helper returns compatibility
- runs vm nested struct-body soa_vector direct and bound helper expressions compatibility
- runs vm nested struct-body soa_vector method shadows compatibility
- runs vm explicit method-like helper-return experimental soa_vector to_aos shadow
- vm runs experimental soa_vector stdlib ref helper
- vm runs experimental soa_vector stdlib ref method
- vm runs experimental soa_vector ref pass-through and return
- runs vm experimental soa_vector stdlib push and reserve helpers
- runs vm experimental soa_vector stdlib push and reserve methods
- vm runs experimental soa_vector single-field index syntax
- vm runs experimental soa_vector reflected multi-field index syntax
- vm runs experimental soa_vector mutating indexed field writes
- vm runs richer borrowed experimental soa_vector mutating indexed field writes
- vm runs method-like borrowed experimental soa_vector mutating indexed field writes
- vm runs borrowed experimental soa_vector reflected index syntax
- vm runs experimental soa_vector bare get and ref field access
- vm runs borrowed local experimental soa_vector reflected index syntax
- vm runs borrowed helper-return experimental soa_vector reflected index syntax
- vm runs experimental soa_vector reflected call-form index syntax
- vm runs experimental soa_vector inline location borrow index syntax
- vm runs dereferenced borrowed helper-return experimental soa_vector reflected index syntax
- vm runs borrowed helper-return experimental soa_vector get/ref methods
- vm runs borrowed helper-return soa_vector ref_ref same-path helper compatibility
- vm runs builtin helper-return soa_vector ref_ref same-path helper
- vm runs borrowed local experimental soa_vector read-only methods
- vm runs inline location experimental soa_vector read-only methods
- vm runs borrowed helper-return experimental soa_vector helper surfaces
- vm runs method-like borrowed helper-return experimental soa_vector helper surfaces
- vm runs direct return borrowed helper-return experimental soa_vector reads
- vm runs direct return method-like borrowed helper-return experimental soa_vector reads
- vm runs direct return inline location borrowed helper-return experimental soa_vector reads
- vm runs inline location method-like borrowed helper-return experimental soa_vector helpers
- vm runs direct return inline location method-like borrowed helper-return experimental soa_vector reads
- vm runs inline location borrowed helper-return experimental soa_vector helpers
- runs vm experimental soa storage helpers
- runs vm experimental soa storage borrowed ref helper
- runs vm experimental soa storage borrowed view helper
- rejects vm experimental soa storage reserve overflow
- runs vm experimental two-column soa storage helpers
- runs vm experimental three-column soa storage helpers
- runs vm experimental four-column soa storage helpers
- runs vm experimental five-column soa storage helpers
- runs vm experimental six-column soa storage helpers
- runs vm experimental seven-column soa storage helpers
- runs vm experimental eight-column soa storage helpers
- runs vm experimental nine-column soa storage helpers
- runs vm experimental ten-column soa storage helpers
- runs vm experimental eleven-column soa storage helpers
- runs vm experimental twelve-column soa storage helpers
- runs vm experimental thirteen-column soa storage helpers
- runs vm experimental fourteen-column soa storage helpers
- runs vm experimental fifteen-column soa storage helpers
- runs vm experimental sixteen-column soa storage helpers
- runs vm with stdlib collection shim helpers
- runs vm with stdlib collection shim multi constructors
- runs vm with templated stdlib collection return envelopes
- runs vm templated stdlib return wrapper temporaries in expressions
- runs vm with templated stdlib wrapper temporary call forms
- runs vm shared stdlib map conformance harness
- runs vm canonical namespaced map helpers on experimental map values
- runs vm wrapper map helpers on experimental map values
- runs vm ownership-sensitive experimental map value methods
- runs vm helper-wrapped inferred experimental map returns
- runs vm helper-wrapped experimental map parameters
- runs vm helper-wrapped experimental map bindings
- runs vm helper-wrapped experimental map assignment RHS values
- runs vm canonical namespaced map constructors on explicit experimental map bindings
- runs vm canonical namespaced map constructors through explicit experimental map returns
- runs vm canonical namespaced map constructors through explicit experimental map parameters
- runs vm wrapper map constructors on explicit experimental map bindings
- runs vm wrapper map constructors through explicit experimental map returns
- runs vm wrapper map constructors through explicit experimental map parameters
- runs vm experimental map variadic constructors
- rejects vm experimental map variadic constructor type mismatch
- runs vm experimental map constructor assignments
- runs vm implicit map auto constructor inference
- runs vm inferred experimental map returns
- runs vm block inferred experimental map returns
- runs vm auto block inferred experimental map returns
- runs vm inferred experimental map call receivers
- runs vm experimental map struct fields
- runs vm inferred experimental map struct fields
- runs vm helper-wrapped inferred experimental map struct fields
- runs vm experimental map method parameters
- runs vm inferred experimental map parameters
- runs vm inferred experimental map default parameters
- runs vm helper-wrapped inferred experimental map default parameters
- runs vm experimental map helper receivers
- runs vm helper-wrapped experimental map helper receivers
- runs vm experimental map method receivers
- runs vm helper-wrapped experimental map method receivers
- runs vm experimental map field assignments
- runs vm helper-wrapped Result.ok experimental map result struct fields
- runs vm dereferenced experimental map storage references
- runs vm helper-wrapped dereferenced Result.ok experimental map result struct fields
- runs vm helper-wrapped experimental map struct storage fields
- runs vm helper-wrapped dereferenced experimental map struct storage fields
- rejects vm canonical namespaced map helpers on borrowed experimental map values
- runs vm canonical namespaced map _ref helpers on borrowed experimental map values
- runs vm experimental map methods
- runs vm borrowed experimental map helpers
- runs vm public borrowed map wrappers
- runs vm borrowed experimental map methods
- runs vm experimental map inserts
- runs vm experimental map ownership-sensitive values
- runs vm canonical namespaced map inserts on explicit experimental map bindings
- runs vm builtin canonical map first-growth inserts
- runs vm builtin canonical map repeated-growth inserts
- runs vm builtin canonical map insert overwrites
- runs vm builtin canonical map non-local growth
- runs vm builtin canonical map nested non-local growth
- runs vm builtin canonical map helper-return borrowed method inserts
- runs vm builtin canonical map struct-field initializer
- runs vm builtin canonical map direct insert on helper-return value receivers
- runs vm builtin canonical map method insert on helper-return value receivers
- runs vm builtin canonical map direct insert on borrowed holder field receivers
- rejects vm canonical map constructor ownership growth
- runs vm experimental map bracket access
- runs vm experimental map custom comparable struct keys
- runs vm shared stdlib vector conformance harness
- runs vm shared vector conformance harness for stdlib and experimental helpers
- runs vm canonical namespaced vector helpers
- runs vm canonical namespaced vector helpers on explicit Vector bindings
- runs vm stdlib wrapper vector helpers on explicit Vector bindings
- rejects vm stdlib wrapper vector helper explicit Vector mismatch
- runs vm stdlib wrapper vector constructors on explicit Vector bindings
- keeps vm stdlib wrapper vector constructor explicit Vector mismatch contract
- runs vm stdlib wrapper vector constructors on inferred auto bindings
- rejects vm stdlib wrapper vector constructor auto inference mismatch
- rejects vm stdlib wrapper vector constructor receivers
- rejects vm stdlib wrapper vector helper receiver mismatch
- rejects vm stdlib wrapper vector method receiver mismatch
- rejects vm canonical namespaced vector constructor temporaries
- rejects vm canonical namespaced vector explicit builtin bindings
- rejects vm canonical namespaced vector named-argument temporaries
- rejects vm canonical namespaced vector named-argument explicit builtin bindings
- rejects vm canonical namespaced vector mutators without imported helpers
- runs vm experimental vector helper runtime contracts
- runs vm experimental vector ownership-sensitive helpers
- runs vm canonical vector helpers on experimental vector receivers
- runs vm vector pop empty runtime contract
- runs vm vector index runtime contract
- runs vm imported container error contract conformance
- runs vm checked pointer conformance harness for imported .prime helpers
- runs vm with templated stdlib vector wrapper temporary call forms
- runs vm templated stdlib vector wrapper temporary methods in expressions
- runs vm with templated stdlib wrapper temporary index forms
- runs vm with templated stdlib wrapper temporary syntax parity
- runs vm with templated stdlib wrapper temporary unsafe parity
- runs vm templated stdlib wrapper temporary count capacity parity

### test_compile_run_vm_collections_wrapper_temporaries_b.cpp

- runs vm with user wrapper temporary at_unsafe shadow precedence
- rejects vm user wrapper temporary unsafe parity shadow mismatch
- rejects vm user wrapper temporary unsafe parity shadow value mismatch
- rejects vm user wrapper temporary unsafe parity shadow arity mismatch
- rejects vm user wrapper temporary unsafe parity shadow missing arguments
- runs vm with user wrapper temporary at shadow precedence
- runs vm with user wrapper temporary count capacity shadow precedence
- rejects vm user wrapper temporary count capacity shadow value mismatch
- runs vm with user wrapper temporary index shadow precedence
- runs vm with user wrapper temporary syntax parity shadow precedence
- rejects vm user wrapper temporary syntax parity shadow mismatch
- rejects vm user wrapper temporary syntax parity shadow value mismatch
- rejects vm templated stdlib collection return envelope unsupported arg
- rejects vm templated stdlib map wrapper temporary key mismatch
- rejects vm templated stdlib map wrapper temporary index key mismatch
- rejects vm templated stdlib wrapper temporary syntax parity key mismatch
- rejects vm templated stdlib wrapper temporary syntax parity value mismatch
- rejects vm templated stdlib wrapper temporary unsafe parity mismatch
- rejects vm templated stdlib wrapper temporary unsafe parity value mismatch
- rejects vm templated stdlib wrapper temporary unsafe parity arity mismatch

### test_compile_run_vm_collections_wrapper_temporaries_c.cpp

- rejects vm templated stdlib wrapper temporary unsafe parity missing arguments
- rejects vm templated stdlib wrapper temporary count capacity parity mismatch
- rejects vm templated stdlib map wrapper temporary index value mismatch
- rejects vm templated stdlib map wrapper temporary call key mismatch
- rejects vm templated stdlib map wrapper temporary call value mismatch
- rejects vm templated stdlib map wrapper temporary unsafe call key mismatch
- rejects vm templated stdlib map wrapper temporary unsafe call value mismatch
- rejects vm templated stdlib map wrapper temporary count key mismatch
- rejects vm templated stdlib map wrapper temporary count value mismatch
- rejects vm templated stdlib map wrapper temporary call arity mismatch
- rejects vm templated stdlib map wrapper temporary call missing key argument
- rejects vm templated stdlib map wrapper temporary unsafe call arity mismatch
- rejects vm templated stdlib map wrapper temporary unsafe call missing key argument
- rejects vm templated stdlib map wrapper temporary count call arity mismatch
- rejects vm templated stdlib map wrapper temporary count method arity mismatch
- rejects vm templated stdlib map wrapper temporary method arity mismatch
- rejects vm templated stdlib map wrapper temporary method missing key argument
- rejects vm templated stdlib map wrapper temporary unsafe method arity mismatch
- rejects vm templated stdlib map wrapper temporary unsafe method missing key argument
- rejects vm templated stdlib vector wrapper temporary call type mismatch
- rejects vm templated stdlib vector wrapper temporary call index mismatch
- rejects vm templated stdlib vector wrapper temporary call arity mismatch
- rejects vm templated stdlib vector wrapper temporary call missing index
- rejects vm templated stdlib vector wrapper temporary unsafe call type mismatch
- rejects vm templated stdlib vector wrapper temporary unsafe call index mismatch
- rejects vm templated stdlib vector wrapper temporary unsafe call arity mismatch
- rejects vm templated stdlib vector wrapper temporary unsafe call missing index
- rejects vm templated stdlib vector wrapper temporary count type mismatch
- rejects vm templated stdlib vector wrapper temporary count call arity mismatch
- rejects vm templated stdlib vector wrapper temporary count method arity mismatch
- rejects vm templated stdlib vector wrapper temporary method arity mismatch

### test_compile_run_vm_core_basic.cpp

- runs vm with method call result
- vm supports support-matrix binding types
- runs vm with raw string literal output
- runs vm with raw single-quoted string literal output
- runs vm with string literal indexing
- runs vm software renderer command serialization deterministically
- runs vm software renderer clip stack serialization deterministically
- runs vm software renderer clip underflow stays deterministic
- runs vm software renderer empty text serialization deterministically
- runs vm two-pass layout tree serialization deterministically
- runs vm two-pass layout empty root deterministically
- runs vm basic widget controls through layout deterministically
- runs vm panel container widget deterministically
- runs vm empty panel container stays balanced deterministically

### test_compile_run_vm_core_file_helpers.cpp

- vm uses stdlib File helper wrappers
- vm uses stdlib File open helper wrappers
- vm stdlib File close helper disarms the original handle
- vm uses stdlib File string helper wrappers
- vm uses stdlib File helper wrappers and broader fallback arities
- vm resolves templated helper overload families by exact arity
- vm supports graphics-style int return propagation with on_error
- vm supports string Result.ok payloads through try
- vm supports direct string Result combinator consumers
- vm supports definition-backed string Result combinator sources

### test_compile_run_vm_core_gfx_helpers.cpp

- vm maps FileError.why codes
- vm uses stdlib ImageError result helpers
- vm uses stdlib ImageError why wrapper
- vm uses stdlib ImageError constructor wrappers
- vm uses stdlib GfxError result helpers
- vm uses canonical stdlib GfxError result helpers
- vm uses canonical stdlib GfxError why helpers
- vm uses canonical stdlib GfxError constructors
- vm uses stdlib experimental Buffer helper methods
- vm uses canonical stdlib Buffer helper methods
- vm uses stdlib experimental Buffer readback helpers
- vm uses canonical stdlib Buffer readback helpers
- vm uses stdlib experimental Buffer allocation helpers
- vm uses canonical stdlib Buffer allocation helpers
- vm uses stdlib experimental Buffer allocation readback path
- vm uses canonical stdlib Buffer allocation readback path
- vm uses stdlib experimental Buffer upload helpers
- vm uses canonical stdlib Buffer upload helpers
- vm uses stdlib FileError why wrapper
- vm uses stdlib FileError eof wrapper
- vm uses stdlib FileError eof constructor wrapper

### test_compile_run_vm_core_results_basic.cpp

- vm supports string return types
- vm supports Result.why hooks
- vm supports stdlib FileError result helpers
- vm supports imported stdlib Result sum pick
- vm supports Result.error on imported stdlib Result sum
- vm supports Result.why on imported stdlib Result sum
- vm supports Result.ok compatibility on imported stdlib Result sum
- vm supports Result.map compatibility on imported stdlib Result sum
- vm supports Result.and_then compatibility on imported stdlib Result sum
- vm supports Result.map2 compatibility on imported stdlib Result sum
- vm supports direct stdlib Result sum sources in compatibility combinators
- vm supports Result.map on IR-backed path
- vm supports Result.and_then on IR-backed path
- vm supports Result.map2 on IR-backed path
- vm supports f32 Result payloads on IR-backed paths
- vm supports direct Result.ok combinator sources on IR-backed paths
- vm supports direct packed ContainerError and ImageError Result payloads on IR-backed paths
- vm supports packed error struct Result combinator payloads on IR-backed paths
- vm supports direct single-slot struct Result.ok payloads on IR-backed paths
- vm supports try on imported stdlib Result sum ok
- vm supports try on imported stdlib Result sum error
- vm supports postfix question on direct imported stdlib Result sum ok
- vm supports postfix question on direct imported stdlib Result sum error
- vm supports dereferenced borrowed stdlib Result sum helpers
- vm propagates imported stdlib Result sum try ok through Result return
- vm propagates imported stdlib Result sum try error through Result return

### test_compile_run_vm_core_results_structs.cpp

- vm supports single-slot struct Result combinator payloads on IR-backed paths
- vm supports direct File Result payloads on IR-backed paths
- vm supports packed File Result combinator payloads on IR-backed paths
- vm supports multi-slot struct Result payloads on IR-backed paths
- vm supports direct array and vector Result payloads on IR-backed paths
- vm supports block-bodied Result.and_then lambdas on IR-backed paths
- vm supports final-if Result.and_then lambdas on IR-backed paths
- vm supports direct map Result payloads on IR-backed paths
- vm supports Buffer Result payloads on IR-backed paths
- vm supports auto-bound direct Result combinator try consumers

### test_compile_run_vm_core_runtime.cpp

- runs vm with heap alloc intrinsic
- runs vm with heap free intrinsic
- runs vm with heap realloc intrinsic
- runs vm with checked memory at intrinsic
- runs vm with unchecked memory at intrinsic
- vm rejects checked memory at out of bounds
- vm rejects dereference after heap free intrinsic
- runs vm with match cases
- runs vm with single task spawn wait
- runs vm with multi task wait tuple
- runs vm with chained method calls
- runs vm with import alias
- runs vm with multiple imports
- runs vm with whitespace-separated imports
- runs vm with capabilities and default effects
- default effects none requires explicit effects in vm
- default effects token does not enable io_err output in vm
- runs vm with implicit utf8 strings
- runs vm with implicit utf8 single-quoted strings
- runs vm with escaped utf8 strings
- runs vm with raw utf8 single-quoted strings

### test_compile_run_vm_core_ui.cpp

- runs vm scene model authoring deterministically
- runs vm ui scene adapter deterministically
- runs vm composite login form deterministically
- runs vm html adapter login form deterministically
- runs vm ui event stream deterministically
- runs vm ui ime event stream deterministically
- runs vm ui resize and focus event stream deterministically
- runs vm with literal method call

### test_compile_run_vm_core_variadics.cpp

- vm rejects recursive calls
- vm accepts string pointers
- vm rejects variadic pointer string packs
- vm rejects variadic reference string packs
- vm ignores top-level executions
- vm materializes variadic Result packs with indexed why and try access
- vm materializes variadic status-only Result packs with indexed error and why access
- vm materializes variadic experimental map packs with indexed canonical count calls
- vm forwards variadic Reference<Buffer> packs through helper methods
- vm forwards variadic Pointer<Buffer> packs through helper methods
- vm materializes variadic pointer uninitialized scalar packs with indexed init and take
- vm materializes variadic borrowed Result packs with indexed dereference try and why access
- vm materializes variadic pointer Result packs with indexed dereference try and why access

### test_compile_run_vm_gpu.cpp

- runs vm with gpu dispatch fallback
- runs vm with canonical stdlib Buffer compute access helpers
- runs vm with experimental stdlib Buffer compute access helpers
- runs vm with gpu dispatch fallback and variadic Buffer packs
- runs vm variadic Reference<Buffer> packs through location/dereference
- runs vm variadic Pointer<Buffer> packs with dereference helpers
- runs vm with gpu dispatch fallback and borrowed variadic Buffer packs
- runs vm with gpu dispatch fallback and pointer variadic Buffer packs
- runs vm with canonical stdlib Buffer arg-pack method receivers

### test_compile_run_vm_maps.cpp

- runs vm with map constructor
- runs vm with map constructor count helper
- runs vm with map count helper
- runs vm with map method call
- rejects vm map indexing sugar without canonical helper
- runs vm with map at_unsafe helper
- rejects vm bool map access helpers without canonical helper
- rejects vm u64 map access helpers without canonical helper
- rejects vm map constructor odd args
- rejects vm map constructor type mismatch
- runs vm with map constructor string binding key
- runs vm with string-keyed map indexing sugar
- runs vm with string-keyed map indexing binding key
- rejects vm map constructor string key from argv binding

### test_compile_run_vm_math.cpp

- runs vm with qualified math names
- rejects vm software numeric types
- runs vm with convert bool from integers
- runs vm float literals
- runs vm float bindings
- rejects vm support-matrix math nominal helpers
- runs vm quaternion multiply and rotation helpers
- runs vm matrix composition order helpers
- runs vm matrix arithmetic helpers with tolerance
- runs vm quaternion arithmetic helpers with tolerance
- rejects vm support-matrix plus mismatch diagnostic
- vm support-matrix implicit conversion remains deterministic

### test_compile_run_vm_maybe.cpp

- runs vm with Maybe some and pick
- runs vm with Maybe none and helper methods
- runs vm with Maybe present variant payload
- rejects retired Maybe mutable helpers with migration diagnostics

### test_compile_run_vm_outputs.cpp

- writes serialized ir output
- no transforms rejects sugar
- no transforms rejects implicit utf8
- writes outputs under out dir
- runs vm file io
- runs vm file read_byte with deterministic eof
- runs vm image api contract deterministically
- runs vm ppm read for ascii p3 inputs
- runs vm ppm read for binary p6 inputs
- rejects truncated vm binary ppm reads deterministically
- rejects oversized vm image read dimensions before overflow
- runs vm ppm write for ascii p3 outputs
- rejects invalid vm ppm write inputs deterministically
- runs vm png write for deterministic rgb outputs
- rejects invalid vm png write inputs deterministically
- rejects oversized vm image write dimensions before overflow
- runs vm png read for stored rgba inputs deterministically
- runs vm png read for stored sub-filter rgba inputs deterministically
- runs vm png read for stored up-filter rgba inputs deterministically
- runs vm png read for stored average-filter rgba inputs deterministically
- runs vm png read for fixed-huffman backreference rgba inputs deterministically
- runs vm png read for dynamic-huffman literal indexed inputs deterministically
- runs vm png read for dynamic-huffman backreference rgba inputs deterministically
- rejects malformed vm png inputs deterministically
- rejects vm png inputs with critical chunk crc mismatches deterministically
- rejects vm png inputs with non-consecutive idat chunks deterministically
- defaults to cpp extension for emit=cpp
- defaults to cpp extension for emit=cpp-ir
- cpp-ir emitter writes IR-lowered cpp for integer subset
- cpp-ir emitter writes string and argv print paths
- cpp-ir emitter writes dynamic string print path
- cpp-ir emitter writes string indexing paths
- cpp emitter uses ir backend for string indexing
- cpp-ir emitter writes pointer indirect paths
- cpp emitter uses ir backend for pointer indirect paths
- cpp-ir emitter writes callable dispatch paths
- cpp-ir emitter writes file read paths
- cpp-ir emitter writes file io paths
- cpp emitter uses ir backend for file io subset
- cpp-ir emitter writes f64 arithmetic paths
- cpp emitter uses ir backend for f64 arithmetic subset
- cpp-ir emitter writes f64 conversion paths
- cpp emitter uses ir backend for f64 conversion subset
- cpp-ir emitter writes f64 to i32 conversion paths
- cpp emitter uses ir backend for f64 to i32 conversion
- cpp-ir emitter writes f32 to i64 conversion paths
- cpp emitter uses ir backend for f32 to i64 conversion
- cpp-ir emitter writes f64 to i64 conversion paths
- cpp emitter uses ir backend for f64 to i64 conversion
- cpp-ir emitter writes f64 to u64 conversion paths
- cpp emitter uses ir backend for f64 to u64 conversion
- cpp-ir emitter writes f64 comparison paths
- cpp emitter uses ir backend for f64 comparison subset
- cpp-ir emitter writes f32 arithmetic and comparison paths
- cpp emitter uses ir backend for f32 arithmetic subset
- defaults to psir extension for emit=ir
- cpp emitter uses ir backend for file read subset
- compiles and runs void main with local binding
- exe-ir emitter compiles and runs i32 subset
- exe-ir emitter compiles and runs i64 subset
- exe-ir emitter compiles and runs argv prints
- exe-ir emitter compiles and runs dynamic string print
- exe-ir emitter compiles and runs string indexing
- exe-ir emitter compiles and runs pointer indirect paths
- exe-ir emitter compiles and runs heap alloc intrinsic
- exe-ir emitter compiles and runs heap free intrinsic
- exe-ir emitter compiles and runs heap realloc intrinsic
- exe-ir emitter compiles and runs checked memory at intrinsic
- exe-ir emitter compiles and runs unchecked memory at intrinsic
- exe-ir emitter faults on checked memory at out of bounds
- exe-ir emitter faults on dereference after heap free intrinsic
- exe-ir emitter compiles and runs file io subset
- exe-ir emitter reports misaligned pointer dereference
- exe-ir emitter compiles and runs call and callvoid paths
- exe-ir emitter compiles and runs f32 arithmetic subset
- exe-ir emitter compiles and runs f64 comparison subset
- exe emitter uses ir backend for f32 arithmetic subset
- exe emitter uses ir backend for string indexing
- exe emitter uses ir backend for pointer indirect paths
- exe emitter uses ir backend for heap alloc intrinsic
- exe emitter uses ir backend for heap free intrinsic
- exe emitter uses ir backend for heap realloc intrinsic
- exe emitter uses ir backend for checked memory at intrinsic
- exe emitter uses ir backend for unchecked memory at intrinsic
- exe emitter uses ir backend for file io subset
- exe-ir emitter compiles and runs f64 arithmetic subset
- exe emitter uses ir backend for f64 comparison subset
- exe emitter uses ir backend for f64 arithmetic subset
- exe-ir emitter compiles and runs f64 conversion subset
- exe emitter uses ir backend for f64 conversion subset
- exe-ir emitter compiles and runs f64 to i32 conversion
- exe emitter uses ir backend for f64 to i32 conversion
- exe-ir emitter clamps f32/f64 to i64 conversion edges
- exe emitter uses ir backend for f32/f64 to i64 conversion edges
- exe-ir emitter truncates in-range f32/f64 to i64
- exe emitter uses ir backend for in-range f32/f64 to i64 truncation
- exe-ir emitter truncates in-range f32/f64 to u64
- exe emitter uses ir backend for in-range f32/f64 to u64 truncation
- cpp and exe emitters match cpp-ir and exe-ir on shared corpus
- cpp and exe diagnostics match cpp-ir and exe-ir (text and json)
- compiles and runs explicit void return
- exe emitter uses ir backend for file read subset
- compiles and runs implicit void main
- compiles and runs argv count
- compiles and runs argv count helper
- compiles and runs argv error output in C++ emitter
- compiles and runs argv error output without newline in C++ emitter
- compiles and runs argv error output u64 index in C++ emitter
- compiles and runs argv unsafe error output in C++ emitter
- compiles and runs argv unsafe line error output in C++ emitter
- compiles and runs argv print in C++ emitter
- compiles and runs argv print without newline in C++ emitter
- compiles and runs argv print with u64 index in C++ emitter
- compiles and runs argv unsafe access in C++ emitter
- compiles and runs argv unsafe access with u64 index in C++ emitter
- compiles and runs three-element array literal
- compiles and runs array literal count method
- compiles and runs array literal unsafe access
- compiles and runs array count helper
- compiles and runs array literal count helper
- compiles and runs literal method call in C++ emitter

### test_compile_run_vm_uninitialized.cpp

- runs vm with uninitialized local storage
- runs vm with uninitialized string storage
- runs vm with pointer-backed uninitialized storage
- runs vm with reference-backed uninitialized storage
- runs vm with pointer-backed uninitialized struct storage
- runs vm with uninitialized borrow
- runs vm with dereferenced uninitialized borrow
- runs vm with uninitialized struct field

## compile_time (30 tests, 3 files)

### test_compile_time_callable.cpp

- builtin meta predicate facts prepare restricted callables
- runtime-only operands fail before compile-time execution
- evaluated user predicates prepare with compile-time arguments
- denied compile-time effect facts reject before callable execution
- compile-time callable preparation enforces deterministic budgets
- unsupported user predicates and operands reject deterministically
- missing facts and preparation budgets fail before execution
- compile-time callable preparation rejects stale requirementPredicateFacts

### test_compile_time_evaluation_facade.cpp

- compile-time evaluation facade names every result and fault
- compile-time evaluation facade formats provenance and results
- compile-time evaluation facade gates effects through the CT host
- compiler-hosted CT facade evaluates published facts without backend artifacts
- compile-time value predicates use shared VM kernel numeric API
- compile-time VM facade stays source locked to compiler-host boundary
- semantic CT host gates effects on phase-qualified metadata
- compile-time evaluation facade reports stable non-success categories
- compile-time evaluation facade wraps published requirement facts
- compile-time evaluation rejects stale or missing requirementPredicateFacts
- compile-time evaluation facade enforces deterministic budgets
- compile-time budget exhaustion diagnostics include provenance
- compile-time evaluation facade reuses deterministic cache results
- compile-time evaluation cache keys include invalidation material
- compile-time evaluation cache rejects corrupt entries
- semantic CT host answers canonical builtin meta predicate facts
- compile-time evaluation facade distinguishes predicate conformance outcomes
- semantic CT host preserves visibility and invalid meta diagnostics

### test_compile_time_value.cpp

- compile-time values preserve kinded equality and hashing
- compile-time values format supported typed facts
- compile-time bool values wrap requirement outcomes
- unsupported runtime-only values reject as invalid evaluation

## import_resolver (6 tests, 1 files)

### test_import_resolver_versions.cpp

- allows comments around version attribute
- allows comments containing > in import list
- resolves highest matching version directory
- rejects missing version match
- versioned archives expand through import resolver
- versioned import succeeds with injected runner on archive path

## ir_pipeline/backends (289 tests, 11 files)

### test_ir_pipeline_backends_architecture.h

- main routes glsl and spirv through ir backends without legacy fallback branches
- design doc records backend boundary policy
- design doc records semantic ownership boundary policy
- design doc records vector map bridge contract
- design doc records stdlib de-experimentalization policy
- design doc records soa public collection contract
- stdlib surface registry stays source locked
- map insert surface registry rejects legacy compatibility spellings
- collection helper surface registry resolves preferred compatibility spellings
- map insert semantic rewrite uses stdlib surface adapter
- gfx buffer semantic rewrite uses stdlib surface adapter
- cmake splits primec library into subsystem targets
- managed backend suite sharding keeps lowering and runtime suites on focused binaries
- include layer guardrail baseline tracks existing private test headers
- glsl and spirv ir backends use glsl ir validation target

### test_ir_pipeline_backends_cpp_vm_a.h

- vm ir backend executes module and returns exit code
- vm ir backend reports invalid ir entry index
- ir serializer backend writes psir magic header
- cpp-ir backend writes C++ source
- cpp-ir backend writes u64 compare source with canonical bool literals
- cpp-ir backend rejects empty non-entry function bodies
- cpp-ir backend reports invalid entry index diagnostics
- cpp-ir backend reports file write string index diagnostics
- cpp-ir backend reports file open string index diagnostics
- cpp-ir backend reports call target range diagnostics
- cpp-ir backend reports jump target range diagnostics
- cpp-ir backend reports unconditional jump target range diagnostics
- cpp-ir backend reports unsupported opcode diagnostics
- cpp-ir backend reports print string index diagnostics
- cpp-ir backend reports string byte load index diagnostics
- cpp-ir backend writes f32 opcode helpers
- cpp-ir backend writes string byte load paths

### test_ir_pipeline_backends_cpp_vm_b.h

- cpp-ir backend writes indirect local pointer paths
- cpp-ir backend emits dup and pop underflow guards
- cpp-ir backend emits arithmetic print file return underflow guards
- cpp-ir backend writes file io paths
- cpp-ir backend writes file write u64 path
- cpp-ir backend writes file write i32 path
- cpp-ir backend writes file write i64 path
- cpp-ir backend writes file write byte path
- cpp-ir backend writes file write newline path
- cpp-ir backend writes file write string path

### test_ir_pipeline_backends_cpp_vm_c.h

- cpp-ir backend writes file read path
- cpp-ir backend writes file write path
- cpp-ir backend writes file append path
- cpp-ir backend omits file io helpers when unused
- cpp-ir backend writes f64 compare helpers
- cpp-ir backend writes f64 arithmetic helpers
- cpp-ir backend writes f64 conversion helpers
- cpp-ir backend omits i32 float conversion clamp helpers when unused
- cpp-ir backend writes u64 float conversion clamp helpers
- cpp-ir backend omits u64 float conversion clamp helpers when unused
- cpp-ir backend writes i64 float conversion clamp helpers
- cpp-ir backend omits i64 float conversion clamp helpers when unused

### test_ir_pipeline_backends_glsl_a.h

- glsl-ir backend writes GLSL source
- glsl-ir backend writes loadstringbyte source
- glsl-ir backend writes pushi64 source for i32-range literals
- glsl-ir backend writes i64 compare source for narrowed values
- glsl-ir backend writes i64 arithmetic source for narrowed values
- glsl-ir backend writes u64 compare source for narrowed values
- glsl-ir backend writes u64 division source for narrowed values
- glsl-ir backend writes i64/u64 to f32 conversion source
- glsl-ir backend writes f32 to i64 conversion source
- glsl-ir backend writes f32 to u64 conversion source
- glsl-ir backend writes f32/f64 passthrough conversion source
- glsl-ir backend writes i32/f64 narrowed conversion source
- glsl-ir backend writes i64/u64 to f64 narrowed conversion source
- glsl-ir backend writes f64 to i64/u64 narrowed conversion source
- glsl-ir backend writes narrowed f64 literal and return source

### test_ir_pipeline_backends_glsl_b.h

- glsl-ir backend writes narrowed f64 add source
- glsl-ir backend writes narrowed f64 sub source
- glsl-ir backend writes narrowed f64 mul source
- glsl-ir backend writes narrowed f64 div source
- glsl-ir backend writes narrowed f64 neg source
- glsl-ir backend writes narrowed f64 equality compare source
- glsl-ir backend writes narrowed f64 inequality compare source
- glsl-ir backend writes narrowed f64 less-than compare source
- glsl-ir backend writes narrowed f64 less-or-equal compare source
- glsl-ir backend writes narrowed f64 greater-than compare source
- glsl-ir backend writes narrowed f64 greater-or-equal compare source
- glsl-ir backend writes file-open-read stub source
- glsl-ir backend writes file-open-write stub source
- glsl-ir backend writes file-open-append stub source

### test_ir_pipeline_backends_glsl_c.h

- glsl-ir backend writes file-close stub source
- glsl-ir backend writes file-flush stub source
- glsl-ir backend writes file-write-i32 stub source
- glsl-ir backend writes file-write-i64 stub source
- glsl-ir backend writes file-write-u64 stub source
- glsl-ir backend writes file-write-string stub source
- glsl-ir backend writes file-write-byte stub source
- glsl-ir backend writes file-write-newline stub source
- glsl-ir backend writes address-of-local source
- glsl-ir backend writes load-indirect source
- glsl-ir backend writes store-indirect source
- glsl-ir backend reports emitter diagnostics
- glsl-ir backend reports print string index diagnostics
- spirv-ir backend reports emitter diagnostics

### test_ir_pipeline_backends_graph_contexts.h

- semantics validator rebuilds base contexts behind explicit validation state
- call target resolution reuses scoped scratch cache
- method target resolution reuses scoped scratch cache
- graph local auto facts use compact structured keys
- type resolution graph builder is wired through semantics testing api
- public semantic-product dump helper is available for pipeline tests
- core IR test helpers expose semantic-product-aware lowering
- public lowerer testing headers stay in sync with semantic-product helper declarations
- public lowerer testing umbrellas keep alias owners ahead of users
- public call dispatch testing header stays in sync with alias-policy helpers
- graph snapshot suite uses semantic-product-aware lowering only
- backend registry keeps semantic-product negative fixture families covered
- ir lowerer header exposes only semantic-product-aware lowering entrypoint
- semantic product routing facts have dedicated public headers
- semantic product routing fact v1 shape markers match public fields
- compile pipeline publishes an initial semantic product shell
- semantic-product consumer coverage matrix stays source locked
- semantic snapshot shared traversal keeps direct and bridge ordering keys
- semantic product bridge path choices use helperNameId without helperName shadow field
- semantic product method call targets use resolvedPathId without resolvedPath shadow field
- semantic product callable summaries use fullPathId without fullPath shadow field
- semantic product binding facts use resolvedPathId without resolvedPath shadow field
- semantic product return facts use definitionPathId without definitionPath shadow field
- semantic product local auto facts use initializerResolvedPathId without initializerResolvedPath shadow field
- semantic product query facts use resolvedPathId without resolvedPath shadow field
- semantic product try facts use operandResolvedPathId without operandResolvedPath shadow field
- semantic product on_error facts use handlerPathId without handlerPath shadow field
- semantic product query and try projections expose stable public lookup keys
- semantic snapshot locals concrete-call canonicalization stays stable
- semantic snapshot method targets concrete-call canonicalization stays stable
- semantic snapshot shared traversal keeps callable summary and on_error ordering keys
- semantic snapshot traversal inventory avoids inactive next-candidate pointer

### test_ir_pipeline_backends_graph_pilot.cpp

- graph type resolver pilot is wired through options and semantics inference

### test_ir_pipeline_backends_graph_utilities.h

- type resolver parity harness is wired through ir pipeline tests
- strongly connected component utility is wired through semantics testing api
- condensation dag utility is wired through semantics testing api

### test_ir_pipeline_backends_registry.cpp

- ir backend registry reports deterministic order and lookup
- emit kind aliases resolve to canonical ir backend kinds
- backend capability registry describes runtime capability availability
- backend capability query routes graphics runtime substrate by target profile
- backend capability query routes runtime reflection by target profile
- ir preparation rejects runtime reflection before unsupported backend lowering
- ir preparation rejects missing semantic-product lowerer preflight runtime reflection ids
- ir preparation rejects stale semantic-product lowerer preflight runtime reflection ids
- all production primec emit kinds route through ir backend resolution
- ir preparation phase manifest pins ordered handoffs
- ir preparation phase manifest documents inline invalidation
- ir preparation helper requires semantic product before lowering
- ir preparation releases lowered AST bodies while preserving source-map provenance
- semantic-product contract rejects missing local-auto facts across entry targets
- semantic-product contract rejects stale local-auto binding types
- compile pipeline semantic handoff gate reaches lowering and rejects stale facts
- published target lookups ignore raw routing facts without maps
- semantic-product coverage validators ignore raw routing facts without maps
- native pick target sum resolution uses semantic-product facts
- native pick payload locals use semantic-product variant metadata
- native pick target sum resolution resolves interned type ids
- native sum construction uses semantic-product variant tags
- native sum slot layout uses semantic-product variant metadata
- native sum variant selection uses semantic-product payload metadata
- native Result combinators use semantic-product variant tags
- native Result combinator payload storage uses semantic-product variant metadata
- native Result combinator sources use semantic-product query facts
- native Result why direct-call sources use semantic-product query facts
- native Result error direct-call sources use semantic-product query facts
- native aggregate pointer dereference calls use semantic-product return facts
- native location direct reference calls use semantic-product return facts
- native field receivers use semantic-product type facts
- native packed Result payloads use semantic-product type facts
- direct Result ok payload metadata uses semantic-product type facts first
- native Result ok emission uses semantic-product payload facts
- native try Result lowering uses semantic-product variant metadata
- native try operand result metadata resolves interned query ids
- native try fact result metadata resolves interned ids
- for-condition auto bindings use semantic-product binding facts
- local-auto statement bindings resolve interned binding ids
- statement binding local info resolves interned binding ids
- statement binding sum lookup uses shared semantic type resolver
- native sum active payload helpers use semantic-product variant tags
- native pick call target sum resolution uses query facts
- native pick method target sum resolution uses query facts
- semantic product callable lookup prefers definition over same-path execution
- compile pipeline freezes published semantic-product string scratch storage
- ir lowerer requires semantic product before lowering
- ir lowerer rejects semantic-product contract version mismatch
- ir lowerer rejects semantic-product module artifact index overflow
- ir lowerer semantic-product completeness manifest covers routing facts
- ir preparation helper reports lowering-stage failure for unresolved entry
- semantic-product direct-call coverage conformance rejects missing targets for published definitions
- ir lowerer production entry rejects missing semantic-product direct-call targets
- semantic-product direct-call coverage conformance rejects missing semantic ids
- ir lowerer rejects stale semantic-product direct-call metadata
- ir lowerer keeps semantic-product direct-call targets authoritative over rooted rewritten expr names
- semantic-product method-call coverage conformance rejects missing targets
- semantic-product method-call coverage conformance rejects missing semantic ids
- method-call coverage rejects missing resolved path ids before lookup gaps
- ir lowerer rejects stale semantic-product method-call metadata
- ir lowerer production entry rejects missing semantic-product method-call targets
- ir lowerer rejects missing semantic-product bridge-path choices
- bridge-path coverage rejects missing helper name ids before lookup gaps
- bridge-path coverage rejects helper name ids before direct-call target gaps
- ir lowerer production entry reports native diagnostic without bridge-path choice
- bridge-path coverage rejects invalid helper name ids before lookup gaps
- ir lowerer rejects stale semantic-product bridge-path metadata
- ir lowerer rejects missing semantic-product binding facts
- ir lowerer rejects semantic-product binding facts missing resolved path ids
- ir lowerer rejects missing semantic-product collection specializations
- ir lowerer rejects missing semantic-product array extent facts
- ir lowerer rejects stale semantic-product collection metadata
- ir lowerer rejects missing semantic-product local binding facts
- ir lowerer rejects missing semantic-product type metadata for struct layouts
- ir lowerer rejects missing semantic-product struct field metadata
- ir lowerer rejects stale semantic-product struct provenance
- ir lowerer completeness checks keep deterministic first-failure order
- semantic-product local-auto call paths accept stdlib surface equivalents
- semantic-product local-auto call paths accept specialized direct calls
- semantic-product local-auto call paths accept overloaded specialized direct calls
- ir preparation rejects semantic-product local-auto path fallback in production mode
- ir lowerer rejects missing semantic-product callable summaries
- ir lowerer effect validation skips semantic-product sum definitions
- ir lowerer rejects missing semantic-product entry parameter facts
- ir lowerer rejects missing semantic-product return facts
- ir lowerer rejects missing semantic-product callable result metadata
- ir lowerer callable result metadata uses interned value ids
- ir lowerer rejects missing semantic-product callable summary path id
- ir lowerer rejects invalid semantic-product callable summary path id
- ir lowerer rejects missing semantic-product return binding types
- ir lowerer result completeness requires published return maps
- ir lowerer rejects missing semantic-product return definition path id
- ir lowerer rejects stale semantic-product return facts
- ir lowerer rejects incomplete semantic-product query facts
- ir lowerer rejects missing semantic-product query resolved path id
- ir lowerer rejects stale semantic-product query facts
- ir lowerer accepts query-owned builtin count target metadata
- ir lowerer rejects stale semantic-product query type metadata
- ir lowerer rejects stale semantic-product query result metadata
- ir lowerer rejects incomplete semantic-product try facts
- ir lowerer rejects missing semantic-product try operand resolved path id
- ir lowerer rejects stale semantic-product try operand metadata
- ir lowerer rejects stale semantic-product try result metadata
- ir lowerer rejects stale semantic-product try context return kind
- ir lowerer rejects stale semantic-product try on_error facts
- ir lowerer rejects missing semantic-product on_error handler path id
- ir lowerer rejects missing semantic-product on_error facts
- ir lowerer rejects stale semantic-product on_error facts
- ir lowerer rejects stale semantic-product on_error result metadata
- ir lowerer rejects mismatched semantic-product on_error bound args
- cli driver preserves parse-stage diagnostic context
- cli driver reports semantic-product availability on post-semantics failures
- compile pipeline result variants keep explicit success failure contract
- compile pipeline result variants expose import failures without success state
- cpp-ir backend accepts semantic-product prepared IR from compile pipeline helper
- vm backend executes semantic-product prepared IR from compile pipeline helper
- semantic-product fact family ownership is explicit
- vm backend conformance keeps semantic-product contract v3
- native backend emits semantic-product prepared IR from compile pipeline helper
- backend conformance keeps semantic-product-owned facts aligned across backends
- backend conformance keeps auto-bound Result combinator facts aligned across backends
- compile pipeline preserves semantic product on post-semantics failure
- compile pipeline skips semantic product for ast-semantic dumps
- compile pipeline builds semantic product for semantic-product dumps
- compile pipeline keeps semantic product for emit paths
- compile pipeline semantic-product generation stays limited to semantic-product dumps and consuming emit paths
- compile pipeline pre-ast dump returns before parser failures
- compile pipeline explicit skip mode omits semantic product for non-consuming paths
- ir pipeline helper parseAndValidate skips semantic product when not requested
- compile pipeline explicit skip mode rejects semantic-product dump requests
- compile pipeline benchmark force-on keeps semantic product for ast-semantic dumps
- compile pipeline benchmark force-off rejects semantic-product dump
- compile pipeline benchmark no-fact-emission keeps semantic product shells empty
- compile pipeline ast-semantic phase counters prove semantic-product build skip
- compile pipeline benchmark semantic phase counters are opt-in and populated
- compile pipeline benchmark semantic allocation counters are opt-in and populated
- compile pipeline benchmark semantic rss checkpoints are opt-in and populated
- compile pipeline benchmark fact-family allowlist keeps only selected collectors
- compile pipeline direct and bridge collector merge keeps output-order parity
- compile pipeline benchmark worker-count stress keeps /std/math/* semantic-product dumps deterministic
- compile pipeline full semantic-product dump golden stays stable across 1,2,4 workers
- compile pipeline benchmark worker-count stress keeps /std/math/* diagnostics deterministic
- compile pipeline benchmark worker-count equivalence keeps /std/math/* diagnostics stable across 1,2,4 workers
- compile pipeline benchmark worker-count equivalence keeps semantic-product families stable despite formatting variance across 1,2,4 workers
- compile pipeline graph-local-auto benchmark shadows preserve published local-auto facts
- lowering variadic reference-pack diagnostic exposes stable public payload
- cli driver maps ir preparation failures through backend diagnostics
- shared vm backend profile exposes canonical diagnostics
- main routes cpp and exe through ir backend alias lookup
- primevm uses shared ir preparation helper
- primevm debug replay uses shared JSON parsing helpers
- ir pipeline helper skips semantic product for non-consuming requests
- semantics helper skips semantic product for non-consuming requests
- backend entrypoint inventory avoids inactive follow-up pointer
- symbol-id migration inventory avoids inactive follow-up pointer

## ir_pipeline/conversions (176 tests, 13 files)

### test_ir_pipeline_conversions_core.h

- ir lowers convert<bool> from u64
- ir lowers convert<bool> from negative i64
- ir treats integer width convert as no-op
- ir lowers location of reference
- ir lowers convert<f32> from i32
- ir lowers convert<int> from float literal
- ir lowerer rejects string comparisons
- ir lowers value block initializers
- ir lowers f32 returns
- ir lowers f64 returns
- ir lowers bool comparison with signed integer
- ir lowerer rejects direct string-keyed map constructor lowering
- ir lowerer supports math-qualified min/max
- ir lowerer supports math lerp with import
- ir lowerer supports math pow with import
- ir lowerer allows local binding named pi
- ir lowerer resolves imported definition calls
- ir lowerer supports hyperbolic math
- ir lowerer supports float pow in native backend
- ir lowerer supports math constant conversions
- ir lowerer supports math-qualified abs/sign/saturate

### test_ir_pipeline_conversions_method_calls_and_argv.cpp

- ir lowerer forwards count to method calls
- ir lowerer supports array method calls
- ir lowerer supports vector method calls
- ir lowerer supports map method calls
- ir lowerer supports entry args print index
- ir lowerer supports entry args print without newline
- ir lowerer supports entry args print_error without newline
- ir lowerer supports entry args print_line_error
- ir lowerer supports entry args print_line_error unsafe
- ir lowerer supports entry args print_error unsafe

### test_ir_pipeline_conversions_numbers.cpp

- ir lowerer supports string vector literals
- ir lowerer supports float bindings
- ir lowerer rejects string literal statements
- ir lowerer rejects lambda expressions
- ir lowerer supports Result.map builtin lambdas
- ir lowerer supports bool payloads for Result.map family
- ir lowerer rejects Result.map wide payloads
- ir lowerer supports Result.map f32 payloads
- ir lowerer supports Result.map with direct Result.ok source
- ir lowerer supports direct packed ContainerError and ImageError Result payloads
- ir lowerer supports packed error struct Result combinator payloads
- ir lowerer supports direct single-slot struct Result.ok payloads
- ir lowerer supports single-slot struct Result combinator payloads
- ir lowerer supports direct File Result payloads
- ir lowerer supports packed File Result combinator payloads
- ir lowerer supports direct multi-slot struct Result.ok payloads
- ir lowerer supports multi-slot struct Result combinator payloads
- ir lowerer supports direct array and vector Result payloads
- retired direct map Result payload literal rejects before lowering
- ir lowerer supports Buffer Result payloads on IR-backed VM paths
- ir lowerer rejects f64 Result payloads that remain unsupported
- ir lowerer supports Result.and_then builtin lambdas
- ir lowerer rejects Result.and_then wide payloads
- ir lowerer supports Result.and_then status-only returns
- ir lowerer supports try on imported status-only Result sum
- ir lowerer propagates imported status-only Result sum errors
- ir lowerer supports try on direct imported status-only Result sums
- ir lowerer propagates direct imported status-only Result sum errors
- ir lowerer supports try on borrowed imported status-only Result sums
- ir lowerer propagates borrowed imported status-only Result sum errors
- ir lowerer supports status-only imported Result helper calls
- ir lowerer supports Result.and_then f32 payloads
- ir lowerer supports Result.and_then with direct Result.ok source
- ir lowerer supports block-bodied Result.and_then lambdas
- ir lowerer supports final-if Result.and_then lambdas
- ir lowerer rejects block-bodied Result.and_then wide payloads
- ir lowerer rejects final-if Result.and_then wide payloads
- ir lowerer supports Result.map2 builtin lambdas
- ir lowerer rejects Result.map2 wide payloads
- ir lowerer supports Result.map2 f32 payloads
- ir lowerer supports Result.map2 with direct Result.ok sources
- ir lowerer supports direct ok Result combinator consumers
- ir lowerer supports direct string Result combinator consumers
- ir lowerer supports definition-backed string Result combinator sources
- ir lowerer preserves inline-call Result metadata from caller-scoped parameter defaults
- ir lowerer rejects wide Result.ok payloads
- ir lowerer accepts move builtin
- ir lowerer rejects non-literal string bindings
- ir lowerer supports print_line with string literals
- ir lowerer supports print_line with string bindings
- ir lowerer supports string binding copy
- ir lowerer supports print_error with string literals
- ir lowerer accepts string literal suffixes
- ir lowerer preserves raw string bytes
- ir lowerer rejects mixed signed/unsigned arithmetic
- ir lowerer rejects mixed signed/unsigned comparison
- ir lowerer rejects dereference of value
- ir lowerer accepts i64 literals
- semantics reject pointer on the right
- semantics reject location of non-local
- ir lowerer supports numeric array literals
- ir lowerer rejects negative loop counts at runtime
- ir lowerer supports numeric vector literals
- ir lowerer supports array access u64 index
- ir lowerer supports array literal unsafe access
- ir lowerer supports array unsafe access
- ir lowerer supports array unsafe access u64 index
- ir lowerer supports entry args count

### test_ir_pipeline_conversions_variadic_basics.cpp

- ir lowerer supports entry args count helper
- ir lowerer supports array count helper
- ir lowerer supports vector count helper
- ir lowerer supports array literal count helper
- ir lowerer supports vector literal count helper
- ir lowerer materializes variadic args packs for direct calls
- ir lowerer forwards pure spread variadic args packs
- ir lowerer materializes string variadic args packs and forwards pure spread
- ir lowerer materializes mixed explicit and spread variadic forwarding
- ir lowerer materializes mixed string spread variadic forwarding
- ir lowerer materializes direct struct variadic args packs for count
- ir lowerer materializes pure spread struct variadic packs for count
- ir lowerer materializes mixed struct spread variadic forwarding for count
- ir lowerer materializes direct struct variadic pack indexing and method access
- ir lowerer materializes pure spread struct pack indexing and method access
- ir lowerer materializes mixed struct pack indexing and method access

### test_ir_pipeline_conversions_variadic_borrowed_vectors.cpp

- retired variadic borrowed vector pack statement mutators no longer run as native vector
- retired variadic borrowed soa_vector count template compatibility rejects before lowering
- retired variadic pointer soa_vector count template compatibility rejects before lowering
- retired variadic soa_vector count template compatibility rejects before lowering
- retired variadic map pack bare count rejects before lowering
- retired variadic map pack indexed contains helpers no longer lower as native map
- retired variadic map pack indexed tryAt inference no longer lowers as native map
- retired variadic map pack canonical count no longer lowers as native map

### test_ir_pipeline_conversions_variadic_collection_refs.cpp

- ir lowerer materializes variadic vector packs with indexed count methods
- ir lowerer materializes variadic vector packs with indexed capacity methods
- ir lowerer materializes variadic vector packs with indexed statement mutators
- ir lowerer materializes variadic array packs with indexed count methods
- ir lowerer materializes variadic borrowed array packs with indexed count methods
- ir lowerer materializes variadic borrowed array packs with indexed dereference access helpers
- ir lowerer materializes variadic scalar reference packs with indexed dereference
- ir lowerer materializes variadic struct reference packs with indexed field and helper access
- ir lowerer materializes variadic struct pointer packs with indexed field and helper access

### test_ir_pipeline_conversions_variadic_field_refs_and_maps.cpp

- ir lowerer rejects variadic struct pointer packs from borrowed pack reference fields
- ir lowerer materializes variadic pointer uninitialized scalar packs with indexed init and take
- ir lowerer materializes variadic borrowed uninitialized scalar packs with indexed init and take
- ir lowerer rejects variadic borrowed map packs with indexed count_ref helpers
- ir lowerer rejects variadic borrowed map packs with indexed dereference count methods
- ir lowerer rejects variadic borrowed map packs with indexed dereference lookup helpers
- ir lowerer rejects variadic borrowed map packs with indexed helper inference

### test_ir_pipeline_conversions_variadic_file_errors.cpp

- ir lowerer materializes variadic FileError packs with indexed why methods
- ir lowerer materializes variadic borrowed FileError packs with indexed dereference why methods
- ir lowerer rejects prefix spread borrowed FileError packs
- ir lowerer materializes variadic pointer FileError packs with indexed dereference why methods
- ir lowerer materializes variadic wrapped FileError packs with named free builtin at receivers
- ir lowerer rejects prefix spread pointer FileError packs

### test_ir_pipeline_conversions_variadic_file_handles.cpp

- ir lowerer materializes variadic File handle packs with indexed file methods
- ir lowerer materializes variadic borrowed File handle packs with indexed dereference file methods
- ir lowerer materializes variadic pointer File handle packs with indexed dereference file methods

### test_ir_pipeline_conversions_variadic_pointer_maps.cpp

- ir lowerer materializes variadic pointer array packs with indexed count methods
- ir lowerer rejects variadic pointer array packs with indexed dereference access helpers
- ir lowerer rejects variadic pointer map packs with indexed count_ref helpers
- ir lowerer rejects variadic pointer map packs with indexed dereference count methods
- ir lowerer rejects variadic pointer map packs with indexed lookup_ref helpers
- ir lowerer rejects variadic pointer map packs with indexed dereference lookup helpers
- ir lowerer rejects variadic pointer map packs with indexed helper inference

### test_ir_pipeline_conversions_variadic_pointer_refs.cpp

- ir lowerer materializes variadic scalar pointer packs with indexed dereference
- ir lowerer rejects variadic scalar pointer packs from borrowed pack access without local binding
- ir lowerer rejects variadic struct pointer packs from borrowed pack access without local binding
- ir lowerer rejects variadic scalar reference packs from borrowed pack access without local binding
- ir lowerer rejects variadic struct reference packs from borrowed pack access without local binding
- ir lowerer rejects variadic scalar pointer packs from borrowed pack field method access
- ir lowerer rejects variadic struct pointer packs from borrowed pack field method access
- ir lowerer rejects variadic scalar pointer packs from borrowed pack reference-field method access

### test_ir_pipeline_conversions_variadic_pointer_vectors.cpp

- ir lowerer materializes variadic pointer vector packs with indexed count methods
- ir lowerer materializes variadic pointer vector packs with indexed dereference capacity methods
- ir lowerer rejects variadic pointer vector packs with indexed dereference access helpers
- ir lowerer rejects variadic pointer vector packs with indexed dereference statement mutators
- ir lowerer materializes variadic borrowed vector packs with indexed count methods
- ir lowerer materializes variadic borrowed vector packs with indexed dereference capacity methods
- ir lowerer rejects variadic borrowed vector packs with indexed dereference access helpers

### test_ir_pipeline_conversions_variadic_results.cpp

- ir lowerer materializes variadic Result packs with indexed why and try access
- ir lowerer materializes variadic borrowed Result packs with indexed dereference try and why access
- ir lowerer materializes variadic pointer Result packs with indexed dereference try and why access
- ir lowerer materializes variadic status-only Result packs with indexed error and why access
- ir lowerer materializes variadic borrowed status-only Result packs with indexed dereference error and why access
- ir lowerer materializes variadic pointer status-only Result packs with indexed dereference error and why access

## ir_pipeline/root (55 tests, 7 files)

### test_ir_pipeline_backends.cpp

- core backend suite registration sentinel

### test_ir_pipeline_entry_args.cpp

- ir lowerer supports entry args print u64 index
- ir lowerer supports entry args print unsafe index
- ir lowerer supports entry args print unsafe u64 index
- ir lowerer tracks unsafe argv bindings
- ir serialize roundtrip preserves unsafe argv prints
- ir lowers stdlib map constructor call as statement

### test_ir_pipeline_external_tooling.cpp

- external tooling prefers glslang validator when present
- external tooling falls back to glslc when needed
- external tooling rejects unknown spirv compiler names
- external tooling uses argv process call for glslang spirv compile
- external tooling uses argv process call for glslc spirv compile
- external tooling uses injected runner for cpp compile command

### test_ir_pipeline_gpu.cpp

- lowers gpu dispatch fallback
- lowers canonical stdlib Buffer compute access helpers
- lowers experimental stdlib Buffer compute access helpers
- lowers gpu dispatch fallback with variadic Buffer packs
- lowers gpu dispatch fallback with borrowed variadic Buffer packs
- lowers gpu dispatch fallback with pointer variadic Buffer packs
- lowers canonical stdlib Buffer arg-pack method receivers

### test_ir_pipeline_pointers.cpp

- ir lowers location of reference binding
- ir lowers pointer plus
- ir lowers pointer plus on reference location
- assign returns value for reference targets
- ir lowers reference to array bindings
- ir lowers location on parameters
- assign returns value for dereference targets
- ir lowers heap realloc pointer sum through dereference offset
- ir lowers pointer minus
- ir lowers pointer minus u64 offsets
- ir lowers pointer minus negative i64 offsets
- pointer plus uses byte offsets in VM
- pointer minus uses byte offsets in VM
- pointer plus accepts negative i64 offsets
- pointer plus accepts i64 offsets
- pointer plus accepts u64 offsets via PushI64
- ir lowers reference assignments

### test_ir_pipeline_pointers_numeric.h

- ir lowers clamp
- ir lowers min
- ir lowers max u64
- ir lowers max mixed i32/i64
- ir lowers abs
- ir lowers sign
- ir lowers saturate i32
- ir lowers saturate u64
- ir lowers clamp u64
- ir lowers clamp mixed i32/i64
- ir lowerer rejects clamp mixed signed/unsigned
- ir lowers convert to int
- ir lowers convert to i64
- ir lowers convert to u64
- ir lowers convert to bool

### test_ir_pipeline_type_resolution_parity.cpp

- default type resolver keeps vm pipeline behavior stable across graph corpus
- graph type resolver intentionally upgrades recursive cycle diagnostics
- graph type resolver still surfaces vm recursive-call lowering limits

## ir_pipeline/serialization (115 tests, 11 files)

### test_ir_pipeline_serialization_calls.h

- ir lowers definition call by inlining
- ir lowers definition call with defaults and named args
- ir lowers void definition call as statement
- ir emits callable function table entries and skips structs
- native backend rejects recursive definition calls
- ir lowers implicit void return
- ir lowers implicit void return without transform
- ir lowers explicit void return
- ir allows expression statements
- ir lowers comparisons and boolean ops
- ir lowers boolean not/or
- ir lowers numeric boolean ops
- ir lowers short-circuit and
- ir lowers short-circuit or
- semantics rejects conflicting auto returns before lowerer entry summary lookup
- semantics rejects unresolved auto return before lowerer entry summary lookup
- semantics rejects unresolved auto return_expr before lowerer entry summary lookup

### test_ir_pipeline_serialization_control_flow_core.h

- ir lowers if/else to jumps
- ir lowers if expression to jumps
- ir lowers repeat to jumps
- repeat count <= 0 skips body
- ir lowers location/dereference assignments
- ir deserialization rejects unknown opcode
- ir serialization round-trips call opcodes
- vm executes call and callvoid opcodes
- vm executes recursive call opcodes
- vm reports missing return in called function
- ir inlining pass inlines straight-line call opcodes
- ir inlining pass keeps recursive call opcodes
- ir inlining pass rejects invalid call targets

### test_ir_pipeline_serialization_control_flow_debug_dump.h

- native emitter debug dump format is deterministic and ordered
- native emitter debug dump includes optimization defaults
- native emitter debug dump sorts duplicate indices by name
- native emitter debug dump docs applied example snapshot stays stable
- native emitter debug dump docs default example snapshot stays stable

### test_ir_pipeline_serialization_control_flow_metadata.h

- ir serializes execution metadata
- ir serialization schema golden fixture stays stable
- ir serializes instruction debug ids
- ir serializes instruction source map metadata
- ir serializes local debug slot metadata
- ir deserialization rejects malformed local debug metadata
- ir deserialization rejects malformed instruction debug id metadata
- ir deserialization rejects malformed instruction source map metadata
- ir deserialization rejects unsupported instruction source map provenance
- ir deserialization rejects unsupported version
- ir marks tail execution metadata
- ir lowerer assigns stable deterministic instruction debug ids
- ir lowerer emits deterministic instruction source map provenance
- compile pipeline IR source maps preserve imported source units
- ir lowerer marks implicit return source map provenance as synthetic
- vm debug adapter preserves lowered source map provenance
- ir leaves tail metadata unset for builtin return
- ir marks tail execution metadata for void direct call tail statement
- ir lowers pointer helper calls

### test_ir_pipeline_serialization_control_flow_native_backend.h

- native backend executes call and callvoid opcodes
- native backend executes recursive call opcodes
- native backend rejects invalid callable IR target
- virtual-register lowering preserves native baseline parity
- native backend reports instrumentation counters per function
- native backend instrumentation counts local load and store ops
- native backend integer stack cache preserves parity and reduces spills
- native backend float stack cache preserves parity and reduces spills
- native backend cache toggle preserves dual-mode parity
- native backend cache mode regression matrix covers branches and call depth
- native backend optimization conformance perf gates enforce parity and thresholds

### test_ir_pipeline_serialization_control_flow_scheduler.h

- scheduler is dependency-safe and latency-aware
- scheduler prioritizes spilled-register latency penalty
- scheduler ties pick lower original instruction index
- scheduler enforces barrier ordering boundaries
- scheduler prioritizes spilled-def latency penalty
- scheduler prioritizes combined spilled use-def penalty
- scheduler rejects allocation name mismatch
- scheduler rejects allocation function count mismatch

### test_ir_pipeline_serialization_control_flow_spills.h

- spill insertion verifies branch and call spill correctness
- spill insertion uses deterministic op ordering for spilled add uses
- spill insertion verifier rejects missing reload ops
- spill insertion verifier rejects missing spill ops
- spill insertion verifier rejects successor index mismatch
- spill insertion verifier rejects edge spill-op mismatch
- spill insertion verifier rejects successor edge count mismatch
- spill insertion verifier rejects function count mismatch

### test_ir_pipeline_serialization_control_flow_verifier.h

- virtual-register verifier accepts valid scheduled allocation output
- virtual-register verifier rejects use-before-def schedules
- virtual-register verifier rejects branch-edge disagreement

### test_ir_pipeline_serialization_control_flow_vregs.h

- virtual-register lowering preserves vm behavior across control flow and calls
- virtual-register lowering rejects inconsistent stack depth merges
- virtual-register liveness builds deterministic loop intervals
- virtual-register liveness tie-breaks equal intervals by register id
- linear-scan allocator has deterministic spill placement on loop cfg
- linear-scan allocator tie-break keeps lower register id resident
- linear-scan allocator rejects unknown spill policy
- linear-scan allocator rejects invalid interval ranges
- linear-scan allocator ignores empty intervals deterministically
- linear-scan allocator spills all intervals with zero registers

### test_ir_pipeline_serialization_struct_layouts.h

- ir emits struct layout metadata
- ir emits std math matrix layout metadata
- ir emits std math quaternion layout metadata
- ir infers omitted struct field envelopes before layout emission
- ir emits struct field visibility and static metadata
- ir emits struct field categories
- ir lowers enum definitions to struct layouts

### test_ir_pipeline_serialization_structs.h

- ir serialize roundtrip and vm execution
- ir lowers semicolon-separated imports
- ir lowers struct brace constructor binding
- ir lowers struct return values
- ir lowers assign from struct-return function call
- semantics reports assign diagnostic for immutable struct local
- ir serialize roundtrip with implicit void return
- ir lowers literal statement with pop
- ir lowers i64 arithmetic
- ir lowers u64 comparison
- ir lowers locals and assign
- ir lowers assign in return expression
- ir lowers increment in return expression
- ir lowers decrement in return expression

## ir_pipeline/to_cpp (37 tests, 2 files)

### test_ir_pipeline_to_cpp.cpp

- ir to cpp emitter writes numeric stack program
- ir to cpp emitter writes i64 arithmetic and comparisons
- ir to cpp emitter writes u64 comparisons with canonical bool literals
- ir to cpp emitter writes f64 to i32 conversion
- ir to cpp emitter writes f32 to i32 conversion clamp helper
- ir to cpp emitter omits i32 float conversion clamp helpers when unused
- ir to cpp emitter writes i64 float conversion clamp helpers
- ir to cpp emitter omits i64 float conversion clamp helpers when unused
- ir to cpp emitter writes u64 float conversion clamp helpers
- ir to cpp emitter writes jump and conditional jump control flow
- ir to cpp emitter writes call and callvoid dispatch
- ir to cpp emitter rejects unsupported opcodes
- ir to cpp emitter rejects invalid entry index
- ir to cpp emitter rejects empty non-entry function bodies
- ir to cpp emitter supports large local indices
- ir to cpp emitter supports large address-of-local index
- ir to cpp emitter supports large file-read local index
- ir to cpp emitter rejects out-of-range call targets
- ir to cpp emitter rejects out-of-range jump targets
- ir to cpp emitter rejects out-of-range string indices
- ir to cpp emitter rejects out-of-range string byte load indices
- ir to cpp emitter rejects out-of-range file write string indices
- ir to cpp emitter rejects out-of-range file open string indices

### test_ir_pipeline_to_cpp_file_io.h

- ir to cpp emitter writes print and argv opcodes
- ir to cpp emitter writes string byte load opcode
- ir to cpp emitter writes indirect local pointer opcodes
- ir to cpp emitter writes file io opcodes
- ir to cpp emitter writes file write u64 opcode
- ir to cpp emitter writes file write i32 opcode
- ir to cpp emitter writes file write i64 opcode
- ir to cpp emitter writes file write byte opcode
- ir to cpp emitter writes file write newline opcode
- ir to cpp emitter writes file write string opcode
- ir to cpp emitter writes file open read opcode
- ir to cpp emitter writes file open write opcode
- ir to cpp emitter writes file open append opcode
- ir to cpp emitter omits file io helpers when unused

## ir_pipeline/to_glsl (58 tests, 2 files)

### test_ir_pipeline_to_glsl.cpp

- ir to glsl emitter writes numeric stack program
- ir to glsl emitter writes jump and conditional jump control flow
- ir to glsl emitter writes call and callvoid opcodes
- ir to glsl emitter writes pushargc opcode
- ir to glsl emitter writes print opcode no-op handling
- ir to glsl emitter writes loadstringbyte opcode
- ir to glsl emitter writes pushi64 opcode when value fits i32
- ir to glsl emitter writes i64 compare and return opcodes
- ir to glsl emitter writes i64 arithmetic opcodes
- ir to glsl emitter writes u64 compare opcodes
- ir to glsl emitter writes u64 division opcode
- ir to glsl emitter writes i64/u64 to f32 conversion opcodes
- ir to glsl emitter writes f32 literal push
- ir to glsl emitter writes f32 arithmetic and compare opcodes
- ir to glsl emitter writes f32 to i32 conversion opcode
- ir to glsl emitter writes f32 to i64 conversion opcode
- ir to glsl emitter writes f32 to u64 conversion opcode
- ir to glsl emitter writes f32/f64 passthrough conversion opcodes
- ir to glsl emitter writes i32/f64 narrowed conversion opcodes
- ir to glsl emitter writes i64/u64 to f64 narrowed conversion opcodes
- ir to glsl emitter writes f64 to i64/u64 narrowed conversion opcodes
- ir to glsl emitter writes i32 to f32 conversion opcode
- ir to glsl emitter writes f32 return opcode

### test_ir_pipeline_to_glsl_runtime.cpp

- ir to glsl emitter writes narrowed f64 literal and return opcodes
- ir to glsl emitter writes narrowed f64 add opcode
- ir to glsl emitter writes narrowed f64 sub opcode
- ir to glsl emitter writes narrowed f64 mul opcode
- ir to glsl emitter writes narrowed f64 div opcode
- ir to glsl emitter writes narrowed f64 neg opcode
- ir to glsl emitter writes narrowed f64 equality compare opcode
- ir to glsl emitter writes narrowed f64 inequality compare opcode
- ir to glsl emitter writes narrowed f64 less-than compare opcode
- ir to glsl emitter writes narrowed f64 less-or-equal compare opcode
- ir to glsl emitter writes narrowed f64 greater-than compare opcode
- ir to glsl emitter writes narrowed f64 greater-or-equal compare opcode
- ir to glsl emitter writes file-open-read stub opcode
- ir to glsl emitter writes file-open-write stub opcode
- ir to glsl emitter writes file-open-append stub opcode
- ir to glsl emitter writes file-close stub opcode
- ir to glsl emitter writes file-flush stub opcode
- ir to glsl emitter writes file-write-i32 stub opcode
- ir to glsl emitter writes file-write-i64 stub opcode
- ir to glsl emitter writes file-write-u64 stub opcode
- ir to glsl emitter writes file-write-string stub opcode
- ir to glsl emitter writes file-write-byte stub opcode
- ir to glsl emitter writes file-write-newline stub opcode
- ir to glsl emitter writes address-of-local opcode
- ir to glsl emitter writes load-indirect opcode
- ir to glsl emitter writes store-indirect opcode
- ir to glsl emitter rejects file-write-string out-of-range index
- ir to glsl emitter rejects address-of-local out-of-range index
- ir to glsl emitter rejects unsupported opcodes
- ir to glsl emitter rejects invalid entry index
- ir to glsl emitter rejects out-of-range local indices
- ir to glsl emitter rejects out-of-range string indices
- ir to glsl emitter rejects out-of-range print string indices
- ir to glsl emitter rejects pushi64 literals outside i32 range
- ir to glsl emitter rejects out-of-range call targets

## ir_pipeline/validation (1362 tests, 90 files)

### test_ir_pipeline_validation_emitter_expr_control_builtin_block_final_value_step_handles_final_statements.cpp

- emitter expr control builtin-block final-value step handles final statements
- emitter expr control builtin-block binding-prelude step prepares binding state
- emitter expr control builtin-block binding-auto step emits auto bindings
- emitter expr control builtin-block binding-explicit step emits typed bindings
- emitter expr control builtin-block binding-fallback step emits inferred bindings
- emitter expr control builtin-block binding-qualifiers step resolves const/reference flags
- emitter expr control builtin-block statement step emits void expression statements
- emitter expr control body-wrapper step rewrites body-argument calls
- emitter expr control if-envelope step recognizes block envelopes
- emitter expr control if-block early-return step emits non-final returns

### test_ir_pipeline_validation_emitter_expr_control_count_rewrite_step_only_rewrites_bare_collection_calls.cpp

- emitter expr control count-rewrite step only rewrites bare collection calls
- emitter expr control builtin-block prelude step gates block handling
- emitter expr control builtin-block early-return step handles non-final returns

### test_ir_pipeline_validation_emitter_expr_control_if_block_final_value_step_emits_final_returns.cpp

- emitter expr control if-block final-value step emits final returns
- emitter expr control if-block binding-prelude step resolves binding metadata
- emitter expr control if-block binding-auto step emits auto bindings
- emitter expr control if-block binding-qualifiers step resolves const/reference flags
- emitter expr control if-block binding-explicit step emits typed bindings
- emitter expr control if-block binding-fallback step emits inferred bindings
- emitter expr control if branch prelude step handles envelope entry cases
- emitter expr control if branch wrapper step wraps branch body
- emitter expr control if branch body step emits ordered statements

### test_ir_pipeline_validation_emitter_expr_control_if_branch_body_return_step_dispatches_returns.cpp

- emitter expr control if branch body return step dispatches returns
- emitter expr control if branch body binding step dispatches bindings
- emitter expr control if branch body dispatch step routes handlers
- emitter expr control if branch body handlers step composes handlers
- emitter expr control if branch value step composes prelude body and wrapper

### test_ir_pipeline_validation_emitter_expr_control_if_branch_emit_step_composes_value_and_handlers.cpp

- emitter expr control if branch emit step composes value and handlers
- emitter expr control if branch body statement step dispatches statements
- emitter expr control if-block statement step emits void statements
- emitter expr control if ternary step emits conditional expression
- emitter expr control if ternary fallback step emits conditional expression
- semantics validator expr capture split step tokenizes captures
- semantics validator statement loop-count step resolves iteration bounds
- ir lowerer lower orchestrator stage order stays stable
- emitter emit setup source delegation stays stable
- emitter builtin collection inference source stays canonical
- soa pending diagnostics route through shared semantics helpers

### test_ir_pipeline_validation_emitter_expr_source_delegation_stays_stable.cpp

- emitter expr contract covers control routing without source locks
- source C++ emitter emits block-argument expression wrappers without source locks
- semantics validator expr source delegation stays stable
- template monomorph source delegation stays stable
- emitter collection helper metadata delegation stays source locked
- emitter collection fallback helpers stay scoped path aware

### test_ir_pipeline_validation_ir_lowerer_call_helpers_build_bundled_entry_call_setup.cpp

- ir lowerer call helpers build bundled entry call setup
- ir lowerer call helpers order positional named and default args
- ir lowerer call helpers skip empty brace block placeholders
- ir lowerer call helpers skip empty brace blocks before packed args
- ir lowerer call helpers reject unknown named arg
- ir lowerer call helpers classify struct helpers and transforms
- ir lowerer call helpers classify struct definitions
- frontend syntax helpers build setup import aliases
- ir lowerer struct type helpers build definition map and struct names
- ir lowerer struct type helpers append layout fields from bindings
- ir lowerer struct type helpers resolve setup import paths
- ir lowerer struct field binding helpers resolve envelope values
- ir lowerer struct field binding helpers infer primitive bindings
- ir lowerer struct field binding helpers extract explicit envelopes
- ir lowerer struct field binding helpers format envelopes

### test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_buffer_and_native_tail_wrappers.cpp

- ir lowerer call helpers dispatch buffer and native tail wrappers
- ir lowerer call helpers resolve and validate map access targets
- ir lowerer call helpers resolve and validate array vector access targets
- ir lowerer native tail wrapper leaves temporary vector receiver to helper lowering

### test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_inline_call_with_count_fallbacks.cpp

- ir lowerer call helpers dispatch inline call with count fallbacks
- ir lowerer call helpers preserve canonical map helper method return chains
- ir lowerer call helpers split legacy and canonical map count defs
- ir lowerer call helpers split legacy and canonical map access defs
- ir lowerer call helpers keep map count and local access same-path defs
- ir lowerer call helpers dispatch bare semantic map sugar inline
- ir lowerer call helpers gate canonical map helpers with semantic target facts
- ir lowerer call helpers prefer graph facts for inline map receiver probes
- ir lowerer call helpers keep vector at methods on builtin path
- ir lowerer call helpers preserve direct wrapper map access defs

### test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_inline_calls_with_locals.cpp

- ir lowerer call helpers dispatch inline calls with locals
- ir lowerer call helpers leave direct experimental map helpers unadapted
- ir lowerer call helpers inline canonical map count and lookup helpers for map locals
- ir lowerer call helpers emit unsupported native call diagnostics for stdlib-only helpers
- ir lowerer inline dispatch emits experimental vector element constructor aliases
- ir lowerer inline dispatch defers experimental vector count methods
- ir lowerer inline dispatch defers experimental vector capacity methods
- ir lowerer inline dispatch defers vector-returning temporary count methods
- ir lowerer inline dispatch defers vector-returning temporary capacity methods
- ir lowerer inline dispatch defers vector-returning temporary count calls
- ir lowerer inline dispatch defers vector-returning temporary capacity calls
- ir lowerer inline dispatch defers vector-returning temporary access calls
- ir lowerer inline dispatch defers vector-returning temporary access methods
- ir lowerer inline dispatch emits vector-returning temporary mutators
- ir lowerer inline dispatch defers vector-returning temporary mutator methods

### test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_native_call_tail_orchestration.cpp

- ir lowerer call helpers dispatch native call tail orchestration
- ir lowerer native unsupported count diagnostics prefer semantic facts

### test_ir_pipeline_validation_ir_lowerer_call_helpers_emit_builtin_array_access.cpp

- ir lowerer call helpers emit builtin array access
- ir lowerer builtin array access prefers semantic target facts
- ir lowerer builtin map access prefers semantic target facts
- ir lowerer call helpers validate non literal string access target
- ir lowerer call helpers key/value key compare opcode selection
- ir lowerer call helpers resolve map lookup string keys
- ir lowerer call helpers emit map lookup string key locals
- ir lowerer call helpers emit map lookup non-string key locals
- ir lowerer call helpers emit map lookup target pointer local
- ir lowerer call helpers emit map lookup key local

### test_ir_pipeline_validation_ir_lowerer_call_helpers_emit_map_lookup_loop_search_scaffold.cpp

- ir lowerer call helpers emit map lookup loop search scaffold
- ir lowerer call helpers emit map lookup access epilogue
- ir lowerer call helpers emit map lookup access
- ir lowerer call helpers emit string access load
- ir lowerer call helpers emit array vector access load
- ir lowerer call helpers emit map lookup loop locals
- ir lowerer call helpers emit map lookup loop condition
- ir lowerer call helpers emit map lookup loop match check
- ir lowerer call helpers emit map lookup loop advance patching
- ir lowerer call helpers emit map lookup at key-not-found guard
- ir lowerer call helpers emit map lookup value load
- ir lowerer call helpers validate map lookup key kinds
- ir lowerer call helpers handle non-method count fallback

### test_ir_pipeline_validation_ir_lowerer_call_helpers_keep_explicit_map_helpers_out_of_native_builtin_emission.cpp

- ir lowerer call helpers infer forwarded collection pair scalar facts
- ir lowerer call helpers keep explicit map helpers out of native builtin emission
- ir lowerer call helpers defer explicit map access for args-pack receivers
- ir lowerer call helpers defer vector metadata and emit safe at while deferring bare count

### test_ir_pipeline_validation_ir_lowerer_call_helpers_source_delegation_stays_stable.cpp

- ir lowerer call-helper contracts replace source delegation locks
- ir lowerer vector type layout traces use generic collection helpers
- ir lowerer collection dispatch metadata resolves through public contracts
- ir lowerer call helpers consume pilot routing semantic-product facts
- ir lowerer call helpers publish root soa constructor metadata without bridge choices
- ir lowerer call helpers keep exact-import vector and map bridge parity
- soa field-view backend cleanup stays stable
- vm heap helpers source delegation stays stable
- vm numeric opcode helpers source delegation stays stable
- vm control flow opcode helpers source delegation stays stable
- ir lowerer effects unit rejects duplicate entry capabilities transform
- ir lowerer call helpers resolve direct definition calls only
- ir lowerer call helpers prefer explicit experimental vector helpers over structs
- ir lowerer call helpers reject missing definition path resolver
- ir lowerer call helpers build definition call resolver
- ir lowerer call helpers resolve definition paths
- ir lowerer call helpers resolve scoped call paths
- ir lowerer call helpers build scoped call path resolver
- ir lowerer call helpers keep alias fallback only on raw path
- ir lowerer call helpers avoid semantic-product scope/root fallback probes
- ir lowerer call helpers fail closed when semantic-product direct-call targets are missing
- ir lowerer call helpers keep unresolved rooted semantic operator targets authoritative
- ir lowerer call helpers keep semantic-product direct-call targets authoritative over rooted rewritten expr names
- ir lowerer call helpers keep rooted rewritten expr names when semantic-product direct-call targets are missing
- ir lowerer call helpers keep source-shaped method paths when semantic-product targets are missing
- ir lowerer semantic-product adapter reuses method-call path ids
- ir lowerer semantic-product adapter rejects call-target source lookup
- ir lowerer semantic-product call-target context separates meaning from provenance
- ir lowerer semantic-product call-target context fails closed on missing meaning
- ir lowerer semantic-product adapter exposes published stdlib surface ids
- ir lowerer semantic-product adapter ignores method-call targets missing resolved path ids
- ir lowerer semantic-product adapter indexes callable summaries by full-path id
- ir lowerer callable summary helper ignores raw summaries without published lookup
- ir lowerer call helpers require semantic-product bridge-path choices
- ir lowerer semantic-product adapter ignores bridge-path choices with missing or invalid helper name ids
- ir lowerer semantic-product adapter joins facts by semantic id without return-path fallback
- ir lowerer semantic-product adapter does not expose return path fallback
- ir lowerer semantic-product index does not expose return path fallback
- ir lowerer semantic-product index requires published return definition-id maps
- ir lowerer return-by-path helper uses published definition-path facts
- ir lowerer return-by-path helper ignores raw path facts without published lookup
- ir lowerer sum metadata helpers use published lookup maps
- ir lowerer sum metadata helpers ignore raw metadata without published lookup
- ir lowerer semantic-product adapter ignores on_error definition-path fallback
- ir lowerer semantic-product index does not expose on_error path fallback
- ir lowerer semantic-product adapter uses on_error semantic-id matches without path fallback
- ir lowerer semantic-product adapter ignores local-auto initializer-path fallback
- ir lowerer semantic-product index does not expose local-auto path fallback
- ir lowerer semantic-product adapter uses local-auto semantic-id matches without path fallback
- ir lowerer semantic-product index requires published binding semantic-id maps
- ir lowerer semantic-product adapter ignores binding scope-name fallback
- ir lowerer statement binding helper consumes semantic-product index directly
- ir lowerer statement binding helper prefers semantic initializer binding facts
- ir lowerer semantic-product adapter ignores query resolved-path fallback
- ir lowerer semantic-product adapter uses query semantic-id matches without path fallback
- ir lowerer semantic-product adapter uses published query semantic-id without path fallback
- ir lowerer semantic-product index does not expose query path fallback
- ir lowerer semantic-product index ignores raw query facts without published lookup
- ir lowerer semantic-product adapter ignores try operand-path fallback
- ir lowerer semantic-product adapter uses try semantic-id matches without path fallback
- ir lowerer semantic-product index ignores raw try facts without published lookup
- ir lowerer semantic-product index does not expose try operand-path fallback
- ir lowerer call helpers keep slashless map import aliases raw
- ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs
- ir lowerer call helpers keep explicit map helper same-path defs
- ir lowerer call helpers keep bare semantic map sugar on canonical defs
- ir lowerer call helpers resolve bare non-semantic contains and tryAt canonical defs
- ir lowerer call helpers prefer canonical remaps over rooted map alias defs
- ir lowerer call helpers reject rooted map alias def families
- ir lowerer call helpers keep lowered collection helper paths reachable via published surface ids
- ir lowerer call helpers classify lowered map helper overloads through semantic surface ids
- ir lowerer call helpers leave experimental map helper calls ordinary
- ir lowerer call helpers prefer specialized rooted raw defs when semantic rooted rewrites miss
- ir lowerer bridge coverage uses published collection surface ids for lowered helper spellings
- ir lowerer direct-call coverage uses published definition and import-alias lookups
- ir lowerer call helpers keep semantic direct-call targets authoritative over rooted rewritten helper paths
- ir lowerer call helpers resolve definition namespace prefixes
- ir lowerer call helpers classify tail call candidates
- ir lowerer call helpers reject missing tail-call resolver
- ir lowerer call helpers build tail-call and definition-exists adapters
- ir lowerer call helpers build bundled call-resolution adapters
- ir lowerer call helpers detect tail execution candidates from statements
- lowerer import aliases are delegated to frontend syntax helpers
- ir lowerer collection surfaces avoid bridge-key literals
- ir lowerer collection literal diagnostics avoid vector-literal traces

### test_ir_pipeline_validation_ir_lowerer_collection_helper_rewrite_guards.cpp

- ir lowerer collection helper rewrite guards explicit map defs
- ir lowerer materialized collection receivers use published helper queries
- ir lowerer late collection constructor guards use published constructor queries
- ir lowerer temp receiver method access skips builtin array lowering
- ir lowerer direct soa wrapper dispatch uses canonical wrapper probes only
- ir lowerer rewrites experimental vector constructor aliases before direct resolution
- ir lowerer collection helper resolves vector aliases before direct definitions
- ir lowerer prefers explicit experimental vector helper before struct constructor defs
- ir lowerer bare vector helper rewrites prefer semantic receiver facts
- ir lowerer materialized collection receivers prefer semantic target facts
- ir lowerer map constructor rewrite checks constructor surface before resolving defs
- ir lowerer constructor metadata helpers retire duplicated constructor tables
- ir lowerer tail dispatch rewrite guards explicit map defs
- ir lowerer tail dispatch semantic query targets resolve interned type ids
- ir lowerer internal soa metadata receivers resolve interned ids
- ir lowerer statement expr has no inline builtin map insert family
- ir lowerer inline map insert helper prefers semantic receiver facts
- ir lowerer vector metadata inline helpers stay in prime definitions
- ir lowerer skips builtin map insert rewrite for direct experimental map locals
- ir lowerer statement collection receiver gates use semantic product ids first
- ir lowerer statement calls step receives semantic product adapters
- ir lowerer statement direct vector receiver fallbacks use semantic target facts
- ir lowerer inline dispatch map helper deferral uses semantic receiver facts first
- ir lowerer inline dispatch vector helper deferral uses semantic receiver facts first
- ir lowerer inline dispatch SoA vector fallback uses semantic receiver facts first
- ir lowerer inline dispatch collection access fallback uses semantic receiver facts first
- ir lowerer tail map insert rewrite uses semantic receiver facts first
- ir lowerer no longer synthesizes generated map value paths
- ir lowerer tail explicit map helper rewrite uses semantic receiver facts first
- ir lowerer tail canonical experimental map helper rewrite uses semantic receiver facts first
- ir lowerer tail borrowed key/value receiver rewrite uses semantic receiver facts first
- ir lowerer native tail map access inference uses semantic receiver facts first
- ir lowerer native tail vector access inference uses semantic receiver facts first
- ir lowerer direct expr and inference rewrites guard explicit map defs
- ir lowerer canonical map contains and tryAt rewrites stay recognized late
- ir lowerer late expression fallback guards explicit map helper defs
- ir lowerer late expression canonical map helpers use path-family gates
- ir lowerer inline native dispatch prefers published canonical map access helpers
- ir lowerer synthetic method probes clear direct helper semantic targets
- ir lowerer binding normalization guards explicit map helper defs
- ir lowerer statement binding args-pack initializer uses semantic target facts
- ir lowerer packed result buffer helpers normalize scoped gfx calls
- ir lowerer rooted semantic rewrite fallback avoids helper allowlists

### test_ir_pipeline_validation_ir_lowerer_conversions_helper_rejects_immutable_assign_target.cpp

- ir lowerer conversions helper rejects immutable assign target
- ir lowerer conversions helper uses semantic mutation target facts before stale locals
- ir lowerer indexed assign consumes semantic collection facts before stale locals
- ir lowerer conversions helper assigns compatible internal soa storage aliases
- ir lowerer conversions helper rejects incompatible internal soa storage aliases
- ir lowerer conversions helper ignores unrelated call names
- ir lowerer conversions control-tail helper lowers if blocks
- ir lowerer conversions control-tail helper treats builtin comparisons as bool conditions
- ir lowerer conversions control-tail helper infers comparison branch values as bool
- ir lowerer conversions control-tail helper rejects incompatible if branch values
- ir lowerer conversions control-tail helper ignores unrelated calls
- ir lowerer return inference helpers infer parameter locals
- ir lowerer return inference helpers keep namespaced File bindings scalar
- ir lowerer return inference helpers keep namespaced File constructor bindings as inferred structs
- ir lowerer return inference helpers recover bool comparison bindings
- ir lowerer return inference helpers report untyped bindings
- ir lowerer return inference helpers reject string references
- ir lowerer return inference helpers reject variadic string references with arg-pack diagnostic
- ir lowerer return inference helpers infer typed value returns
- ir lowerer return inference helpers recover bool comparison returns
- ir lowerer return inference helpers report missing return in error mode
- ir lowerer return inference helpers reject mixed void and value returns
- ir lowerer result helpers resolve Result.ok method
- ir lowerer conversions helper dereferences direct aggregate pointer calls from semantic-product return facts
- ir lowerer conversions helper locations direct reference calls from semantic-product return facts

### test_ir_pipeline_validation_ir_lowerer_count_access_helpers_build_bundled_entry_count_setup.cpp

- ir lowerer count access helpers build bundled entry count setup
- ir lowerer count access helpers prefer semantic product entry args facts
- ir lowerer count access helpers require published entry args binding maps
- ir lowerer count access classifiers prefer semantic direct-name facts
- ir lowerer count access classifiers prefer semantic dereferenced target facts
- ir lowerer count access classifiers prefer semantic indexed target facts
- ir lowerer capacity classifiers prefer semantic target facts
- ir lowerer count access helpers reject removed /array/capacity alias
- ir lowerer count access helpers classify capacity and string count
- ir lowerer count access helpers emit string count calls
- ir lowerer count access helpers emit dynamic string count calls
- ir lowerer count access helpers defer canonical runtime string count calls
- ir lowerer count access helpers defer canonical runtime string facts
- ir lowerer call helpers lower soa_vector count calls

### test_ir_pipeline_validation_ir_lowerer_count_access_helpers_emit_count_access_calls.cpp

- ir lowerer count access helpers emit count access calls
- ir lowerer count access helpers build count classifier adapters
- ir lowerer count access helpers build bundled classifiers
- ir lowerer count access helpers classify canonical counts and defer vector reads
- ir lowerer count access helpers normalize parser-shaped canonical map access receivers
- ir lowerer count access helpers prefer graph facts for runtime string names
- ir lowerer count access helpers defer string map access emission
- ir lowerer string literal helper interns string table values
- ir lowerer string literal helper builds string interner
- ir lowerer string literal helper parses and validates encoding
- ir lowerer string literal helper resolves string table targets
- ir lowerer string literal helper builds string table target resolver
- ir lowerer string literal helper builds bundled context
- ir lowerer string literal helper reports table-target diagnostics
- ir lowerer template type parse helper splits nested template args
- ir lowerer template type parse helper splits template type names
- ir lowerer template type parse helper parses Result return type names
- ir lowerer runtime error helpers emit print-and-return sequence
- ir lowerer runtime error helpers map each helper to expected message
- ir lowerer runtime error helpers emit file-error why dispatch sequence

### test_ir_pipeline_validation_ir_lowerer_entry_setup_step_resolves_entry_metadata.cpp

- ir lowerer entry setup step resolves entry metadata
- ir lowerer entry setup step rejects missing entry
- ir lowerer entry setup step rejects published software numeric preflight facts
- ir lowerer rejects missing semantic-product lowerer preflight software numeric ids
- ir lowerer rejects stale semantic-product lowerer preflight software numeric ids
- ir lowerer entry setup step rejects published runtime reflection preflight facts
- ir lowerer entry setup step rejects missing routing completeness
- ir lowerer imports structs setup step builds maps and layouts
- ir lowerer imports structs setup step rejects unknown field envelopes
- ir lowerer locals setup step resolves orchestration adapters
- ir lowerer locals setup step rejects unsupported string array return
- ir lowerer inference setup bootstrap wires callbacks
- ir lowerer inference setup uses semantic pointer facts before stale locals
- ir lowerer inference setup bootstrap validates dependencies
- ir lowerer inference setup orchestrates callbacks
- ir lowerer inference setup validates dependencies
- ir lowerer inference array-kind setup wires callbacks
- ir lowerer inference array-kind setup uses semantic collection facts before stale locals
- ir lowerer inference array-kind setup validates dependencies
- ir lowerer inference expr-kind base setup wires callback
- ir lowerer inference expr-kind base setup validates dependencies
- ir lowerer inference expr-kind call-base setup wires callback

### test_ir_pipeline_validation_ir_lowerer_expr_emit_setup_dispatches_upload_readback_passthrough_step.cpp

- ir lowerer expr emit setup dispatches upload/readback passthrough step
- ir lowerer expr emit setup validates upload/readback dispatch dependencies
- emitter emit setup math-import step detects supported imports
- emitter emit setup lifecycle helper step matches helper suffixes
- emitter expr control name step resolves constants and locals
- emitter primitive return inference keeps scoped builtin control calls
- emitter expr control float-literal step formats literals
- emitter expr control integer-literal step formats literals
- emitter expr control bool-literal step formats literals
- emitter expr control string-literal step formats literals
- emitter expr control field-access step formats receiver access
- emitter expr control call-path step rewrites non-method call names
- emitter helpers types require explicit slashes for map import aliases
- emitter helpers struct types source stays free of map alias normalization
- emitter helpers expose source Result cpp bridge types
- emitter helper path preference preserves slashless map helper candidates
- emitter expr control method-path step rewrites eligible method calls
- emitter count rewrite skips explicit canonical vector capacity helpers
- emitter count rewrite rejects removed array capacity alias

### test_ir_pipeline_validation_ir_lowerer_file_write_helpers_emit_write_bytes_loops.cpp

- ir lowerer file write helpers emit write-bytes loops
- ir lowerer file write helpers dispatch file-handle methods
- ir lowerer file write helpers emit flush and close calls
- ir lowerer string call helpers emit values from locals
- ir lowerer string call helpers emit entry-args call values from locals
- ir lowerer string call helpers surface errors and not-handled cases

### test_ir_pipeline_validation_ir_lowerer_file_write_helpers_resolve_write_opcodes.cpp

- ir lowerer file write helpers resolve write opcodes
- ir lowerer file write helpers resolve and emit file open modes
- ir lowerer file write helpers dispatch File constructor calls
- ir lowerer file write helpers prefer graph facts for File paths
- parseAndValidate rewrites imported File constructor entry points through stdlib open helpers
- ir lowerer file write helpers emit write steps
- ir lowerer file write helpers emit write and write_line calls
- ir lowerer file write helpers emit write_byte calls
- ir lowerer file write helpers emit read_byte calls
- ir lowerer file write helpers emit write_bytes calls

### test_ir_pipeline_validation_ir_lowerer_flow_helpers_emit_counted_loop_scaffolding.cpp

- ir lowerer flow helpers emit counted loop scaffolding
- ir lowerer flow helpers recover bool while conditions from builtin comparisons
- ir lowerer binding local info recovers bool comparison initializers
- ir lowerer binding local info clears struct paths for file handles
- ir lowerer statement bindings prefer struct constructor full paths
- ir lowerer statement bindings only rehydrate unresolved value structs
- ir lowerer storage helpers skip file handle struct copies
- ir lowerer flow helpers emit body statements
- ir lowerer flow helpers emit body statements with file scope
- ir lowerer flow helpers declare for-condition bindings
- ir lowerer flow helpers recover bool for comparison-backed for-condition bindings
- ir lowerer flow helpers init for-condition bindings

### test_ir_pipeline_validation_ir_lowerer_flow_helpers_resolve_buffer_load_info.cpp

- ir lowerer flow helpers resolve buffer load info
- ir lowerer flow helpers emit buffer load calls
- ir lowerer flow helpers emit buffer builtin calls
- ir lowerer flow helpers use semantic buffer facts before stale locals
- ir lowerer string call helpers emit literal and binding values

### test_ir_pipeline_validation_ir_lowerer_inference_call_return_setup_resolves_canonical_namespaced_map_access_direct.cpp

- ir lowerer inference call-return setup resolves canonical namespaced map access directly
- ir lowerer inference call-return setup resolves canonical namespaced map at unsafe directly
- ir lowerer inference call-return setup resolves namespaced push definition directly
- ir lowerer inference call-return setup resolves namespaced pop definition directly
- ir lowerer inference expr-kind call-return setup validates dependencies
- ir lowerer inference return-info setup handles declared return transforms
- ir lowerer inference return-info setup treats pointerish returns as address-like i64
- ir lowerer inference return-info setup infers auto returns
- ir lowerer inference return-info setup infers implicit auto canonical map returns
- ir lowerer inference return-info setup validates dependencies
- ir lowerer inference get-return-info step resolves and caches returns

### test_ir_pipeline_validation_ir_lowerer_inference_call_return_setup_resolves_namespaced_capacity_definition_directl.cpp

- ir lowerer inference call-return setup resolves namespaced capacity definition directly
- ir lowerer inference call-return setup resolves namespaced access definition directly
- ir lowerer inference call-return setup rejects bare semantic access receiver facts
- ir lowerer inference call-return setup rejects bare semantic access reorder facts
- ir lowerer inference call-return setup rejects bare semantic access stale facts
- ir lowerer inference call-return setup uses semantic contains receiver facts before local metadata
- ir lowerer inference call-return setup uses semantic count receiver facts before local metadata
- ir lowerer inference call-return setup uses semantic count tail facts before stale strings
- ir lowerer inference call-return setup uses semantic method access receiver facts before stale locals
- ir lowerer inference call-return setup uses semantic unresolved builtin receiver facts before stale locals
- ir lowerer inference call-return setup uses semantic vector mutator receiver facts before stale locals
- ir lowerer inference call-return setup uses semantic capacity receiver facts before stale locals
- ir lowerer inference call-return setup rejects vector compatibility array access fallback
- ir lowerer inference call-return setup resolves namespaced at unsafe definition directly
- ir lowerer inference call-return setup resolves canonical namespaced map count directly
- ir lowerer inference call-return setup keeps explicit compatibility map count on direct fallback
- ir lowerer inference call-return setup keeps unresolved compatibility map count without definitions
- ir lowerer inference call-return setup rejects explicit map aliases against canonical-only defs

### test_ir_pipeline_validation_ir_lowerer_inference_expr_kind_call_base_setup_infers_try_from_indexed_borrowed_and_po.cpp

- ir lowerer inference expr-kind call-base setup infers try from indexed borrowed and pointer Result args packs
- ir lowerer inference expr-kind call-base setup uses semantic query facts for scalar call kinds
- ir lowerer inference expr-kind call-base setup uses semantic File constructor facts
- ir lowerer inference expr-kind call-base setup uses semantic try operand Result facts
- ir lowerer inference expr-kind call-base setup uses semantic try file method receiver facts
- ir lowerer inference expr-kind call-base setup uses semantic field receiver facts
- ir lowerer inference expr-kind call-base setup uses semantic take and borrow query facts
- ir lowerer inference expr-kind call-base setup uses semantic Result.ok payload facts
- ir lowerer inference expr-kind call-base setup does not infer missing Result method facts from fallback
- ir lowerer inference expr-kind call-base setup keeps syntax Result method fallback
- ir lowerer inference expr-kind call-base setup uses semantic FileError receiver facts
- ir lowerer inference expr-kind call-base setup uses semantic File receiver facts
- ir lowerer inference expr-kind call-base setup uses semantic query and local-auto receiver facts
- ir lowerer inference expr-kind call-base setup does not infer missing query facts from fallback
- ir lowerer inference expr-kind call-base setup uses semantic try facts before local Result state
- ir lowerer inference expr-kind call-base setup does not infer missing try facts from local Result state
- ir lowerer inference expr-kind call-base setup uses semantic map receiver facts
- ir lowerer semantic-product index requires published query and try semantic-id maps
- ir lowerer inference expr-kind call-base setup leaves builtin comparison kind unresolved without semantic facts
- ir lowerer inference expr-kind call-base setup validates dependencies
- ir lowerer inference base-kind helpers resolve parser-shaped canonical map result helpers
- ir lowerer inference expr-kind call-return setup wires callback
- ir lowerer inference expr-kind call-return setup supports deferred return-info wiring
- ir lowerer inference call-return setup resolves namespaced count definition directly
- ir lowerer inference call-return setup rejects vector alias count without compatibility definition
- ir lowerer inference call-return setup rejects slashless vector alias count without compatibility definition
- ir lowerer inference call-return setup rejects canonical return-info forwarding from compatibility vector count defs
- ir lowerer inference call-return setup treats removed array count aliases as direct definitions
- ir lowerer inference call-return setup keeps removed array count aliases unresolved without definitions
- ir lowerer inference call-return setup treats removed array capacity aliases as direct definitions
- ir lowerer inference call-return setup keeps removed array capacity aliases unresolved without definitions

### test_ir_pipeline_validation_ir_lowerer_inference_expr_kind_dispatch_infers_try_from_indexed_map_tryat_args_pack_lo.cpp

- ir lowerer inference expr-kind dispatch infers try from indexed map tryAt args pack lookups
- ir lowerer inference expr-kind dispatch rejects stale indexed map value facts
- ir lowerer inference expr-kind dispatch uses semantic map receiver facts
- ir lowerer inference expr-kind dispatch uses semantic try operand Result facts
- ir lowerer inference expr-kind dispatch uses semantic name facts before stale locals
- ir lowerer inference expr-kind dispatch uses semantic method receiver facts
- ir lowerer inference expr-kind dispatch uses semantic File call facts
- ir lowerer inference expr-kind dispatch uses semantic Result method facts
- ir lowerer inference expr-kind dispatch setup validates dependencies
- ir lowerer inference expr-kind call-fallback setup wires callback
- ir lowerer inference expr-kind call-fallback setup validates dependencies
- ir lowerer inference expr-kind call-operator fallback setup wires callback
- ir lowerer inference expr-kind call-operator fallback setup validates dependencies
- ir lowerer inference expr-kind call-operator fallback setup validates state dependencies
- ir lowerer inference expr-kind call-control-flow fallback setup wires callback
- ir lowerer inference expr-kind call-control-flow fallback setup validates dependencies
- ir lowerer inference expr-kind call-pointer fallback setup wires callback
- ir lowerer inference expr-kind call-pointer fallback setup validates state dependencies
- ir lowerer return/calls setup wires emitFileErrorWhy callback
- ir lowerer return/calls setup validates dependencies
- ir lowerer statements/calls step emits assign-or-expr fallback

### test_ir_pipeline_validation_ir_lowerer_inference_get_return_info_step_reports_missing_definitions.cpp

- ir lowerer inference get-return-info step reports missing definitions
- ir lowerer inference get-return-info step treats sums as type returns
- ir lowerer inference get-return-info step treats semantic-product sums as type returns
- ir lowerer inference get-return-info step rejects recursive lookup
- ir lowerer inference get-return-info step validates dependencies
- ir lowerer inference get-return-info callback setup wires callback
- ir lowerer inference get-return-info keeps file handles scalar
- ir lowerer inference get-return-info callback setup validates dependencies
- ir lowerer inference get-return-info setup wires callback
- ir lowerer inference get-return-info setup validates dependencies
- ir lowerer inference get-return-info step uses semantic-product return metadata
- ir lowerer inference get-return-info setup precomputes semantic-product return metadata
- ir lowerer inference get-return-info setup requires published callable summary maps
- ir lowerer inference rejects canonical count auto wrappers without semantic product
- ir lowerer inference expr-kind dispatch setup wires callback
- ir lowerer inference expr-kind dispatch prefers builtin comparison fallback over unknown direct return
- ir lowerer inference expr-kind dispatch infers try from namespaced File constructors
- ir lowerer inference expr-kind dispatch rejects stale indexed pointer Result args packs

### test_ir_pipeline_validation_ir_lowerer_inline_call_return_value_step_emits_expected_instructions.cpp

- ir lowerer inline-call return-value step emits expected instructions
- ir lowerer inline-call return-value step validates dependencies
- ir lowerer statements entry-execution step wires context and cleanup
- ir lowerer statements entry-execution step validates dependencies
- ir lowerer statements function-table step finalizes entry function
- ir lowerer statements function-table step validates dependencies
- ir lowerer statements source-map step finalizes instruction metadata
- ir lowerer statements source-map step validates dependencies
- ir lowerer expr emit setup wires unary passthrough callbacks
- ir lowerer expr emit setup validates unary callback dependencies
- ir lowerer expr emit setup dispatches move passthrough step
- ir lowerer expr emit setup validates move dispatch dependencies

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_aliases_pure_fileerror_variadic_forwarding.cpp

- ir lowerer inline param helper aliases pure FileError variadic forwarding
- ir lowerer inline param helper aliases pure File handle variadic forwarding
- ir lowerer inline param helper rejects FileError variadic alias type mismatch
- ir lowerer inline param helper rejects File handle variadic alias type mismatch
- ir lowerer inline param helper materializes borrowed File handle variadic args packs
- ir lowerer inline param helper materializes borrowed FileError variadic args packs
- ir lowerer inline param helper aliases pure borrowed FileError variadic forwarding
- ir lowerer inline param helper aliases pure borrowed File handle variadic forwarding

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_aliases_pure_map_variadic_forwarding.cpp

- ir lowerer inline param helper aliases pure map variadic forwarding
- ir lowerer inline param helper rejects map variadic alias type mismatch
- ir lowerer inline param helper aliases pure struct variadic forwarding
- ir lowerer inline param helper materializes direct struct variadic args packs
- ir lowerer inline param helper materializes mixed struct variadic forwarding
- ir lowerer inline param helper bridges canonical builtin soa helpers while rejecting direct experimental conversion helpers
- ir lowerer inline param helper bridges builtin soa for canonical count and get callee paths
- ir lowerer setup type helper combines numeric kinds
- ir lowerer setup type helper normalizes bool comparison kinds
- ir lowerer setup type helper rejects mixed bool float comparison kinds
- ir lowerer setup type helper builds value-kind adapters
- ir lowerer setup type helper builds bundled adapters
- ir lowerer setup math helper detects math imports
- ir lowerer struct type helpers join template args text
- ir lowerer struct type helpers resolve scoped struct paths
- ir lowerer struct type helpers build scoped struct path resolver
- ir lowerer struct type helpers resolve definition namespace prefixes from map

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_borrowed_array_variadic_args_packs.cpp

- ir lowerer inline param helper materializes borrowed array variadic args packs
- ir lowerer inline param helper aliases pure borrowed array variadic forwarding
- ir lowerer inline param helper materializes Buffer variadic args packs
- ir lowerer inline param helper aliases pure Buffer variadic forwarding
- ir lowerer inline param helper materializes borrowed Buffer variadic args packs
- ir lowerer inline param helper materializes pointer Buffer variadic args packs
- ir lowerer inline param helper aliases pure borrowed Buffer variadic forwarding
- ir lowerer inline param helper aliases pure pointer Buffer variadic forwarding

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_borrowed_result_variadic_args_packs.cpp

- ir lowerer inline param helper materializes borrowed Result variadic args packs
- ir lowerer inline param helper aliases pure borrowed Result variadic forwarding
- ir lowerer inline param helper rejects borrowed Result variadic alias type mismatch
- ir lowerer inline param helper materializes pointer Result variadic args packs
- ir lowerer inline param helper aliases pure pointer Result variadic forwarding
- ir lowerer inline param helper rejects pointer Result variadic alias type mismatch
- ir lowerer inline param helper rejects variadic pointer string packs with arg-pack diagnostic
- ir lowerer inline param helper rejects variadic string reference packs with arg-pack diagnostic

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_borrowed_vector_variadic_args_packs.cpp

- ir lowerer inline param helper materializes borrowed vector variadic args packs
- ir lowerer inline param helper aliases pure borrowed vector variadic forwarding
- ir lowerer inline param helper materializes borrowed soa_vector variadic args packs
- ir lowerer inline param helper aliases pure borrowed soa_vector variadic forwarding
- ir lowerer inline param helper materializes direct borrowed imported SoaVector values
- ir lowerer inline param helper materializes mixed borrowed soa_vector variadic forwarding
- ir lowerer inline param helper materializes soa_vector variadic args packs
- ir lowerer inline param helper aliases pure soa_vector variadic forwarding
- ir lowerer inline param helper rejects soa_vector variadic alias type mismatch
- ir lowerer inline param helper rejects borrowed map variadic alias type mismatch

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_pointer_array_variadic_args_packs.cpp

- ir lowerer inline param helper preserves generic pointer target metadata
- ir lowerer inline param helper materializes pointer array variadic args packs
- ir lowerer inline param helper aliases pure pointer array variadic forwarding
- ir lowerer inline param helper materializes borrowed map variadic args packs
- ir lowerer inline param helper aliases pure borrowed map variadic forwarding
- ir lowerer inline param helper materializes pointer map variadic args packs
- ir lowerer inline param helper materializes pointer vector variadic args packs
- ir lowerer inline param helper aliases pure pointer map variadic forwarding
- ir lowerer inline param helper aliases pure pointer vector variadic forwarding
- ir lowerer inline param helper aliases pure pointer soa_vector variadic forwarding

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_pointer_soa_vector_variadic_args_packs.cpp

- ir lowerer inline param helper materializes pointer soa_vector variadic args packs
- ir lowerer inline param helper aliases pure pointer soa_vector variadic forwarding
- ir lowerer inline param helper materializes mixed pointer soa_vector variadic forwarding
- ir lowerer inline param helper rejects pointer map variadic alias type mismatch
- ir lowerer inline param helper rejects pointer vector variadic alias type mismatch
- ir lowerer inline param helper rejects pointer soa_vector variadic alias type mismatch
- ir lowerer inline param helper rejects pointer array variadic alias type mismatch
- ir lowerer inline param helper rejects Buffer variadic alias type mismatch
- ir lowerer inline param helper rejects borrowed Buffer variadic alias type mismatch
- ir lowerer inline param helper rejects pointer Buffer variadic alias type mismatch
- ir lowerer inline param helper materializes direct pointer imported SoaVector values

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_rejects_borrowed_fileerror_variadic_alias_type_mismatch.cpp

- ir lowerer inline param helper rejects borrowed FileError variadic alias type mismatch
- ir lowerer inline param helper rejects borrowed File handle variadic alias type mismatch
- ir lowerer inline param helper clears stale struct paths on file-handle params
- ir lowerer inline param helper materializes pointer File handle variadic args packs
- ir lowerer inline param helper aliases pure pointer File handle variadic forwarding
- ir lowerer inline param helper rejects pointer File handle variadic alias type mismatch
- ir lowerer inline param helper materializes pointer FileError variadic args packs
- ir lowerer inline param helper aliases pure pointer FileError variadic forwarding
- ir lowerer inline param helper rejects pointer FileError variadic alias type mismatch

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_rejects_borrowed_vector_variadic_alias_type_mismatch.cpp

- ir lowerer inline param helper rejects borrowed vector variadic alias type mismatch
- ir lowerer inline param helper rejects borrowed soa_vector variadic alias type mismatch
- ir lowerer inline param helper preserves borrowed soa_vector spread struct metadata
- ir lowerer inline param helper materializes struct pointer variadic args packs
- ir lowerer inline param helper aliases pure struct pointer variadic forwarding
- ir lowerer inline param helper materializes mixed struct pointer variadic forwarding
- ir lowerer inline param helper rejects struct pointer variadic alias type mismatch
- ir lowerer inline param helper rejects borrowed array variadic alias type mismatch
- ir lowerer inline param helper rejects array variadic alias type mismatch
- ir lowerer inline param helper materializes map variadic args packs
- ir lowerer inline param helper aliases immutable concrete map params from map locals
- ir lowerer inline param helper aliases mutable concrete map params from map locals
- ir lowerer inline param helper accepts bare std ui mutable struct params
- ir lowerer inline param helper accepts bare std ui immutable struct params

### test_ir_pipeline_validation_ir_lowerer_inline_param_helper_rejects_variadic_reference_packs_without_location_forwa.cpp

- ir lowerer inline param helper rejects variadic reference packs without location forwarding
- ir lowerer inline param helper rejects variadic pointer packs without location forwarding
- ir lowerer inline param helper materializes pointer packs from borrowed pack access
- ir lowerer inline param helper materializes mixed variadic forwarding
- ir lowerer inline param helper materializes string variadic args packs
- ir lowerer inline param helper materializes vector variadic args packs
- ir lowerer inline param helper materializes array variadic args packs
- ir lowerer inline param helper aliases pure vector variadic forwarding
- ir lowerer inline param helper aliases pure array variadic forwarding

### test_ir_pipeline_validation_ir_lowerer_inline_struct_arg_helper_reports_diagnostics.cpp

- ir lowerer inline struct arg helper reports diagnostics
- ir lowerer inline struct arg helper accepts compatible soa vector storage
- ir lowerer inline struct arg helper accepts internal soa storage aliases
- ir lowerer inline struct arg helper accepts std ui struct aliases
- ir lowerer inline struct arg helper accepts expected brace field constructors
- ir lowerer inline struct arg helper rejects incompatible internal soa storage aliases
- ir lowerer inline param helper emits non-struct parameter flow
- ir lowerer inline param helper reports diagnostics
- ir lowerer inline param helper adopts inferred vector auto params
- ir lowerer inline param helper preserves explicit vector argument metadata
- ir lowerer inline param helper materializes variadic args packs
- ir lowerer inline param helper materializes FileError variadic args packs
- ir lowerer inline param helper materializes File handle variadic args packs
- ir lowerer inline param helper aliases spread args packs

### test_ir_pipeline_validation_ir_lowerer_on_error_helpers_wire_definition_handlers.cpp

- ir lowerer on_error helpers wire definition handlers
- ir lowerer on_error helpers wire definition handlers from call adapters
- ir lowerer on_error helpers reject missing resolution adapters
- ir lowerer on_error helpers prefer semantic-product metadata
- ir lowerer on_error helpers skip semantic-product sum definitions
- ir lowerer on_error helpers reject missing semantic bound arg ids
- ir lowerer on_error helpers require definition semantic ids for semantic-product metadata
- ir lowerer on_error helpers reject definition-path fallback facts
- ir lowerer on_error helpers use semantic-id facts without definition-path fallback
- ir lowerer on_error helpers require published definition-id maps
- ir lowerer on_error helpers build bundled entry call and on_error setup
- ir lowerer on_error entry setup validates semantic bound arg counts
- ir lowerer on_error helpers build bundled entry count and call/on_error setup
- ir lowerer uninitialized type helpers build bundled entry and setup resolution
- ir lowerer uninitialized type helpers build bundled runtime entry and setup resolution
- ir lowerer uninitialized type helpers build bundled entry return, runtime entry, and setup resolution

### test_ir_pipeline_validation_ir_lowerer_pow_helper_rejects_mixed_signed_unsigned_operands.cpp

- ir lowerer pow helper rejects mixed signed unsigned operands
- ir lowerer pow helper ignores non pow/abs/sign builtins
- ir lowerer conversions helper emits float conversion opcode
- ir lowerer conversions helper lowers alloc intrinsic to heap alloc
- ir lowerer conversions helper lowers free intrinsic to heap free
- ir lowerer conversions helper lowers realloc intrinsic to heap realloc
- ir lowerer conversions helper sizes memory pointer arithmetic from semantic facts
- ir lowerer conversions helper lowers checked memory at intrinsic to bounded pointer arithmetic
- ir lowerer conversions helper lowers unchecked memory at intrinsic to raw pointer arithmetic
- ir lowerer conversions helper emits vector record from layout fields
- ir lowerer vector record materialization uses layout helpers

### test_ir_pipeline_validation_ir_lowerer_result_helpers_emit_result_why_with_composed_ops.cpp

- ir lowerer result helpers emit Result.why with composed ops
- ir lowerer result helpers emit resolved Result.why calls
- ir lowerer map insert helper writes grown pointers back through wrapper locals
- ir lowerer flow helpers restore scoped state
- ir lowerer flow helpers emit file scope cleanup sequences
- ir lowerer flow helpers emit struct copy sequences
- ir lowerer flow helpers disarm soa storage temporaries after copy
- ir lowerer flow helpers classify borrowed struct copy sources
- ir lowerer flow helpers emit destroy helper calls from ptr locals
- ir lowerer flow helpers emit move helper calls from ptr locals
- ir lowerer flow helpers emit vector removed-slot destruction sequences
- ir lowerer flow helpers emit compare-to-zero sequences
- ir lowerer comparison helpers treat builtin comparisons as bool conditions
- ir lowerer flow helpers emit float literal sequences
- ir lowerer flow helpers emit return for definition
- ir lowerer flow helpers resolve and emit gpu builtins
- ir lowerer flow helpers match and emit unary passthrough calls
- ir lowerer flow helpers resolve counted loop kinds

### test_ir_pipeline_validation_ir_lowerer_result_helpers_resolve_file_handle_result_payload_metadata.cpp

- ir lowerer result helpers resolve file handle Result payload metadata
- ir lowerer result helpers prefer file handles over stale struct paths
- ir lowerer result helpers preserve semantic-id file handle payload metadata
- ir lowerer result helpers resolve array and vector Result payload metadata
- ir lowerer result helpers resolve Buffer Result payload metadata
- ir lowerer result helpers resolve map Result payload metadata

### test_ir_pipeline_validation_ir_lowerer_result_helpers_resolve_final_if_result_and_then_metadata.cpp

- ir lowerer result helpers resolve final-if Result.and_then metadata
- ir lowerer result helpers require semantic query facts for unresolved Result.map metadata
- ir lowerer result helpers resolve packed error struct Result combinator metadata
- ir lowerer result helpers resolve parser-shaped packed error struct Result combinator metadata
- ir lowerer result helpers resolve function-returned array Result payload metadata
- ir lowerer result helpers resolve function-returned map Result payload metadata
- ir lowerer result helpers resolve function-returned Buffer Result payload metadata
- ir lowerer result helpers build locals-aware resolver adapters
- ir lowerer result helpers resolve indexed args-pack Result expressions
- ir lowerer result helpers use semantic local Result source facts
- ir lowerer result helpers use semantic indexed args-pack Result facts
- ir lowerer result helpers resolve indexed args-pack file handle method results
- ir lowerer result helpers resolve indexed borrowed args-pack file handle method results
- ir lowerer result helpers resolve indexed pointer args-pack file handle method results

### test_ir_pipeline_validation_ir_lowerer_result_helpers_resolve_indexed_dereferenced_args_pack_result_references.cpp

- ir lowerer result helpers resolve indexed dereferenced args-pack Result references
- ir lowerer result helpers resolve indexed dereferenced args-pack Result pointers
- ir lowerer result helpers resolve direct File constructor Results
- ir lowerer result helpers resolve Result.why call info
- ir lowerer result helpers resolve Result.error call info
- ir lowerer result helpers classify Result.why error kinds
- ir lowerer result helpers normalize Result.why type names
- ir lowerer result helpers emit Result.why local expressions
- ir lowerer result helpers emit Result.why error-local extraction
- ir lowerer result helpers emit Result.why value-local setup
- ir lowerer result helpers try emit Result.error method calls
- ir lowerer Result.error direct calls prefer semantic-product query facts

### test_ir_pipeline_validation_ir_lowerer_result_helpers_try_emit_result_ok_method_calls.cpp

- ir lowerer result helpers try emit Result.ok method calls
- ir lowerer result helpers emit Buffer Result.ok method calls
- ir lowerer result helpers pack single-slot struct Result.ok values
- ir lowerer result helpers preserve multi-slot struct Result.ok values
- ir lowerer result helpers resolve definition result metadata
- ir lowerer result helpers tolerate missing direct result metadata callbacks
- ir lowerer result helpers resolve from locals and return-info lookups
- ir lowerer result helpers require semantic query facts for generic call result metadata
- ir lowerer result helpers use semantic query facts for direct Result ok payload metadata
- ir lowerer result helpers infer syntax-owned direct Result ok payloads
- ir lowerer result helpers use semantic binding facts for direct Result ok name payloads
- ir lowerer result helpers reject resolved-path semantic query fallback in production mode
- ir lowerer inference dispatch requires semantic try facts
- ir lowerer inference dispatch rejects operand-path semantic try fallback in production mode
- ir lowerer result helpers resolve direct Result.ok struct payload metadata
- ir lowerer result helpers resolve direct Result.ok comparison payload metadata
- ir lowerer result helpers emit direct Result.ok comparison payloads
- ir lowerer result helpers emit direct Result.ok arithmetic payloads
- ir lowerer result helpers emit Result.ok payloads from semantic query facts
- ir lowerer result helpers resolve Result.map struct payload metadata
- ir lowerer result metadata resolves Result.ok payload query type ids

### test_ir_pipeline_validation_ir_lowerer_result_helpers_try_emit_result_why_method_calls.cpp

- ir lowerer result helpers try emit Result.why method calls
- ir lowerer Result.why source queries prefer interned error ids
- ir lowerer Result.error source queries prefer interned error ids
- ir lowerer result helpers dispatch Result.why and FileError.why
- ir lowerer result helpers compose Result.why expression ops
- ir lowerer result helpers compose Result.why call ops

### test_ir_pipeline_validation_ir_lowerer_runtime_error_helpers_emit_fileerror_why_call_paths.cpp

- ir lowerer runtime error helpers emit FileError.why call paths
- ir lowerer runtime error helpers build scoped emitters
- ir lowerer runtime error helpers build bundled emitters
- ir lowerer runtime error helpers build bundled string-literal and emitters setup
- ir lowerer index kind helpers normalize and validate supported kinds
- ir lowerer index kind helpers map index opcodes by kind
- ir lowerer setup math helper resolves namespaced builtins
- ir lowerer setup math helper resolves root builtin paths without imports
- ir lowerer setup math helper resolves root namespace builtin paths without imports
- ir lowerer builtin root helper rejects rooted paths without imports
- ir lowerer builtin root helper rejects root namespace paths without imports
- ir lowerer pointer helper resolves parser-shaped soa_vector builtins
- ir lowerer setup math helper validates builtin support names
- ir lowerer setup math helper requires import for bare names
- ir lowerer setup math helper resolves constants only for supported names
- ir lowerer setup math helper builds scoped name resolvers
- ir lowerer setup math helper builds bundled name resolvers
- ir lowerer setup math helper keeps current bundled binding adapter routes
- ir lowerer setup math helpers keep semantic product FileError bindings
- ir lowerer setup inference helper infers pointer target kinds

### test_ir_pipeline_validation_ir_lowerer_setup_inference_helper_handles_invalid_comparison_operator_calls.cpp

- ir lowerer setup inference helpers tolerate missing callbacks
- ir lowerer setup inference helper handles invalid comparison/operator calls
- ir lowerer setup inference helper infers GPU/buffer call return kinds
- ir lowerer setup inference helper handles invalid GPU/buffer calls
- ir lowerer setup inference helper infers count-capacity call return kinds
- ir lowerer setup inference helper handles non count-capacity calls
- ir lowerer statement binding helper infers vector kind from initializer call
- ir lowerer statement binding helper recovers bool kind from builtin comparison initializer
- ir lowerer statement binding helper prefers semantic temporary facts
- ir lowerer statement binding helper infers pointer kind from alloc initializer call
- ir lowerer statement binding helper infers pointer kind from realloc initializer call
- ir lowerer statement binding helper infers pointer kind from checked memory at initializer call
- ir lowerer statement binding helper infers pointer kind from unchecked memory at initializer call
- ir lowerer statement binding helper keeps canonical map constructor value metadata
- ir lowerer statement binding helper keeps scoped Buffer ctor as Buffer metadata
- ir lowerer statement binding helper inherits map metadata from named source binding
- ir lowerer statement binding helper infers call parameter local info
- ir lowerer statement binding helper preserves specialized vector struct paths for mutable vector parameters
- ir lowerer statement binding helper recovers bool default parameter kind from builtin comparison
- ir lowerer statement binding helper sets string parameter defaults
- ir lowerer statement binding helper classifies variadic Result parameters
- ir lowerer statement binding helper uses semantic query facts for default Result params

### test_ir_pipeline_validation_ir_lowerer_setup_inference_helper_handles_non_math_and_invalid_math_calls.cpp

- ir lowerer setup inference helper handles non-math and invalid math calls
- ir lowerer setup inference helper infers non-math scalar call return kinds
- ir lowerer setup inference helper handles invalid non-math scalar calls
- ir lowerer setup inference helper infers control-flow call return kinds
- ir lowerer setup inference helper handles invalid control-flow calls
- ir lowerer setup inference helper infers pointer builtin call return kinds
- ir lowerer setup inference helper handles invalid pointer builtin calls
- ir lowerer setup inference helper infers comparison/operator return kinds

### test_ir_pipeline_validation_ir_lowerer_setup_inference_helper_rejects_invalid_pointer_targets.cpp

- ir lowerer setup inference helper rejects invalid pointer targets
- ir lowerer setup inference helper infers buffer element kinds
- ir lowerer setup inference helper infers and validates array element kinds
- ir lowerer setup inference helper resolves call expression return kinds
- ir lowerer setup inference helper handles unresolved call return kinds
- ir lowerer setup inference helper ignores unqualified array and map access kinds
- ir lowerer setup inference helper rejects reordered bare key/value access kinds
- ir lowerer setup inference helper rejects bare key/value reference access kinds

### test_ir_pipeline_validation_ir_lowerer_setup_inference_helper_resolves_reordered_named_access_kinds.cpp

- ir lowerer setup inference helper ignores reordered bare access kinds
- ir lowerer setup inference helper keeps leading collection receiver for positional access
- ir lowerer setup inference helper keeps leading soa receiver for positional access
- ir lowerer setup inference helper keeps labeled named receiver for access
- ir lowerer setup inference helper handles invalid and templated access kinds
- ir lowerer setup inference helper prefers graph facts for string access receivers
- ir lowerer setup inference helper ignores wrapper-returned canonical map access string kinds
- ir lowerer setup inference helper defers canonical vector access string kinds
- ir lowerer setup inference helper defers bare vector method access string kinds
- ir lowerer setup inference helper defers std-namespaced vector method access string kinds
- ir lowerer setup inference helper resolves slash-method vector access string kinds
- ir lowerer setup inference helper defers wrapper-returned vector access kinds
- ir lowerer setup inference helper infers body value kinds with locals scaffolding
- ir lowerer setup inference helper recovers bool body comparison tails
- ir lowerer setup inference helper recovers bool body-local comparison bindings
- ir lowerer setup inference helper prefers semantic binding facts for body locals scaffolding
- ir lowerer setup inference helper validates body locals scaffolding diagnostics
- ir lowerer setup inference helper infers math builtin return kinds

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_allows_count_capacity_receiver_probing.cpp

- ir lowerer setup type helper allows count/capacity receiver probing
- ir lowerer setup type helper rejects access receiver fallback probing
- ir lowerer setup type helper resolves method call definitions from expressions
- ir lowerer setup type helper prefers canonical bare vector count and capacity methods
- ir lowerer setup type helper prefers canonical bare vector access methods
- ir lowerer setup type helper resolves parser-shaped canonical vector methods
- ir lowerer setup type helper prefers canonical bare vector mutator methods
- ir lowerer setup type helper rejects explicit rooted vector slash methods while honoring /array/count
- ir lowerer setup type helper rejects slash-path map methods from expressions
- ir lowerer setup type helper rejects canonical fallback for explicit map contains and tryAt methods
- ir lowerer setup type helper rejects bare map contains and tryAt methods
- ir lowerer setup type helper rejects explicit map contains and tryAt slash methods even when definitions exist
- ir lowerer setup type helper resolves declared receiver aliases through slashless map imports
- ir lowerer setup type helper resolves canonical map methods from slashless receiver call paths
- ir lowerer setup type helper resolves canonical map methods from generated receiver overload paths
- ir lowerer setup type helper prefers direct helper-return soa_vector mutator wrappers
- ir lowerer setup type helper prefers nested helper-return soa_vector mutator wrappers
- ir lowerer setup type helper resolves struct receiver method definitions from expressions

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_keeps_builtin_array_count_fallback_and_rejects_bare_vecto.cpp

- ir lowerer setup type helper keeps builtin array count fallback and rejects bare vector method fallback
- ir lowerer setup type helper reports unknown vector mutators when no override definition exists
- ir lowerer setup type helper keeps semantic explicit vector count helper over scalar builtin target
- ir lowerer setup type helper requires semantic vector method ids before gate facts
- ir lowerer setup type helper reports method call definition diagnostics from expressions
- ir lowerer setup type helper resolves return info kinds by path
- ir lowerer setup type helper resolves method call return kinds
- ir lowerer setup type helper resolves direct definition call return kinds
- ir lowerer setup type helper rejects direct definition call return kinds via removed vector aliases
- ir lowerer setup type helper rejects slashless vector alias return kinds without alias definitions

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_keeps_reject_diagnostics_for_canonical_map_helper_receive.cpp

- ir lowerer setup type helper keeps reject diagnostics for canonical map helper receiver calls
- ir lowerer setup type helper rejects bare map access primitive receiver fallback
- ir lowerer setup type helper rejects bare map tryAt receiver fallback
- ir lowerer setup type helper prefers semantic map receiver probe facts
- ir lowerer setup type helper keeps namespaced canonical contains receiver precedence
- ir lowerer setup type helper rejects alias receiver fallback when expr path is unavailable
- ir lowerer setup type helper keeps reject diagnostics when expr path is unavailable
- ir lowerer setup type helper keeps auto-wrapper primitive diagnostics for vector alias receiver calls
- ir lowerer setup type helper accepts direct alias primitive fallback from inferred receiver kinds
- ir lowerer setup type helper rejects slash-method alias primitive fallback from inferred receiver kinds
- ir lowerer setup type helper keeps explicit vector access receiver same-path precedence
- ir lowerer setup type helper rejects slash-method vector alias primitive receiver fallback
- ir lowerer setup type helper rejects wrapper string access primitive receiver fallback

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_rejects_canonical_map_access_fallback_to_compatibility_de.cpp

- ir lowerer setup type helper rejects canonical map access fallback to compatibility defs
- ir lowerer setup type helper rejects explicit map access aliases through canonical defs
- ir lowerer setup type helper defers reordered positional bare access calls
- ir lowerer setup type helper defers reordered bare access graph facts
- ir lowerer setup type helper defers positional bare access calls
- ir lowerer setup type helper defers reordered named bare access calls
- ir lowerer setup type helper defers labeled named bare access calls
- ir lowerer setup type helper resolves canonical soa get/ref call method return kinds
- ir lowerer setup type helper resolves soa field call method return kinds

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_rejects_wrapper_string_slash_method_access_primitive_fall.cpp

- ir lowerer setup type helper rejects wrapper string slash-method access primitive fallback
- ir lowerer setup type helper prefers graph facts for indexed string receivers
- ir lowerer setup type helper keeps parser-shaped canonical vector receiver routed diagnostics
- ir lowerer setup type helper keeps reject diagnostics for explicit slash-method map access receivers
- ir lowerer setup type helper rejects array compatibility slash-method vector access primitive fallback
- ir lowerer setup type helper rejects array compatibility slash-method vector count primitive fallback
- ir lowerer setup type helper rejects array compatibility slash-method vector capacity primitive fallback
- ir lowerer setup type helper preserves wrapper vector slash-method count diagnostics
- ir lowerer setup type helper preserves wrapper vector slash-method capacity diagnostics
- ir lowerer setup type helper keeps reject diagnostics for wrapper-returned explicit slash-method map access
- ir lowerer setup type helper keeps reject diagnostics for explicit map tryAt receivers
- ir lowerer setup type helper resolves explicit map count and contains receivers by inferred kind
- ir lowerer setup type helper keeps explicit map count and contains receiver same-path precedence
- ir lowerer setup type helper rejects bare map method receiver canonical fallback
- ir lowerer setup type helper rejects bare map method receiver alias fallback
- ir lowerer setup type helper keeps explicit map tryAt receiver alias precedence
- ir lowerer setup type helper resolves dereferenced indexed variadic map receivers
- ir lowerer setup inference helper ignores wrapper-returned slash-method map access kinds
- ir lowerer setup inference helper ignores wrapper-returned canonical map access int32 kinds

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_builtin_like_count_call_methods.cpp

- ir lowerer setup type helper resolves builtin-like count call methods
- ir lowerer setup type helper skips non-eligible count call method resolution
- ir lowerer setup type helper resolves capacity call method return kinds
- ir lowerer setup type helper skips non-eligible capacity call method resolution
- ir lowerer return inference helper analyzes entry return transforms
- ir lowerer return inference helper handles void and diagnostics
- ir lowerer return inference helper reads semantic product return facts
- ir lowerer return inference helper analyzes declared return transforms
- ir lowerer return inference helper resolves declared array and struct returns
- ir lowerer return inference helper unwraps referenced collection returns
- ir lowerer setup type infers referenced declared collection receivers
- ir lowerer inline call context helper prepares scoped setup
- ir lowerer inline call context helper reports setup diagnostics
- ir lowerer inline struct arg helper emits struct constructor argument flow

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_direct_definition_call_return_kinds_via_removed.cpp

- ir lowerer setup type helper rejects removed map alias return-kind fallback
- ir lowerer setup type helper rejects canonical map count fallback while keeping direct access defs
- ir lowerer setup type helper rejects canonical map contains fallback while keeping direct access defs
- ir lowerer setup type helper rejects canonical map tryAt fallback while keeping direct access defs
- ir lowerer setup type helper prefers canonical direct map count-like return info
- ir lowerer setup type helper rejects canonical map constructor fallback to compatibility defs
- ir lowerer setup type helper rejects canonical return-info forwarding when compatibility defs lack return info
- ir lowerer setup type helper skips unmatched direct definition call return kinds
- ir lowerer setup type helper resolves count call method return kinds
- ir lowerer setup type helper defers bare access call method return kinds

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_indexed_args_pack_pointer_map_receivers.cpp

- ir lowerer setup type helper resolves indexed args-pack pointer map receivers
- ir lowerer setup type helper resolves indexed args-pack pointer vector receivers
- ir lowerer setup type helper resolves dereferenced indexed args-pack borrowed array receivers
- ir lowerer setup type helper resolves dereferenced indexed args-pack pointer array receivers
- ir lowerer setup type helper resolves dereferenced indexed args-pack borrowed vector receivers
- ir lowerer setup type helper resolves dereferenced indexed args-pack pointer vector receivers
- ir lowerer setup type helper resolves indexed args-pack pointer soa_vector receivers
- ir lowerer setup type helper resolves indexed args-pack file receivers
- ir lowerer setup type helper resolves dereferenced indexed args-pack borrowed file receivers
- ir lowerer setup type helper resolves dereferenced indexed args-pack pointer file receivers
- ir lowerer setup type helper keeps borrowed local soa_vector receivers distinct from vector
- ir lowerer setup type helper resolves indexed args-pack map receivers
- ir lowerer setup type helper resolves indexed args-pack Buffer receivers
- ir lowerer setup type helper resolves dereferenced indexed args-pack borrowed Buffer receivers
- ir lowerer setup type helper resolves dereferenced indexed args-pack pointer Buffer receivers
- ir lowerer setup type helper dispatch reports missing name receiver diagnostics
- ir lowerer setup type helper selects method call receiver expression
- ir lowerer setup type helper skips non-method call receiver selection
- ir lowerer setup type helper reports method receiver selection diagnostics

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_indexed_args_pack_struct_receiver_methods_from_e.cpp

- ir lowerer setup type helper resolves indexed args-pack struct receiver methods from expressions
- ir lowerer setup type helper resolves soa_vector receiver method definitions from expressions
- ir lowerer setup type helper requires semantic-product method targets
- ir lowerer setup type helper rejects synthetic direct-call probes for method resolution
- ir lowerer setup type helper keeps semantic direct user collection fallbacks
- ir lowerer setup type helper rejects semantic-product method targets without lowered definitions
- ir lowerer setup type helper does not reconstruct method targets from receivers when semantic-product defs are missing
- ir lowerer setup type helper rejects alias receiver call return fallback to canonical stdlib defs
- ir lowerer setup type helper keeps reject diagnostics for alias receiver call returns
- ir lowerer setup type helper rejects alias receiver defs without canonical return fallback
- ir lowerer setup type helper keeps reject diagnostics when alias receiver defs lack returns
- ir lowerer setup type helper rejects map alias receiver call return fallback to canonical stdlib defs
- ir lowerer setup type helper prefers canonical map method return structs over alias defs
- ir lowerer setup type helper keeps canonical map non-struct fallback over alias defs
- ir lowerer setup type helper keeps reject diagnostics for std-namespaced vector method access
- ir lowerer setup type helper keeps reject diagnostics for map alias receiver unsafe call returns

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_method_definitions_from_receiver_targets.cpp

- ir lowerer setup type helper resolves method definitions from receiver targets
- ir lowerer setup type helper normalizes helper-return SoaVector collections for shadows
- ir lowerer setup type helper rejects rooted vector slash methods while honoring /array/count
- ir lowerer setup type helper rejects slash-path map helpers on map receivers
- ir lowerer setup type helper reports method target lookup diagnostics
- ir lowerer setup type helper rejects canonical vector receiver fallback to removed aliases
- ir lowerer setup type helper rejects canonical soa access fallback to rooted aliases
- ir lowerer setup type helper rejects canonical soa mutator fallback to rooted aliases
- ir lowerer setup type helper rejects canonical soa to_aos fallback to rooted alias
- ir lowerer setup type helper resolves name receiver targets
- ir lowerer setup type helper reports name receiver diagnostics
- ir lowerer setup type helper dispatches receiver target resolution
- ir lowerer setup type helper gates bare map receiver probes with semantic target facts
- ir lowerer setup type helper resolves indexed args-pack vector receivers
- ir lowerer setup type helper resolves indexed args-pack soa_vector receivers
- ir lowerer setup type helper resolves indexed args-pack array receivers
- ir lowerer setup type helper resolves indexed args-pack borrowed array receivers
- ir lowerer setup type helper resolves indexed args-pack pointer array receivers
- ir lowerer setup type helper resolves indexed args-pack borrowed map receivers

### test_ir_pipeline_validation_ir_lowerer_setup_type_helper_skips_explicit_vector_helper_call_routing.cpp

- ir lowerer setup type helper skips explicit vector helper call routing
- ir lowerer setup type helper rejects direct vector access helper routing even with real defs
- ir lowerer setup type helper rejects explicit map helper return kinds same-path
- ir lowerer setup type helper rejects explicit slash-method map access return kinds
- ir lowerer setup type helper rejects explicit slash-method vector access return kinds
- ir lowerer setup type helper rejects explicit slash-method vector access return kinds for constructor receivers
- ir lowerer setup type helper rejects bare vector access method return kinds

### test_ir_pipeline_validation_ir_lowerer_statement_binding_helper_classifies_variadic_fileerror_parameters.cpp

- ir lowerer statement binding helper classifies variadic FileError parameters
- ir lowerer statement binding helper classifies variadic File handle parameters
- ir lowerer statement binding helper classifies namespaced File handle parameters
- ir lowerer statement binding helper clears struct paths for File handles
- ir lowerer statement binding helper classifies variadic borrowed File handle parameters
- ir lowerer statement binding helper classifies variadic pointer File handle parameters
- ir lowerer statement binding helper classifies variadic borrowed FileError parameters
- ir lowerer statement binding helper classifies variadic pointer FileError parameters
- ir lowerer statement binding helper classifies variadic borrowed Result parameters
- ir lowerer statement binding helper classifies variadic pointer Result parameters
- ir lowerer statement binding helper classifies variadic vector parameters
- ir lowerer statement binding helper classifies variadic array parameters
- ir lowerer statement binding helper classifies variadic borrowed array parameters
- ir lowerer statement binding helper classifies variadic pointer array parameters
- ir lowerer statement binding helper classifies variadic Buffer parameters
- ir lowerer statement binding helper classifies variadic borrowed Buffer parameters
- ir lowerer statement binding helper classifies variadic pointer Buffer parameters
- ir lowerer statement binding helper classifies variadic scalar reference parameters

### test_ir_pipeline_validation_ir_lowerer_statement_binding_helper_classifies_variadic_struct_reference_parameters.cpp

- ir lowerer statement binding helper tolerates missing parameter callbacks
- ir lowerer statement binding helper classifies variadic struct reference parameters
- ir lowerer statement binding helper classifies variadic scalar pointer parameters
- ir lowerer statement binding helper classifies variadic struct pointer parameters
- ir lowerer statement binding helper classifies variadic borrowed map parameters
- ir lowerer statement binding helper classifies variadic pointer map parameters
- ir lowerer statement binding helper classifies variadic pointer vector parameters
- ir lowerer statement binding helper classifies variadic pointer soa_vector parameters
- ir lowerer statement binding helper classifies variadic borrowed imported SoaVector parameters
- ir lowerer statement binding helper classifies variadic pointer imported SoaVector parameters
- ir lowerer statement binding helper classifies variadic soa_vector parameters
- ir lowerer statement binding helper keeps specialized experimental soa_vector references as plain references without semantic surface metadata
- ir lowerer statement binding helper classifies parsed variadic borrowed
- ir lowerer statement binding helper uses semantic-product args-pack binding types
- ir lowerer statement binding helper rejects missing semantic args-pack binding type
- ir lowerer statement binding helper rejects incomplete semantic args-pack binding type
- ir lowerer statement binding helper preserves inferred borrowed soa_vector return metadata
- ir lowerer statement binding helper preserves inferred borrowed map return metadata
- ir lowerer statement binding helper classifies explicit soa_vector locals with specialized struct paths compatibility
- ir lowerer statement binding helper preserves indexed borrowed soa_vector
- ir lowerer statement binding helper preserves indexed pointer soa_vector
- ir lowerer statement binding helper classifies parsed borrowed imported
- ir lowerer statement binding helper canonicalizes imported bare struct
- ir lowerer statement binding helper classifies specialized vector
- ir lowerer statement binding helper preserves explicit specialized
- ir lowerer statement binding helper leaves explicit bare struct locals unresolved
- ir lowerer statement binding helper defers parsed explicit bare struct
- ir lowerer statement binding helper classifies explicit file locals
- ir lowerer statement binding helper keeps semantic scalar initializer
- ir lowerer statement binding helper keeps semantic pointer initializer
- ir lowerer statement binding helper keeps semantic reference initializer
- ir lowerer statement binding helper keeps semantic collection
- ir lowerer statement binding helper classifies variadic map parameters
- ir lowerer statement binding helper rejects string reference parameters
- ir lowerer statement binding helper rejects variadic string reference parameters with arg-pack diagnostic
- ir lowerer statement binding helper selects uninitialized zero opcodes
- ir lowerer statement binding helper rejects unknown uninitialized value kind
- ir lowerer statement binding helper emits literal string bindings
- ir lowerer statement binding helper emits checked argv string bindings

### test_ir_pipeline_validation_ir_lowerer_statement_binding_helper_emits_runtime_string_call_bindings.cpp

- ir lowerer statement binding helper emits runtime string call bindings
- ir lowerer statement binding helper emits runtime string access bindings
- ir lowerer statement binding helper rejects non-string source bindings
- ir lowerer statement binding helper emits uninitialized local init and drop
- ir lowerer statement binding helper routes local drop through optional ptr callback
- ir lowerer statement binding helper emits struct-storage init via ptr copy
- ir lowerer statement binding helper skips struct copy for file-handle storage locals
- ir lowerer statement binding helper emits indirect uninitialized init
- ir lowerer statement binding helper emits indirect struct init via ptr copy
- ir lowerer statement binding helper skips indirect struct copy for file handles
- ir lowerer statement binding helper validates uninitialized init drop diagnostics
- ir lowerer statement binding helper emits uninitialized take statements
- ir lowerer statement binding helper skips non-emittable take statements
- ir lowerer statement binding helper surfaces take resolution errors
- ir lowerer statement binding helper emits print statement builtins

### test_ir_pipeline_validation_ir_lowerer_statement_binding_helper_validates_print_statement_builtin_diagnostics.cpp

- ir lowerer statement binding helper validates print statement builtin diagnostics
- ir lowerer statement binding helper emits pathspace statement builtins
- ir lowerer statement binding helper emits inline return statements
- ir lowerer statement binding helper emits non-inline void return statements
- ir lowerer statement binding helper validates return diagnostics
- ir lowerer statement binding helper emits if statements
- ir lowerer statement binding helper treats builtin comparisons as bool conditions
- ir lowerer statement binding helper lowers match via statement callback
- ir lowerer statement binding helper validates if and match diagnostics
- ir lowerer statement call helper emits buffer_store
- ir lowerer statement call helper emits buffer_store for variadic Buffer receivers

### test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_buffer_store_diagnostics.cpp

- ir lowerer statement call helper validates buffer_store diagnostics
- ir lowerer buffer_store target kind uses semantic facts before stale locals
- ir lowerer statement call helper emits dispatch statements
- ir lowerer dispatch dimensions use semantic facts before expression inference
- ir lowerer statement call helper validates dispatch diagnostics
- ir lowerer map insert rewrite uses semantic receiver facts before stale locals
- ir lowerer vector mutator rewrite uses semantic receiver facts before stale locals
- ir lowerer experimental vector setters defer to method definitions
- ir lowerer SoA helper dispatch uses semantic receiver facts before stale locals
- ir lowerer statement call helper emits direct calls
- ir lowerer statement call emission source delegation stays stable

### test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_direct_call_diagnostics.cpp

- ir lowerer statement call helper validates direct-call diagnostics
- ir lowerer statement call helper emits semantic direct void helpers as statements
- ir lowerer statement call helper does not pop direct struct statements
- ir lowerer statement call helper updates referenced soa metadata receivers
- ir lowerer statement call helper emits assign and expression pops
- ir lowerer statement call helper validates assign and expression pop errors
- ir lowerer statement call helper builds callable definition contexts
- ir lowerer statement call helper builds callable context for struct helpers with implicit this
- ir lowerer statement call helper skips struct layouts for file-handle this locals
- ir lowerer statement call helper preserves struct slot counts for variadic callable locals
- ir lowerer statement call helper orchestrates callable lowering
- ir lowerer statement call helper prefers semantic callable inventory
- ir lowerer statement call helper orchestrates entry execution cleanup
- ir lowerer statement call helper validates entry execution diagnostics
- ir lowerer statement call helper finalizes function table wiring

### test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_function_table_diagnostics.cpp

- ir lowerer statement call helper validates function table diagnostics
- ir lowerer arithmetic helper emits integer add opcode
- ir lowerer arithmetic helper rewrites quaternion multiply from result-type fallback
- ir lowerer arithmetic helper rewrites mat3 vec3 multiply from result-type fallback
- ir lowerer arithmetic helper validates pointer operand side
- ir lowerer arithmetic helper uses semantic pointer operand facts before stale locals
- ir lowerer arithmetic helper treats reference handles as pointer operands
- ir lowerer arithmetic helper allows scalar reference offsets on the right
- ir lowerer arithmetic helper infers scalar reference offsets on the right
- ir lowerer arithmetic helper keeps mutable scalar locals numeric
- ir lowerer arithmetic helper infers mutable scalar locals as numeric
- ir lowerer arithmetic helper allows scoped buffer byte offsets on the right
- ir lowerer arithmetic helper ignores non arithmetic calls
- ir lowerer comparison helper emits integer less-than opcode
- ir lowerer comparison helper lowers logical and short-circuit
- ir lowerer comparison helper rejects string operands
- ir lowerer comparison helper ignores non comparison calls
- ir lowerer saturate helper emits is_nan predicate opcode
- ir lowerer saturate helper rejects bool saturate operand
- ir lowerer saturate helper ignores non saturate builtins
- ir lowerer clamp helper emits radians multiply opcode
- ir lowerer clamp helper rejects mixed signed unsigned clamp args
- ir lowerer clamp helper ignores non clamp builtins
- ir lowerer arc helper emits exp2 float multiply opcode
- ir lowerer arc helper rejects non-float arc operands
- ir lowerer arc helper ignores non arc builtins
- ir lowerer pow helper emits integer multiply opcode

### test_ir_pipeline_validation_ir_lowerer_statements_calls_step_validates_dependencies.cpp

- ir lowerer statements/calls step validates dependencies
- ir lowerer statements entry-statement step appends source ranges
- ir lowerer statements entry-statement step validates dependencies
- ir lowerer inline-call statement step appends source ranges
- ir lowerer inline-call statement step validates dependencies
- ir lowerer inline-call cleanup step patches return jumps
- ir lowerer inline-call cleanup step validates dependencies
- ir lowerer inline-call active-context step runs statements and cleanup
- ir lowerer inline-call active-context step restores context on failure
- ir lowerer inline-call active-context step validates dependencies
- ir lowerer inline-call gpu-locals step copies known locals
- ir lowerer inline-call gpu-locals step validates dependencies
- ir lowerer inline-call context-setup step initializes context and zero value
- ir lowerer inline-call context-setup step validates dependencies

### test_ir_pipeline_validation_ir_lowerer_string_call_helpers_handle_call_expression_paths.cpp

- ir lowerer string call helpers handle call-expression paths
- ir lowerer string call helpers report call-expression diagnostics
- ir opcode allowlist matches vm/native support matrix
- ir call semantics matrix accepts recursive call opcodes with tail metadata
- ir call semantics matrix rejects non-direct call targets for vm and native
- ir validator rejects invalid jump targets
- ir validator rejects invalid call targets
- ir validator rejects invalid print flags
- ir validator rejects invalid string indices
- ir validator rejects local indices beyond 32-bit
- ir validator rejects unknown opcodes
- ir validator rejects unknown metadata bits
- ir validator glsl target accepts basic integer control-flow subset
- ir validator glsl target rejects out-of-range i64 literals
- ir validator glsl target rejects out-of-range local slots
- ir validator wasm target accepts integer control-flow subset
- ir validator wasm target accepts float and conversion subset
- ir validator wasm target rejects unsupported opcodes
- ir validator wasm target accepts call opcodes
- ir validator wasm target accepts wasi output and argv opcodes
- ir validator wasm target accepts wasi file opcodes and file_write effect
- ir validator wasm target accepts wasi file read opcode and file_read effect

### test_ir_pipeline_validation_ir_lowerer_struct_field_binding_helpers_resolve_layout_bindings.cpp

- ir lowerer struct field binding helpers resolve layout bindings
- ir lowerer struct field binding helpers collect struct layout bindings
- ir lowerer struct field binding helpers collect layout bindings from program context
- ir lowerer struct return path helpers infer from definitions
- ir lowerer struct return path helpers infer from expressions
- ir lowerer struct return helpers reject vector alias canonical forwarding

### test_ir_pipeline_validation_ir_lowerer_struct_layout_helpers_compute_uncached_diagnostics.cpp

- ir lowerer struct layout helpers compute uncached diagnostics
- ir lowerer struct layout helpers compute from field info
- ir lowerer struct layout helpers compute from field info diagnostics
- ir lowerer struct layout helpers validate semantic product coverage
- ir lowerer struct layout coverage ignores generated collection helper subpaths
- ir lowerer struct layout helpers append program layouts
- ir lowerer struct layout helpers append program layout diagnostics
- ir lowerer struct layout helpers classify layout transforms
- ir lowerer call helpers build this params and collect struct fields
- ir lowerer call helpers build inline call parameter lists
- ir lowerer call helpers build inline call ordered arguments
- ir lowerer call helpers collect packed variadic inline call arguments
- ir lowerer call helpers leave inferred map receiver methods unresolved
- ir lowerer call helpers emit resolved inline definition call

### test_ir_pipeline_validation_ir_lowerer_struct_return_helpers_keep_map_tryat_alias_precedence.cpp

- ir lowerer struct return helpers keep map tryAt alias precedence
- ir lowerer struct return helpers reject explicit map tryAt method canonical forwarding
- ir lowerer struct return helpers keep explicit map tryAt method alias precedence
- ir lowerer struct layout helpers parse and extract alignment transforms
- ir lowerer struct layout helpers classify binding type layout
- ir lowerer struct layout helpers append struct fields
- ir lowerer struct layout helpers resolve binding type layout
- ir lowerer struct layout helpers compute with cache
- ir lowerer struct layout helpers compute uncached layout

### test_ir_pipeline_validation_ir_lowerer_struct_return_helpers_reject_canonical_fallback_from_alias_defs_without_ret.cpp

- ir lowerer struct return helpers reject canonical fallback from alias defs without returns
- ir lowerer struct return helpers keep empty result when alias candidates have no struct returns
- ir lowerer struct return helpers reject canonical vector access call forwarding
- ir lowerer struct return helpers keep vector method alias precedence
- ir lowerer struct return helpers reject canonical vector method forwarding
- ir lowerer struct return helpers keep bare map access canonical forwarding
- ir lowerer struct return helpers keep canonical slash-path map access forwarding
- ir lowerer struct return helpers keep canonical map access call forwarding
- ir lowerer struct return helpers reject map access compatibility call forwarding
- ir lowerer struct return helpers reject map tryAt compatibility call forwarding

### test_ir_pipeline_validation_ir_lowerer_struct_type_helpers_infer_struct_return_paths.cpp

- ir lowerer struct type helpers infer struct return paths
- ir lowerer struct type helpers infer call target struct paths
- ir lowerer struct type helpers infer name-expression struct paths
- ir lowerer struct type helpers infer field-access struct paths
- ir lowerer struct type helpers infer args-pack indexed field-access paths
- ir lowerer struct type helpers build layout field index and collect fields
- ir lowerer struct type helpers resolve struct array info from path
- ir lowerer struct type helpers resolve and apply struct array info from layout field index
- ir lowerer struct type helpers build struct array resolvers from layout field index
- ir lowerer struct type helpers build bundled struct array adapters
- ir lowerer struct type helpers apply struct array info
- ir lowerer struct type helpers resolve struct slot field by name
- ir lowerer struct type helpers resolve field slot from layout

### test_ir_pipeline_validation_ir_lowerer_struct_type_helpers_resolve_struct_slot_layouts_from_definition_fields.cpp

- ir lowerer struct type helpers resolve struct slot layouts from definition fields
- ir lowerer struct type helpers resolve struct slots from definition field index
- ir lowerer struct type helpers synthesize generated soa vector layouts
- ir lowerer struct type helpers resolve bare std ui field aliases
- ir lowerer struct type helpers build struct-slot resolvers from definition field index
- ir lowerer struct type helpers build bundled struct-slot resolution adapters
- ir lowerer struct type helpers build bundled struct-slot adapters with owned state
- ir lowerer struct type helpers build bundled struct layout adapters
- ir lowerer struct type helpers report definition slot layout diagnostics
- ir lowerer struct type helpers apply struct value info
- ir lowerer struct type helpers build struct value info applier
- ir lowerer struct type helpers build bundled struct-type resolution adapters
- ir lowerer struct type helpers build bundled setup-type and struct-type adapters

### test_ir_pipeline_validation_ir_lowerer_struct_type_helpers_skip_unsupported_struct_value_paths.cpp

- ir lowerer struct type helpers skip unsupported struct value paths
- ir lowerer struct type helpers skip known scalar-like value bindings
- ir lowerer uninitialized type helpers classify supported types
- ir lowerer uninitialized type helpers build type resolver
- ir lowerer uninitialized type helpers report diagnostics
- ir lowerer uninitialized type helpers resolve local storage metadata
- ir lowerer uninitialized type helpers resolve local storage candidates
- ir lowerer uninitialized type helpers resolve local storage access
- ir lowerer uninitialized type helpers resolve field template args by struct path
- ir lowerer uninitialized type helpers collect field bindings from index
- ir lowerer uninitialized type helpers build field binding index
- ir lowerer uninitialized type helpers build field binding index from struct layout field index
- ir lowerer uninitialized type helpers build bundled struct and uninitialized field indexes
- ir lowerer uninitialized type helpers build bundled struct and uninitialized resolution setup
- ir lowerer uninitialized type helpers build bundled setup-type, struct-type, and uninitialized setup
- ir lowerer uninitialized type helpers build bundled setup-math, setup-type, and uninitialized setup
- ir lowerer uninitialized type helpers check field index struct path
- ir lowerer uninitialized type helpers infer call target struct paths from field index

### test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_build_bundled_program_entry_return_runtime_and_s.cpp

- ir lowerer uninitialized type helpers build bundled program entry return runtime and setup resolution
- ir lowerer setup locals helper unpacks orchestration adapters
- ir lowerer on_error helpers reject unknown handler
- ir lowerer setup type helper maps primitive aliases
- ir lowerer setup type helper maps file and packed error types
- ir lowerer setup type helper returns unknown for unsupported names
- ir lowerer setup type helper maps value kinds to type names
- ir lowerer setup type helper returns empty name for unknown kind
- ir lowerer setup type helper resolves method receiver local targets
- ir lowerer setup type helper rejects pointer and non-array reference receivers
- ir lowerer setup type helper resolves method receiver call targets
- ir lowerer setup type helper falls back for method receiver call targets
- ir lowerer setup type helper resolves method receiver struct paths from call expressions
- ir lowerer setup type helper keeps direct map access unsafe receiver paths separated
- ir lowerer setup type helper rejects non-struct method receiver call paths

### test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_build_storage_resolver_from_definition_field_ind.cpp

- ir lowerer uninitialized type helpers build storage resolver from definition field index
- ir lowerer uninitialized type helpers build bundled uninitialized resolution adapters
- ir lowerer bundled uninitialized adapters specialize vector local struct paths from element metadata
- ir lowerer binding transform helpers classify qualifiers and mutability
- ir lowerer binding transform helpers detect explicit binding types
- ir lowerer binding transform helpers extract first binding type transform
- ir lowerer binding transform helpers extract uninitialized template args
- ir lowerer binding transform helpers detect entry args params
- ir lowerer binding type helpers classify binding kind and string/fileerror types
- ir lowerer binding type helpers resolve value kinds from transforms
- ir lowerer binding type helpers mark reference-to-array metadata
- ir lowerer binding type helpers build bundled setup adapters
- ir lowerer binding type helpers prefer semantic product temporary facts
- ir lowerer binding type adapters resolve interned binding type ids before copied text
- ir lowerer binding type helpers prefer semantic collection specialization facts
- ir lowerer binding type helpers stop using transform fallback for semantic expr ids
- ir lowerer binding type helpers treat semantic local-auto facts as non-explicit bindings
- ir lowerer count access helpers classify entry args and count calls

### test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_infer_call_target_struct_paths_with_visited_fiel.cpp

- ir lowerer uninitialized type helpers infer call target struct paths with visited field index callbacks
- ir lowerer uninitialized type helpers unwrap semantic pointer-like struct query types
- ir lowerer uninitialized type helpers infer definition return paths with visited map lookups
- ir lowerer uninitialized type helpers infer definition return paths from definition map
- ir lowerer uninitialized type helpers infer wrapped experimental soa vector return paths
- ir lowerer uninitialized type helpers infer definition return paths by call target field index
- ir lowerer uninitialized type helpers infer expression struct paths by definition call target field index
- ir lowerer struct return path helpers infer implicit if expression returns
- ir lowerer uninitialized type helpers build expression struct path inferer from definition call target field index
- ir lowerer uninitialized type helpers specialize explicit vector return paths from definition call targets
- ir lowerer uninitialized type helpers infer dereference expression struct paths
- ir lowerer uninitialized type helpers only infer namespaced internal SoA helper struct paths for location-style probes
- ir lowerer uninitialized type helpers infer helper return experimental soa_vector struct paths
- ir lowerer uninitialized type helpers find field template args
- ir lowerer uninitialized type helpers resolve field storage candidates
- ir lowerer uninitialized type helpers resolve field storage type info
- ir lowerer uninitialized type helpers infer concrete stdlib map constructor structs
- ir lowerer uninitialized type helpers infer forwarded stdlib map constructor structs
- ir lowerer uninitialized type helpers use semantic try value facts
- ir lowerer uninitialized type helpers use semantic source type facts

### test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_resolve_field_storage_access.cpp

- ir lowerer uninitialized type helpers resolve field storage access
- ir lowerer uninitialized type helpers resolve unified storage access
- ir lowerer uninitialized type helpers resolve indirect storage access
- ir lowerer uninitialized type helpers resolve indexed args-pack pointer storage access
- ir lowerer uninitialized type helpers resolve pointer helper storage access
- ir lowerer uninitialized type helpers resolve unified storage with field bindings
- ir lowerer uninitialized type helpers resolve unified storage from definitions
- ir lowerer uninitialized type helpers resolve unified storage from definition field index

### test_ir_pipeline_validation_ir_validator_accepts_lowered_canonical_module.cpp

- ir validator accepts lowered canonical module
- ir lowerer rejects non-eliminated reflection query paths
- ir lowerer reflection queries leave no runtime call state
- ir lowerer map contains avoids missing-key runtime helpers
- ir lowerer guarded map Result lookup avoids missing-key runtime helpers
- ir lowerer effects unit resolves entry and non-entry defaults
- ir lowerer effects unit rejects published software numeric preflight facts
- ir lowerer effects unit rejects published runtime reflection preflight facts
- ir lowerer native effects leave map values to ordinary lowering
- ir lowerer gpu effects unit rejects published software numeric preflight facts
- ir lowerer gpu effects unit rejects published runtime reflection preflight facts
- ir lowerer helper classifies soa_vector as collection builtin
- ir lowerer helper rejects array namespaced vector constructor alias builtin
- shared collection helpers reject removed rooted vector constructor alias
- shared simple-call helpers reject removed rooted vector constructor alias
- ir lowerer helper keeps canonical vector constructor builtin
- ir lowerer helper keeps parser-shaped canonical vector constructor builtin
- ir lowerer helper recognizes experimental vector element alias constructor
- ir lowerer helper keeps bare array builtin inside namespaced stdlib internals
- ir lowerer helper keeps bare pointer builtins inside namespaced stdlib internals
- ir lowerer helper keeps bare array access builtins inside namespaced stdlib internals
- simple-call helpers keep rooted and namespaced internal soa storage bare builtins
- emitter builtin assign keeps internal soa storage helper paths
- emitter control helpers keep internal soa storage helper paths
- shared return helpers keep scoped stdlib and custom paths builtin
- emitter helpers keep generated internal soa helper paths builtin
- ir lowerer helper accepts parser-shaped canonical map entry constructors as builtin map
- shared simple-call helpers reject removed array count alias
- semantics removed-alias helpers reject rooted vector spellings
- semantics namespaced vector helper detection rejects removed rooted aliases
- ir lowerer setup-type vector helper detection rejects removed rooted aliases
- ir lowerer setup-type removed vector method alias helper rejects rooted aliases
- ir lowerer access helper rejects removed rooted vector access aliases
- ir lowerer access helper classifies namespaced access helpers
- ir lowerer stdlib surface metadata rejects experimental map lowering helpers
- stdlib surface metadata resolves collection helper member tokens
- stdlib surface metadata classifies collection helper categories
- ir lowerer helper keeps parser-shaped intrinsic memory builtins
- ir lowerer helper keeps parser-shaped gpu builtins
- ir lowerer helper keeps parser-shaped rooted convert builtin
- ir lowerer helper keeps namespaced convert builtin tails
- emitter helper keeps parser-shaped rooted negate builtin
- emitter helpers keep parser-shaped std math builtins
- emitter helpers keep internal soa builtins under rooted and namespaced paths
- shared helper bodies keep scoped stdlib builtins normalized
- emitter collection inference keeps namespaced internal soa builtins out of string-value access inference
- stdlib surface metadata resolves collection alias paths
- emitter cpp keeps canonical vector count builtin fallback
- emitter cpp keeps explicit canonical vector count same-path during emission
- emitter cpp keeps array count builtin fallback
- semantics accepts and lowerer emits empty soa_vector literals
- public soa count helper lowers through wrapper routing
- bare soa_vector count helper lowers through wrapper return routing compatibility
- nested struct-body soa_vector constructor-bearing helper returns lower through direct and bound expressions compatibility
- bare soa_vector get helper lowers through wrapper return routing compatibility
- ir lowerer lowers non-empty soa_vector literals through substrate helper routing
- root get helper forms lower through canonical helper routing
- root get vector receiver rejects template arguments
- root ref helper forms stop in semantics on borrowed-view pending diagnostic
- root ref vector receiver rejects non-soa target
- semantics accepts to_soa before lowerer rejection
- semantics accepts to_soa method forms before lowerer rejection
- semantics accepts to_aos before lowerer rejection
- semantics rejects explicit soa_vector reserve on vector target through canonical helper path
- explicit soa_vector mutators lower through canonical helper routing
- root to_aos bare and direct helper forms reject during semantics
- imported root to_aos bare and direct helper forms still need compile-pipeline helper materialization
- root to_aos method helper forms reject during semantics
- imported root to_aos method helper forms still need compile-pipeline helper materialization
- imported builtin soa_vector bare helper forms lower through canonical helper routing
- imported builtin soa_vector method access forms stop in semantics on borrowed-view pending diagnostic
- imported builtin soa_vector method mutators lower through canonical helper routing
- canonical experimental wrapper to_aos slash-method lowers successfully
- borrowed helper-return experimental wrapper lowers through conversion helper
- borrowed helper-return experimental wrapper bare conversion alias lowers through generic wildcard import
- root to_aos canonical routing ignores vector-only user helper
- root to_aos vector receiver keeps canonical reject contract
- semantics rejects soa_vector field-view before lowerer
- semantics rejects soa_vector field-view call-form before lowerer
- semantics rejects soa_vector field-view direct-call index shape before lowerer
- semantics rejects soa_vector field-view call-form index shape before lowerer
- semantics rejects soa_vector get method named args before lowerer
- semantics rejects soa_vector ref method named args before lowerer
- ir lowerer effects unit validates program effect traversal
- ir lowerer effects unit rejects unsupported nested expression effects
- ir lowerer vm effects unit reports vm surface diagnostics
- ir lowerer gpu effects unit reports gpu surface diagnostics
- ir lowerer effects unit prefers semantic product callable summaries
- ir lowerer effects unit skips semantic callable summaries for sum types
- ir lowerer entry setup uses vm effects surface when requested
- ir lowerer entry setup uses native effects surface when requested
- ir lowerer entry setup uses gpu effects surface when requested
- ir lowerer effects unit keeps nested expression effect checks syntax owned
- ir lowerer effects unit resolves entry metadata masks
- ir lowerer effects unit resolves entry metadata masks from semantic product

### test_ir_pipeline_validation_ir_validator_wasm_target_accepts_heap_alloc_effects_and_capabilities.cpp

- ir validator wasm target accepts heap_alloc effects and capabilities
- ir validator wasm-browser target accepts integer control-flow subset
- ir validator wasm-browser target rejects wasi-only opcodes
- ir validator wasm-browser target rejects effects and capabilities

### test_ir_pipeline_validation_semantics_validate_source_delegation_stays_stable.cpp

- semantics validate publishes allowlisted pilot routing artifacts
- semantics validate publishes return and import artifacts through public semantic product views
- semantics validate publishes struct field metadata and parameter binding facts
- semantics validate publishes module artifacts in import order

### test_ir_pipeline_validation_semantics_validator_public_contracts.cpp

- semantic product publishes validator inference routing facts
- semantic product publishes stdlib helper routing ids

## ir_pipeline/wasm (17 tests, 2 files)

### test_ir_pipeline_wasm.cpp

- wasm emitter writes deterministic empty-module section snapshots
- wasm emitter passes structural section validation for empty module
- wasm emitter rejects invalid empty-module entry index
- wasm emitter lowers simple integer function to deterministic bytes
- wasm emitter rejects unsupported backward jump control flow
- wasm emitter lowers canonical backward-branch loops
- wasm emitter rejects malformed jump targets deterministically
- wasm emitter rejects malformed backward jump targets deterministically
- wasm emitter lowers float ops and conversions to deterministic opcodes
- wasm emitter lowers i64 and u64 conversion opcodes deterministically
- wasm emitter lowers direct call and callvoid opcodes
- wasm emitter lowers recursive call opcode in canonical IR

### test_ir_pipeline_wasm_wasi.cpp

- wasm emitter adds wasi imports memory and argv output lowering
- wasm emitter maps wasi file open write flush close and error paths
- wasm emitter maps wasi file read byte and eof paths
- wasm emitter formats decimal file writes for i32 i64 and u64
- wasm emitter rejects unsupported opcodes for this slice

## misc (146 tests, 12 files)

### test_diagnostics_codes.cpp

- import diagnostic code stays stable
- import diagnostic record emits stable code
- parser diagnostic stability contract stays source locked
- unclassified diagnostic fields stay implementation tier
- semantic unknown-call diagnostics stay source locked
- lowering variadic reference-pack diagnostics stay source locked

### test_execution_parsing.cpp

- parses execution without body
- rejects execution without parentheses
- parser rejects execution body arguments
- parses execution transforms
- parses execution named arguments
- parses execution bracket-labeled arguments
- parses execution with collection call arguments
- parses execution with named args and collections

### test_execution_whitespace.cpp

- parses execution args across lines
- parses named execution args across lines

### test_import_errors.cpp

- unterminated import fails
- unterminated import with whitespace fails
- missing import path fails
- import path suffix fails
- import path suffix before version fails
- import path suffix with single quotes fails
- import version with trailing junk fails
- import version missing equals fails
- import version requires quoted string
- unterminated import string literal fails
- import version with trailing junk and single quotes fails
- duplicate import version attribute fails
- duplicate import version attribute with single quotes fails
- unquoted non-slash import path fails
- unquoted import path with invalid segment fails
- unquoted import path with trailing slash fails
- unquoted import path with digit segment fails
- unquoted import path with dot fails
- import version mismatch across paths fails
- unquoted import path with reserved keyword fails
- unquoted import path with import keyword fails
- invalid import version fails
- import version with too many parts fails
- empty import version fails
- invalid import version with single quotes fails
- missing import version directory fails
- missing import minor version fails
- missing import minor version with single quotes fails
- missing import major version fails
- missing import major version with single quotes fails
- absolute versioned import without roots fails
- import version mismatch fails
- private import path fails
- private import path fails from import root
- import directory without prime files fails
- logical import version requires import roots
- versioned import fails when file missing

### test_import_multiple.cpp

- expands multiple import paths
- expands whitespace-separated import paths
- ignores nested duplicate imports

### test_import_resolver.cpp

- expands single import
- expanded source ledger records primary direct nested and directory imports
- expanded source ledger records generated import separators
- expanded source diagnostic mapper indexes many segment lookups
- expanded source diagnostic mapper preserves closed endpoint fallback
- compile pipeline exposes stdlib auto include source units
- compile pipeline uses module manifest for direct gfx import
- compile pipeline uses module manifest for gfx wildcard import
- compile pipeline rejects malformed stdlib module manifest metadata
- compile pipeline maps parser diagnostics through source units
- parser diagnostic stability contract exposes code notes and source-unit spans
- compile pipeline maps imported semantic diagnostics through source units
- semantic unknown-call stability contract exposes mapped notes
- rejects single legacy include alias
- expands import with whitespace
- expands import with tight comment
- expands import with bare slash path
- expands bare slash imports with semicolons
- expands bare slash imports with whitespace separators
- bare slash import does not use absolute filesystem path
- resolves versioned import with single quotes
- rejects versioned legacy include alias
- rejects version-first legacy include alias
- ignores duplicate imports
- ignores duplicate imports with equivalent relative paths
- missing import fails
- ignores import directives inside string literals
- ignores import directives inside comments
- ignores import keyword within identifiers
- ignores import keyword after path separator
- resolves import from import path
- resolves relative import from import path
- supports semicolon separated imports

### test_ir_pipeline.cpp

- ir preparation stops unresolved generic semantic facts before lowering

### test_lexer.cpp

- lexes namespace keyword and identifiers
- lexes import keyword
- lexes slash paths as identifiers
- lexes raw string literal suffixes
- lexes string literal with underscore suffix
- lexes single-quoted string literals
- lexes string literals with escapes
- lexes unterminated string literal as invalid token
- lexes numeric literals with suffixes and exponents
- lexes leading dot float literals
- lexes numeric literals with comma separators
- lexes numbers with trailing dot and empty hex
- skips comments and tracks lines
- skips block comments
- skips block comments with newline
- lexes punctuation tokens
- lexes unknown punctuation as end token
- lexes minus as invalid punctuation
- lexes unterminated block comment as invalid token

### test_printers_manual.cpp

- ast printer includes call transforms and bodies
- ir printer covers bindings assigns and exec bodies
- ast printer covers executions with templates

### test_scene_bgra8_renderer.cpp

- scene bgra8 renderer fills clipped flat rect from serialized scene
- scene bgra8 renderer applies rounded rect sdf edge coverage
- scene bgra8 renderer orders painter before local z
- scene bgra8 renderer uses local z for painter ties
- scene bgra8 renderer source-over blends overlapping primitives
- scene bgra8 renderer shades deterministic sdf button states
- scene bgra8 renderer composes sdf button with 2d primitives
- scene text shaper emits deterministic international glyph runs
- scene text shaper chooses first fallback and stable missing glyph
- scene text shaper rejects invalid utf8 deterministically
- scene text rasterizer packs shaped glyph coverage deterministically
- scene text rasterizer emits stable missing glyph coverage
- scene bgra8 renderer composes text overlay over scene primitives
- ui scene surface bridge renders prime-authored ui scene to bgra8

### test_stdlib_map_ownership.cpp

- canonical map surface owns standalone stdlib implementation
- experimental map production traces are classified as backing substrate

### test_template_parsing.cpp

- parses template list on definition
- parses heterogeneous type pack parameter on definition
- parses heterogeneous type pack parameter on struct
- parses heterogeneous type pack field expansion
- parses heterogeneous type pack expansion in type envelopes
- parses template list on call
- parses integer template argument metadata on call and method call
- compile-time argument metadata reserves symbol and unsupported kinds
- parses bare typeof symbol as compile-time intrinsic
- parses empty template argument lists
- parses nested template arguments on call
- rejects invalid non-type template arguments
- parses nested template argument on return transform
- rejects invalid heterogeneous type pack declarations
- parses type pack expansion in call template arguments
- parses canonical generic requirement predicate transforms
- parses public requirement syntax after text-filter normalization
- parses public ct_if statement and expression branch syntax

## parser/basic (315 tests, 11 files)

### test_parser_basic_block_control_flow.cpp

- parses if with block arguments
- parses single-branch if statement sugar
- parses if blocks with return statements

### test_parser_basic_control_flow.cpp

- parses loop form
- parses while form
- parses for form
- parses for form with separators
- parses transform-prefixed loop form
- parses single-quoted strings
- parses arguments without commas
- parses separators as whitespace
- parses transform arguments with slash paths
- parses parameters without commas
- parses semicolon parameters without return transform
- parses template arguments without commas
- parses type-pack index template arguments
- parses comments as whitespace
- parses if call labels with comments
- parses if call labels with trailing label comments
- parses match call labels with comments
- parses comment between signature and body
- parses comment between exec args and terminator
- parses local binding statements
- parses bare binding statements
- parses binding type transforms with multiple template args
- parses transform template lists with multiple args
- parses transform string arguments with suffix
- parses transform list with comma separators
- parses definition tail transforms and normalizes to prefix list
- parses nested definition tail transforms
- parses template lists without commas
- parses nested template lists without commas
- parses nested template lists with semicolons
- parses nested template lists with comments and semicolons
- parses parameter list without commas
- parses call arguments without commas
- single_type_to_return stays in transform list
- single_type_to_return preserves custom type transform
- parses method calls with template arguments
- parses literal statement
- parses struct definition without return
- parses pod definition without return

### test_parser_basic_definition_execution.cpp

- parses transform-prefixed execution
- parses binding-like transforms on calls
- parses execution transforms in bodies and arguments
- parses task spawn and wait surface
- parses semicolon-separated transforms and lists

### test_parser_basic_definition_transforms.cpp

- parses trait constraint transforms
- parses reflection transforms on struct definitions
- parses reflection generate list for all v1 helpers
- parses reflection generate Compare helper
- parses transform groups
- parses transform groups with semicolons
- parses transform groups with comments and semicolons
- parses semantic transform full form arguments
- parses semantic transform arguments without separators
- parses semantic transform full forms across bracket continuation
- parses semantic transform scalar and body full forms
- parses semantic transform method-call full form argument
- parses semantic transform method-call without explicit arguments
- parses semantic transform field-access full form argument
- parses semantic transform field-access full form on call receiver
- parses semantic transform indexing full form argument
- parses semantic transform index then field-access full form
- parses semantic transform index then method-call full form
- parses semantic transform index then template method-call full form
- parses semantic transform index then template method-call named argument
- parses semantic transform indexing full form on field access
- parses semantic transform indexing field access on call receiver
- parses semantic transform indexing full form on method call
- parses semantic transform indexing method call on call receiver

### test_parser_basic_definitions_core.cpp

- parses minimal main definition
- parses definition without return transform
- parses definition with omitted parameter envelopes
- parses definition without parameter list
- parses definition without parameter list and return transform
- parses struct definition without parameter list
- parses sum declaration variants in source order
- parses void return without transform
- parses slash path definition
- parses nested definitions inside bodies
- parses lambda expressions in bodies
- parses lambda expressions after earlier call arguments
- parses lambda return inside void definitions
- parses lambda captures with separators
- parses lambda captures with comment-only separators
- parses lambda capture ampersand
- parses lambda with semicolon-only parameter list
- parses lambda template arguments
- parses lambda captures with comments and operator chain
- parses lambda template arguments with comments and semicolons
- parses brace constructor values
- parses lower-case constructor in typed binding initializer
- parses brace constructor argument lists
- parses positional brace constructor argument lists
- parses primitive brace constructor as convert
- parses block call with parens
- parses call with trailing body arguments
- parses execution with arguments
- parses import paths with comments
- parses semicolon separators
- parses comma-separated statements and bindings
- parses trailing separators in lists
- parses labeled struct-literal local binding as constructor initializer
- parses leading separators in template lists
- parses comma-separated transform lists

### test_parser_basic_expr_forms.cpp

- parses statement call with block arguments
- parses boolean literals
- parses false boolean literal
- rejects malformed numeric tokens in expression parser
- expression parser rejects trailing tokens
- expression parser rejects malformed string token in canonical mode
- expression parser rejects indexing without closing bracket
- expression parser rejects if keyword without call syntax
- expression parser rejects match keyword without call syntax
- expression parser reports missing close paren after if fallback
- expression parser reports missing close paren after match fallback
- expression parser treats match call without body as regular call
- expression parser treats if call without body as regular call
- expression parser rejects match template arguments without call
- expression parser rejects if template arguments without call
- expression parser rejects member call without closing paren
- expression parser treats bare call body as brace constructor
- expression parser preserves brace constructor body expressions
- expression parser keeps single collection brace item positional
- expression parser rewrites primitive brace constructor to convert
- expression parser rewrites empty primitive brace constructor to convert
- expression parser normalizes return inside brace constructor body
- parses transform arguments
- parses boolean transform arguments
- parses transform arguments without commas
- parses transform list without commas
- parses transform string arguments
- parses named call arguments
- parses named arguments with comments
- parses argument label after expression with comments
- parses argument label with comment before value
- parses call arguments separated by semicolons
- parses call arguments with leading semicolons
- parses call arguments with semicolon-only body
- parses call arguments with repeated mixed separators
- parses named call arguments separated by semicolons
- parses named call arguments with leading semicolons

### test_parser_basic_literals.cpp

- parses hex integer literals
- parses unsigned hex integer literals
- parses uppercase hex integer literals
- parses integer literals with comma separators
- parses i64 and u64 integer literals
- rejects u32 integer literals
- rejects integer literals without suffix
- rejects integer literals with unsupported suffix
- rejects negative unsigned integer literals
- rejects i32 integer literals out of range
- rejects i32 integer literals below minimum
- rejects i64 integer literals below minimum
- rejects i64 integer literals above maximum
- rejects hex integer literals without digits
- rejects hex integer literals with invalid digit
- rejects decimal integer literals with invalid digit
- rejects u64 integer literals above maximum
- parses i32 integer literals
- parses float literals
- parses float literals with comma separators
- parses float literals without suffix
- parses float literals with trailing dot
- parses float literals with leading dot
- parses negative leading dot float literals
- parses float literals with suffix after dot
- parses float literals with f suffix
- parses float literals with exponent after dot
- parses float literals with exponent
- parses float literals with exponent and sign
- parses float literals with uppercase exponent
- rejects float literals with missing exponent digits
- rejects float literals without width suffix in canonical mode
- parses float literals with width suffix in canonical mode
- parses f64 float literals in canonical mode
- parses double literals
- parses signed i64 min literal
- parses signed i32 min literal
- parses string literal arguments
- parses string literal escape sequences
- parses single-quoted string literal arguments
- parses single-quoted string literal raw content
- parses single-quoted string literal unknown escape

### test_parser_basic_parser_helpers.cpp

- parser helper recognizes core builtin names
- parser helper validates math builtin qualification
- parser helper validates gpu builtin qualification
- parser helper validates memory intrinsic qualification
- parser helper rejects cross-qualified builtin names
- parser helper recognizes additional core builtin names
- parser helper finds transform names
- parser helper rejects non-builtin slash paths
- parser helper validates identifier edge cases
- parser helper validates transform-name special cases
- parser helper validates float literal edge cases
- parser helper validates additional float literal malformed forms
- parser helper describes invalid token edge cases
- parser helper describes invalid token high-byte and newline
- parser helper hex digit utility handles edge chars
- parser helper rejects reserved keyword slash segments
- parser helper rejects additional reserved keyword forms
- parser helper rejects all reserved keywords as identifiers
- parser helper classifies struct transform names
- parser helper classifies extended struct transform names
- parser helper classifies binding transform lists
- parser helper handles empty and argument-only binding transform lists
- parser helper classifies binding auxiliary transforms
- parser helper explicit type detection ignores effects capabilities
- parser helper detects explicit type transform without arguments
- parser helper validates qualified gpu helper names
- parser helper recognizes additional builtin name groups
- parser helper recognizes control and container builtin names
- parser helper recognizes operator builtin names
- parser helper validates late math builtin names
- parser helper validates advanced math builtin names
- parser helper validates interpolation and trig math builtins
- parser helper validates foundational math builtin names
- parser helper handles malformed and unslashed builtin paths
- parser helper detects explicit type after skipped transforms
- parser helper handles mixed transform list without explicit type

### test_parser_basic_semantic_transforms_a.cpp

- parses semantic transform index then template method-call on method-call receiver
- parses semantic transform index then template named method-call on method-call receiver
- parses semantic transform index then template method-call body on method-call receiver
- parses semantic transform index then template named method-call body on method-call receiver
- parses semantic transform index then template method-call no-arg body on method-call receiver
- parses semantic transform index after template method-call no-arg body on method-call receiver
- parses semantic transform index after template named method-call body on method-call receiver
- parses semantic transform method-call after nested indexed template body chain
- parses semantic transform indexing after nested indexed template body chain method-call
- parses semantic transform method-call after nested indexed template body chain indexed method-call
- parses semantic transform indexing after nested indexed template body chain indexed method-call
- parses semantic transform field-access after nested indexed template body chain indexed method-call
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access
- parses semantic transform method-call after nested indexed template body chain indexed method-call field-access
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail
- parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail field-access
- parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail field-access
- parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access
- parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call
- parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access
- parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call
- parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access
- parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index
- parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index
- parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call

### test_parser_basic_semantic_transforms_b.cpp

- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access
- parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call
- parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call field-access
- parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call field-access index
- parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access index
- parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access index method-call
- parses semantic transform field-access after nested indexed template body chain method-call
- parses semantic transform field-access after nested indexed template body chain
- parses semantic transform indexing after nested indexed template body chain field-access
- parses semantic transform indexing full form on call result
- parses semantic transform index then method-call on call result
- parses semantic transform index then template method-call on call result
- parses semantic transform index then template method-call without arguments
- parses semantic transform index after indexed template method-call result
- parses semantic transform index full form on template method chain
- parses semantic transform method-call with body arguments
- parses semantic transform method-call with named argument
- parses semantic transform method-call with multiple arguments
- parses semantic transform method-call with multiple named arguments
- parses semantic transform method-call with template arguments
- parses semantic transform method-call with template arguments and no call arguments
- parses semantic transform method-call with template arguments and body
- parses semantic transform method-call with call receiver and template args
- parses semantic transform method-call with multiple template arguments
- parses semantic transform method-call with template named arguments
- parses semantic transform full form template call
- parses semantic transform full form multi-template call
- parses semantic transform full form string literal
- parses semantic transform full form false literal
- parses semantic transform full form f64 literal
- parses semantic transform full form u64 literal
- parses semantic transform full form i64 literal
- parses semantic transform full form brace call with multiple values

### test_parser_basic_sugar.cpp

- normalizes string literals with double-quoted escapes
- parses ascii string literal arguments
- parses raw string literal arguments
- parses raw string literal escapes
- parses method call sugar
- parses index sugar
- parses variadic parameter and spread sugar into canonical arg-pack markers
- parses typed variadic parameters and canonical spread markers
- parses push method and call forms with equivalent argument wiring
- parses index, method-at, and call-at forms with equivalent at arguments
- parses brace constructor in call arguments
- parses brace constructor in return expression
- parses binding after call without separator
- parses if statement sugar
- parses match statement sugar
- parses if sugar in return argument
- parses match sugar in return argument
- parses match cases as call form
- parses block expression with parens
- parses block expression with mixed separators
- parses return inside block expression list
- parses if sugar with block statements in return argument
- parses if sugar inside block with mixed separators
- ignores line comments
- ignores block comments
- parses call with body arguments in expression

## parser/errors (153 tests, 6 files)

### test_parser_errors_more_identifiers.cpp

- multiple return statements parse
- lambda requires body
- lambda captures reject unexpected separator token
- lambda captures reject invalid first token
- lambda captures reject numeric token after capture entry
- lambda captures reject missing close bracket after comments
- lambda captures accept separator-only entry
- lambda captures accept semicolon-only entry
- lambda template requires parameter list
- lambda capture list still requires parameter list after comments
- lambda template still requires parameter list after comments
- lambda template requires closing angle
- lambda template with comments still requires closing angle
- lambda parameter default still requires lambda body
- block shorthand rejected in expression context
- reserved keyword cannot name definition
- class keyword cannot name definition
- control keyword cannot name definition
- match keyword cannot name definition
- reserved keyword cannot name parameter
- non-ascii identifier rejected
- non-ascii slash path rejected
- non-ascii type identifier rejected
- non-ascii whitespace rejected
- reserved keyword rejected in type identifier
- control keyword rejected in type identifier
- return without argument fails
- return with too many arguments fails
- return value not allowed for void definitions

### test_parser_errors_more_literals.cpp

- string literal requires suffix
- canonical string literal requires double quotes
- canonical string literal rejects raw suffix
- canonical string literal requires normalized escapes
- canonical mode requires explicit return transform on definitions
- string literal rejects unknown suffix
- string literal rejects unknown escape
- non-ASCII identifier rejects
- ascii string literal rejects non-ASCII characters
- raw ascii string literal rejects non-ASCII characters
- raw string literal rejects embedded quotes
- named arguments rejected for print builtin
- named arguments require bracket syntax
- named arguments reject comment-separated equals
- named arguments rejected for math builtin
- named arguments rejected for slash math builtin
- named arguments rejected for math builtin after import
- import /std/math rejected
- named arguments on bare vector clear require helper resolution
- missing return fails in parser
- block return does not satisfy definition return
- import inside namespace fails
- import inside definition body fails
- import path must be a slash path
- import path rejects invalid slash identifier
- import path rejects trailing slash
- non-ascii identifiers are rejected
- namespace name must be a simple identifier
- invalid slash path identifier fails
- slash path rejects reserved keyword segment
- slash path rejects control keyword segment
- slash path requires leading slash
- out of range literal fails
- minimum i32 literal succeeds
- minimum hex i32 literal succeeds
- below minimum i32 literal fails
- hex literal out of range fails
- below minimum hex literal fails
- integer literal requires suffix
- float literal requires suffix in canonical mode
- negative unsigned literal is rejected
- invalid float literal is rejected
- leading dot float literal validates exponent

### test_parser_errors_more_named_args.cpp

- named args for builtin fail in parser
- named args for slash builtin fail in parser
- named args for collection builtin fail in parser
- named args for pointer helpers fail in parser
- named args for count fail in parser
- named args for pathspace builtins fail in parser
- named args reject slash identifiers
- named args for array access fail in parser
- named args for unsafe array access fail in parser
- execution positional argument after named parses
- execution named arguments cannot target builtins
- execution positional after named with collections parses
- positional argument after named parses

### test_parser_errors_more_punctuation.cpp

- top-level definition without empty parameter list is accepted
- empty transform list is rejected
- non-ascii transform identifier rejected
- invalid punctuation character rejected
- unterminated string literal rejected
- unterminated block comment rejected
- slash path transform identifier accepted
- non-ascii transform argument rejected
- parameter identifiers reject template arguments
- parameter list requires bracketed parameter bindings
- binding initializer rejects named arguments with equals
- call arguments accept leading comma
- call arguments accept comma-only body
- parenthesized expression requires closing paren
- indexing sugar rejected in canonical mode
- variadic parameter must be final
- spread argument must be final

### test_parser_errors_more_transforms.cpp

- transform group cannot be empty
- text transform group requires parentheses
- text transform group with comments requires parentheses
- semantic transform group requires parentheses
- semantic transform group with comments requires parentheses
- transform arguments cannot be empty
- transform arguments reject punctuation token
- transform string arguments require suffix
- transform arguments reject nested envelopes
- text transform arguments reject full forms
- empty transform template list parses
- empty transform template list with comments parses
- binding without initializer parses as binding
- binding-like transforms allow paren call
- binding initializer rejects return without value
- parameter default rejects paren initializer
- parameter default rejects named arguments
- field access allows member without parameter list
- template arguments require a call
- match template arguments require a call
- member template arguments require a call
- return statement cannot have transforms
- if statement cannot have transforms
- match statement cannot have transforms
- spawn transform is rejected on bindings
- spawn transform is rejected on definitions
- definition without parameter list is allowed
- return missing parentheses fails
- missing '>' in template list fails
- unexpected end of file in definition body
- single-branch if in value position is rejected
- match statement requires else block
- definitions must have body
- definition tail transforms require parameter list first
- executions reject tail transform lists
- namespace identifier cannot be reserved keyword
- namespace identifier cannot be control keyword
- unexpected end of file inside namespace block
- reserved keyword cannot name argument
- control keyword cannot name argument
- top-level binding-style transform requires parameter list
- top-level binding-only transform requires parameter list

### test_parser_errors_more_transforms_canonical.cpp

- statement transform requires callable syntax
- expression transform requires callable syntax
- canonical mode rejects transform-prefixed loop body sugar
- canonical mode rejects transform-prefixed while body sugar
- canonical mode rejects transform-prefixed for body sugar
- canonical mode rejects transform-prefixed loop body sugar in expression
- canonical mode rejects transform-prefixed if body sugar in expression
- canonical mode rejects transform-prefixed match body sugar in expression
- transform-prefixed loop parses without body

## parser/root (1 tests, 1 files)

### test_parser_errors.cpp

- reports line and column on parse error

## semantics/bindings (242 tests, 8 files)

### test_semantics_bindings_assignments.cpp

- collections validate
- collection literals reject labeled entries
- array requires one template argument
- array rejects envelope-level length template arg
- map constructor odd argument count uses ordinary diagnostics
- assign to mutable binding succeeds
- assign to immutable binding fails
- assign allowed in expression context
- assign to inferred mutable binding from convert call succeeds
- assign to indexed vector element succeeds
- assign to indexed inferred vector element succeeds
- assign to namespaced indexed inferred vector element succeeds
- assign to std-namespaced indexed inferred vector element succeeds
- assign to indexed vector field succeeds
- assign to namespaced indexed vector field succeeds
- pointer binding and dereference assignment validate
- reference participates in arithmetic
- location accepts reference binding
- dereference requires pointer or reference
- literal statement validates
- pointer assignment requires mutable binding
- assign through non-pointer dereference fails
- reference binding assigns to target
- reference assignment requires mutable binding
- reference binding requires location
- reference binding rejects location pointer type mismatch
- reference binding rejects location reference type mismatch
- safe scope accepts pointer to reference conversion initializer at semantics time

### test_semantics_bindings_control_flow.cpp

- if validates block arguments
- if rejects float condition
- if expression rejects void blocks
- if expression rejects mixed string/numeric branches
- if expression accepts nested string branches
- repeat validates block arguments
- repeat validates bool count
- block expression validates and introduces scope
- block expression accepts direct uninitialized borrow reference binding
- block expression accepts dereferenced uninitialized borrow reference binding
- block expression allows explicit return value
- block expression allows early return
- block expression rejects void return value
- if expression allows return value in branch blocks
- if expression branch accepts direct uninitialized borrow reference binding
- if expression branch accepts dereferenced uninitialized borrow reference binding
- block expression must end with value
- block expression rejects void tail
- block expression requires body arguments
- block statement rejects arguments
- block requires body arguments
- block scope does not leak bindings
- block bindings infer primitive type from initializer expressions
- repeat rejects float count
- repeat accepts bool count
- repeat rejects string count

### test_semantics_bindings_control_flow_borrowing.cpp

- borrow checker rejects assign before pointer alias use in repeat body
- borrow checker allows assign after pointer alias use in repeat body
- borrow checker rejects assign after pointer alias use across while iterations
- borrow checker rejects assign after pointer alias use across for iterations
- borrow checker rejects assign after pointer alias use across loop iterations
- borrow checker rejects assign after pointer alias use across repeat iterations
- borrow checker allows assign after pointer alias use in single-iteration loop
- borrow checker allows assign after pointer alias use in single-iteration repeat
- borrow checker rejects assign before pointer alias use in if branch
- borrow checker allows assign after pointer alias use in if branch
- borrow checker diagnostics include root and sink for pointer alias writes
- borrow checker allows branch-local assign after last pointer alias use with no merge use
- borrow checker rejects branch-local assign before post-merge pointer alias use
- borrow checker rejects match-branch assign before post-merge pointer alias use
- borrow checker rejects body-block assign before post-block pointer alias use
- borrow checker allows body-block assign after last pointer alias use with no post-block use
- borrow checker rejects lambda-capture assign before later pointer alias use
- borrow checker allows lambda-capture assign after last pointer alias use with no later use

### test_semantics_bindings_core.cpp

- local binding requires initializer
- local binding accepts brace initializer
- local binding infers type without transforms
- binding initializer infers type from value block
- binding initializer rejects void call
- binding infers type from user call
- binding inference prefers user definition named array
- binding inference keeps builtin array collection type
- binding inference prefers user definition named vector template call
- binding inference keeps builtin vector literal type
- vector binding accepts explicit mut type with constructor call
- vector binding infers mut type from constructor call
- vector binding accepts omitted explicit initializer
- local binding type must be supported
- software numeric bindings are rejected
- software numeric collection bindings are rejected
- soa_vector binding rejects compatibility spelling
- soa public spelling validates bindings returns references and pointers
- soa_vector binding rejects compatibility spelling before element checks
- soa public spelling requires struct element type
- soa_vector binding rejects disallowed element field envelope
- soa_vector binding rejects nested struct disallowed envelope
- field-only definition can be used as a type
- non-field definition is not a valid type
- float binding validates
- float binding accepts math expression with imported constants
- explicit fixed numeric binding rejects incompatible kind
- bool binding validates
- string binding validates
- copy binding validates
- binding rejects effects transform arguments
- binding rejects effects transform without explicit type
- binding rejects capabilities transform arguments
- binding rejects return transform with explicit type
- binding rejects transform arguments
- binding rejects placement transforms
- parameter rejects placement transforms
- binding rejects duplicate static transform
- restrict binding validates
- restrict binding accepts int alias
- restrict requires template argument
- restrict rejects duplicate transform
- restrict rejects mismatched type
- restrict rejects software numeric types
- stdlib-owned definitions keep direct stdlib constructor imports visible

### test_semantics_bindings_core_transforms.cpp

- binding align_bytes validates
- binding align_bytes rejects template arguments
- binding align_bytes rejects wrong argument count
- binding align_kbytes rejects template arguments
- binding align_kbytes rejects invalid
- binding align_kbytes rejects wrong argument count
- binding align_kbytes validates
- binding align_bytes rejects invalid
- binding rejects return transform with inferred type
- binding qualifiers are allowed
- binding rejects multiple visibility qualifiers

### test_semantics_bindings_pointers.cpp

- pointer helpers validate
- dereference accepts helper-returned pointer calls
- dereference auto inference accepts helper-returned pointer calls
- dereference rejects helper-returned non-pointer calls
- dereference auto inference rejects helper-returned non-pointer calls
- pointer helpers reject template arguments
- pointer helpers reject block arguments
- memory intrinsics validate with heap_alloc effect
- pointer targets allow top-level uninitialized storage
- pointer helper roots allow uninitialized borrow binding
- reference targets allow top-level uninitialized storage
- variadic pointer packs allow top-level uninitialized storage elements
- variadic reference packs allow top-level uninitialized storage elements
- memory at validates checked pointer access
- memory at rejects non-pointer target
- memory at rejects mismatched index and count kinds
- memory at_unsafe validates unchecked pointer access
- memory at_unsafe rejects non-pointer target
- memory at_unsafe rejects template arguments
- alloc requires heap_alloc effect
- alloc rejects unsupported pointer target type
- realloc requires pointer target
- free rejects template arguments
- binding array type requires one template argument
- binding map type requires two template arguments
- binding canonical map type stays ordinary stdlib code
- pointer bindings require template arguments
- pointer bindings accept struct targets
- reference bindings require template arguments
- reference bindings accept struct targets
- reference bindings accept array targets
- reference bindings accept Buffer targets
- pointer bindings accept Buffer targets
- pointer bindings accept array targets
- reference bindings reject array without element type
- pointer bindings reject array without element type
- pointer bindings reject unknown targets
- reference bindings reject unknown targets
- location requires local binding name
- location accepts parameters
- dereference accepts pointer parameters
- pointer arithmetic accepts pointer parameters
- assign allows mutable parameters
- assign allows mutable pointer parameters
- binding allows templated type
- binding allows struct types
- binding allows untagged struct definitions
- typed binding accepts empty lower-case constructor
- struct brace constructor accepts named arguments
- struct brace constructor allows positional after labels
- struct brace constructor allows defaulted fields
- struct brace constructor accepts named arguments while skipping static fields
- struct brace constructor rejects extra arguments
- struct brace constructor keeps argument mismatch when static fields are skipped
- struct call-shaped constructor rejects field mapping
- wrapper-returned struct constructor validates in resolved helper arguments
- wrapper-returned struct constructor keeps resolved helper mismatch diagnostics
- struct brace constructor works in arguments
- struct brace constructor accepts labeled arguments
- struct brace constructor accepts multiple positional arguments
- struct brace constructor rejects duplicate labels
- struct brace constructor rejects too many positional entries
- struct brace constructor accepts return value blocks
- struct brace constructor uses last expression
- binding initializer accepts labeled arguments
- binding initializer accepts labeled struct-literal local bindings
- binding initializer shorthand still rejects named args for builtins
- binding initializer accepts return value blocks
- binding initializer rejects named args for builtins
- binding initializer allows struct constructor block
- binding initializer allows struct constructor if
- struct constructor rejects unknown named arguments
- binding resolves struct types in namespace
- binding allows pointer types
- i64 and u64 bindings validate
- pointer plus validates
- pointer minus validates
- pointer minus accepts u64 offsets
- pointer plus accepts i64 offsets
- pointer minus accepts i64 offsets
- pointer minus rejects pointer + pointer
- pointer minus rejects pointer on right
- pointer plus accepts reference locations
- pointer plus rejects pointer + pointer
- pointer plus rejects pointer on right
- pointer plus rejects non-integer offset

### test_semantics_bindings_struct_defaults.cpp

- struct binding omits initializer when effect-free
- omitted initializer rejects non-struct binding
- omitted initializer rejects effectful Create
- omitted initializer accepts builtin array field initializer
- omitted initializer rejects effectful user-defined array field initializer
- omitted initializer accepts effect-free Create
- omitted initializer rejects effect-free Create with vector alias call helper fallback
- omitted initializer rejects effect-free Create with vector alias method helper fallback
- omitted initializer rejects effect-free Create with canonical slash-path vector method helper
- omitted initializer keeps explicit-template vector alias call diagnostics in Create
- omitted initializer rejects Create when extra-arg vector alias method fallback makes constructor effectful
- omitted initializer accepts effect-free Create with bare array count method
- explicit-template vector alias fallback keeps mismatch diagnostics in Create
- vector alias method fallback keeps canonical mismatch diagnostics in Create

### test_semantics_bindings_struct_defaults_maps.h

- omitted initializer rejects Create with canonical map method precedence when constructor is not effect-free
- map method precedence now rejects omitted initializer through Create effectfulness gate
- canonical map call precedence keeps builtin count diagnostics before omitted initializer
- map call precedence keeps builtin diagnostics before omitted initializer
- omitted initializer rejects Create with canonical slash-path map call helper when constructor is not effect-free
- omitted initializer keeps alias diagnostics for map call helper fallback
- wrapper-returned canonical map call keeps builtin count diagnostics before omitted initializer
- omitted initializer keeps canonical diagnostics for wrapper-returned map call helper fallback
- omitted initializer rejects wrapper-returned canonical map method helper fallback when constructor is not effect-free
- omitted initializer keeps canonical diagnostics for wrapper-returned map method helper fallback
- omitted initializer allows Create to fill missing fields
- omitted initializer rejects Create missing fields
- zero-arg struct call allows Create to fill missing fields
- zero-arg struct call rejects missing Create fields

## semantics/calls_and_flow (370 tests, 9 files)

### test_semantics_calls_and_flow_access.cpp

- count helper validates on string binding
- count builtin rejects block arguments
- count builtin rejects missing argument
- count rejects non-collection target
- count forwards to type method
- count helper validates on variadic args parameter
- count method validates on variadic args parameter
- count on variadic args still rejects extra arguments
- variadic args access helper validates
- variadic args access method validates
- variadic args unsafe access method validates
- variadic args index validates
- variadic args index supports string method resolution
- variadic args index rejects non-integer index
- count rejects missing type method
- array method calls resolve to definitions
- map method calls resolve to definitions
- imported map size method calls resolve to local definitions
- rooted map helper aliases use ordinary explicit path diagnostics
- map pre-dispatch inference keeps rooted and canonical helper paths isolated
- vector method calls resolve to definitions
- method calls support templated definitions
- templated method calls infer receiver types
- templated method call on user-defined vector call receiver resolves by return type
- method calls reject template args for block-inferred receiver
- method calls reject template args on non-template defs
- method calls reject pointer receivers
- unknown method calls fail
- method calls resolve on struct constructor expressions

### test_semantics_calls_and_flow_access_indexing.h

- count builtin rejects template arguments
- count method rejects template arguments
- count method rejects block arguments
- array access rejects template arguments
- array literal access validates
- array access rejects argument count mismatch
- array access rejects block arguments
- unsafe array access rejects template arguments
- unsafe array access rejects argument count mismatch
- unsafe array access rejects block arguments
- array access rejects non-integer index
- unsafe array access rejects non-integer index
- string access rejects non-integer index
- string access validates integer index
- string literal access rejects non-integer index
- string literal access validates integer index
- unsafe string access rejects non-integer index
- unsafe string access validates integer index
- unsafe string literal access rejects non-integer index
- unsafe string literal access validates integer index
- array access rejects non-collection target
- unsafe array access rejects non-collection target

### test_semantics_calls_and_flow_comparisons_literals.cpp

- string comparisons validate
- string comparisons reject mixed types
- builtin at map string comparisons reject mixed types
- builtin map at comparisons prefer canonical map access over root at
- arithmetic operators reject bool operands
- comparisons reject non-numeric operands
- comparisons allow bool with signed integers
- comparisons reject bool with unsigned
- comparisons reject mixed int and float operands
- stdlib map constructor validates through imported helper
- stdlib map constructor validates bool keys and values
- array literal validates single element argument passing
- array literal missing template arg fails
- vector literal missing template arg fails
- soa literal missing template arg fails
- soa literal validates when element type is soa-safe struct
- soa literal requires struct element type
- soa literal rejects string element field envelope
- soa literal rejects nested template element field envelope
- soa literal rejects nested struct disallowed envelope
- soa literal accepts nested primitive struct fields
- soa literal requires heap_alloc effect when non-empty
- array literal type mismatch fails
- vector literal type mismatch fails
- array literal rejects software numeric type
- vector literal rejects software numeric type
- map constructor without import fails ordinary resolution
- map constructor rejects software numeric types
- map constructor key type mismatch fails
- map constructor value type mismatch fails
- map constructor odd raw argument count uses ordinary argument diagnostics
- map access validates key type
- map constructor access validates
- map constructor access validates for string keys
- unsafe map access validates key type
- string map access rejects numeric index
- unsafe string map access rejects numeric index
- map access accepts matching key type
- map access accepts string key expression
- unsafe map access accepts string key expression

### test_semantics_calls_and_flow_control_blocks.h

- statement call with block arguments validates
- block arguments require definition target
- statement call with block arguments accepts bindings
- method call with block arguments targets definition
- block binding infers string type
- block expression with parens validates
- block expression allows return value
- block expression requires block arguments
- user-defined block call ignores builtin rules
- block expression rejects arguments
- block expression requires a value
- block expression must end with expression
- if missing else fails
- if condition requires bool
- if statement sugar accepts call arguments
- if rejects trailing block arguments
- if blocks ignore definition name collisions
- if requires branch envelopes
- if rejects named arguments in source syntax
- if rejects builtin string map access mixed with numeric branch
- if accepts bare map at before branch compatibility
- labeled arguments accept bracket syntax
- binding not allowed in expression
- execution rejects binding transforms
- unknown identifier fails for bare name
- repeat is statement-only
- string literal count validates

### test_semantics_calls_and_flow_control_core.cpp

- repeat rejects missing count
- repeat requires block arguments
- repeat rejects template arguments
- loop rejects named arguments
- loop body rejects named arguments
- while rejects named arguments
- for rejects named arguments
- for accepts comma separators
- loop rejects template arguments
- while rejects template arguments
- for rejects template arguments
- brace constructor values validate
- loop while for validate
- loop accepts i64 count
- loop accepts u64 count
- for accepts semicolon separators
- loop rejected in value blocks
- loop rejected in single-item value blocks
- while rejected in value blocks
- for rejected in value blocks
- for condition accepts binding
- for step accepts binding
- for condition binding requires bool in source syntax
- for requires init condition step and body
- loop rejects non-block body
- loop rejects negative count
- loop rejects non-integer count
- loop requires count and body
- while condition requires bool
- while requires condition and body
- loop blocks ignore definition name collisions
- shared_scope rejects non-loop statements
- shared_scope rejects arguments
- shared_scope rejects template arguments
- shared_scope requires loop body envelope
- shared_scope hoists while bindings
- reference participates in signedness checks

### test_semantics_calls_and_flow_control_misc.cpp

- namespace blocks may be reopened for intra-namespace calls
- string literal method count validates
- duplicate named arguments fail
- array literal validates multiple elements
- array literal validates bool elements
- vector literal validates
- vector literal validates bool elements
- vector literal requires heap_alloc effect
- expression effects narrow active effects
- expression effects must be subset of enclosing
- vector literal allows heap_alloc effect
- vector literal allows empty without heap_alloc
- named arguments match parameters
- named arguments with reordered params match
- named arguments allow positional fill after labels
- named arguments on method calls allow builtin names
- named arguments reject builtin method calls
- named arguments rejected for assign builtin
- named arguments allow user-defined builtin name
- loop rejects negative i64 count
- loop body propagates statement validation errors
- loop rejects template arguments in suite
- while rejects template arguments in suite
- for rejects template arguments in suite
- while requires condition and body in suite
- for requires init condition step and body in suite
- repeat rejects named arguments in suite
- loop requires count and body in suite
- loop rejects trailing block arguments in suite
- while rejects trailing block arguments in suite
- for rejects trailing block arguments in suite
- loop count requires integer in suite
- while condition requires bool in suite
- for condition expression requires bool in suite
- for condition binding requires bool in suite
- loop count expression validation failure in suite
- while condition expression validation failure in suite
- for condition expression validation failure in suite

### test_semantics_calls_and_flow_effects.cpp

- boolean literal validates
- spawn publishes task facts and wait returns task result
- multi wait publishes stdlib tuple result facts
- spawn requires task effect
- wait requires task effect
- task handles must be waited before return
- task handles reject double wait
- multi wait consumes each task handle
- task handles cannot escape through return
- task handles cannot escape through call arguments
- spawn rejects mutable captures
- spawn rejects reference captures
- if statement sugar validates
- return inside if block validates
- if blocks ignore colliding then definition
- missing return on some control paths fails
- return after partial if validates
- return rejected in execution body
- print requires io_out effect
- dispatch requires gpu_dispatch effect
- std gpu dispatch validates with effect
- definition validation context isolates compute flag between definitions
- definition validation context isolates effects between definitions
- std gpu buffer requires gpu_dispatch effect
- std gpu upload requires array input
- std gpu upload rejects user definition named array call target
- std gpu upload accepts builtin array literal input
- std gpu readback requires buffer input
- std gpu buffer_load requires compute definition
- canonical stdlib gfx Buffer load helper requires compute definition
- std gpu global_id requires compute definition
- legacy gpu_buffer name is rejected
- legacy /gpu/global_id_x path is rejected
- std gpu dispatch requires kernel name
- std gpu dispatch argument count mismatch
- std gpu buffer size requires integer expression
- std gpu buffer requires numeric element type
- std gpu upload rejects template arguments
- std gpu readback rejects template arguments
- std gpu buffer_load rejects template arguments
- std gpu buffer_store rejects template arguments
- std gpu buffer_load requires integer index
- std gpu buffer_store rejects mismatched value type
- canonical stdlib gfx Buffer store helper requires compute definition
- std gpu compute builtins validate
- execution effects must be subset of definition effects
- top-level execution effects must be subset of target effects
- execution effects scope vector literals
- implicit default effects allow print
- print_error requires io_err effect
- print_line_error requires io_err effect
- notify requires pathspace_notify effect
- notify rejects non-string path argument
- notify rejects user-defined at on user-defined vector target
- notify accepts string array access
- notify rejects string map access without path inference
- notify rejects template arguments
- notify rejects argument count mismatch
- notify rejects block arguments
- insert requires pathspace_insert effect
- insert rejects template arguments
- insert rejects non-string path argument
- insert accepts string array access
- insert rejects block arguments
- insert rejects argument count mismatch
- insert not allowed in expression context
- take requires pathspace_take effect
- take rejects template arguments
- take rejects non-string path argument
- take rejects string map access without path inference
- take rejects block arguments
- take rejects argument count mismatch
- take not allowed in expression context
- bind requires pathspace_bind effect
- unbind requires pathspace_bind effect
- schedule requires pathspace_schedule effect
- schedule rejects non-string path argument
- notify not allowed in expression context
- print not allowed in expression context
- print rejects pointer argument
- print rejects collection argument
- print rejects missing arguments
- print_line rejects block arguments
- print_line rejects missing arguments
- print_error rejects missing arguments
- print_error rejects block arguments
- print_line_error rejects missing arguments
- print_line_error rejects block arguments
- array literal rejects block arguments
- map constructor block arguments require definition target
- vector literal rejects block arguments
- print accepts string array access
- print accepts string map access
- print accepts string vector literal access
- definition rejects duplicate identical effects transforms
- print rejects user-defined at on user-defined vector target
- print rejects struct block value
- print accepts string binding
- print accepts bool binding
- print accepts bool literal
- print rejects float literal
- print_error accepts bool binding
- print_line_error accepts bool binding
- default effects allow print
- default effects allow print_error
- default effects allow vector literal
- default effects reject invalid names
- definition validation context isolates moved bindings
- definition validation context isolates active effects
- definition validation context isolates on_error handlers
- File constructor requires file_read effect for read mode
- file read methods require file_read effect
- file write methods still require file_write effect
- file_write effect allows file operations
- file_write effect still allows read operations
- file read_byte accepts mutable integer binding
- file read_byte rejects immutable binding
- file write_bytes accepts builtin array argument
- file write_bytes rejects user definition named array
- try requires on_error
- user-defined try helper keeps named arguments
- builtin try rejects named arguments
- on_error allows int return type for graphics-style flow
- type namespace helper call drops synthetic receiver
- type namespace helper auto inference drops synthetic receiver
- on_error rejects non-Result non-int return type
- statement on_error rejects non-block execution target
- statement block on_error reports block return requirement
- try rejects mismatched error type

### test_semantics_calls_and_flow_named_args.cpp

- unknown named argument fails
- named argument duplicates positional parameter fails
- execution named arguments match parameters
- execution named arguments allow positional fill after labels
- execution named argument duplicates positional parameter fails
- execution unknown named argument fails in source syntax
- execution duplicate named argument fails
- execution arguments reject named builtins
- named arguments not allowed on builtin calls in source syntax
- named argument cannot bind variadic parameter
- named fixed argument still allows trailing variadic positionals
- method call fixed argument still allows trailing variadic positionals
- execution named argument cannot bind variadic parameter
- method call variadic parameter rejects mismatched trailing argument
- execution variadic parameter rejects mismatched trailing argument

### test_semantics_calls_and_flow_numeric_builtins.cpp

- convert builtin validates
- convert missing template arg fails
- convert rejects missing argument
- convert rejects extra arguments
- convert unsupported template arg fails
- convert rejects decimal target
- convert rejects integer target
- convert<bool> accepts u64 literal
- convert<bool> accepts float operand
- convert rejects string operand
- convert accepts user-defined vector-named operand call
- convert still rejects builtin vector literal operand
- convert resolves struct helper
- convert resolves imported struct helper
- convert rejects imported private helper
- convert rejects ambiguous helper overloads
- convert rejects helper with wrong return type
- convert rejects helper with wrong param count
- convert rejects missing struct helper
- abs rejects non-numeric operand
- sign rejects string operand
- saturate rejects string operand
- pow accepts integer operands
- pow accepts float operands
- pow rejects mixed int/float operands
- pow rejects mixed signed/unsigned operands
- sin rejects integer operand
- rounding math builtins validate
- float predicate math builtins validate
- min rejects non-numeric operand
- max rejects non-numeric operand
- clamp rejects non-numeric operand
- clamp rejects mixed signed/unsigned operands

## semantics/calls_and_flow/collections (1312 tests, 31 files)

### test_semantics_calls_and_flow_collections_bare_map_call_form_statement_args.cpp

- bare map call form statement body arguments fall back to canonical helper target
- bare map call form statement body arguments keep validating through canonical helper target
- bare map helper expression body arguments keep canonical alias diagnostics
- bare map call form expression body arguments reject unresolved count target
- bare map call form expression body arguments reject before canonical mismatch
- reference-wrapped map helper receiver expression body arguments fall back to canonical helper target
- reference-wrapped map helper receiver expression body arguments keep canonical mismatch diagnostics
- map namespaced count method expression body arguments validate through stdlib helper target
- map namespaced at method expression body arguments validate through slash-path target
- map stdlib call form expression body arguments use canonical helper target
- vector namespaced access alias chained method rejects canonical struct-return forwarding
- vector namespaced access alias chained method keeps removed-alias diagnostics
- vector namespaced access alias field expression now fails on project inference first
- vector canonical access call keeps same-path struct receiver forwarding
- vector canonical unsafe access call forwards field inference
- map namespaced access call keeps canonical struct-return forwarding
- map namespaced access alias rejects canonical struct-return forwarding
- map namespaced access alias reports current receiver diagnostics
- map namespaced unsafe access alias rejects canonical struct-return forwarding
- map namespaced unsafe access alias reports current receiver diagnostics
- vector namespaced access alias field expression keeps removed-alias diagnostics
- canonical vector constructor call infers canonical helper return kind
- canonical vector constructor auto binding infers same-path helper return kind
- canonical vector constructor auto binding ignores experimental constructor rewrite
- canonical vector constructor auto binding keeps same-path overload family
- canonical vector constructor helper return keeps non-vector count diagnostics
- templated canonical vector constructor helper return keeps non-vector count diagnostics
- templated canonical vector constructor helper return keeps non-vector capacity diagnostics
- templated experimental vector constructor helper return keeps non-vector count diagnostics
- vector constructor alias call keeps removed-alias diagnostics
- canonical vector constructor call keeps local same-path helper overload
- explicit vector import supports bare stdlib constructor and namespaced helpers
- explicit vector import keeps duplicate same-path ctor diagnostics
- wildcard vector import supports concise vector binding example
- vector method alias access rejects routed array receiver diagnostics
- vector method alias access field expression keeps rooted alias diagnostics
- vector unsafe method alias access struct method chain keeps array receiver diagnostics

### test_semantics_calls_and_flow_collections_bare_vector_pop_helper_resolution.cpp

- bare vector pop statement body arguments validate without imported stdlib helper
- bare vector pop template specialization keeps canonical unknown target without import
- bare vector pop arity mismatch keeps canonical unknown target without import
- pop call validates through imported vector helper before user helper
- pop method keeps user-defined vector helper precedence
- vector clear alias keeps rooted unknown target without helper
- bare vector clear validates through imported stdlib helper
- clear allows vector elements with nested drop requirements
- direct experimental vector constructor binds as Vector under wildcard import
- direct experimental vector constructor keeps temporary receiver inference
- clear call validates through imported vector helper before user helper
- clear method keeps user-defined vector helper precedence
- bare vector clear template specialization keeps canonical unknown target without import
- bare vector clear arity mismatch keeps canonical unknown target without import
- vector remove_at alias keeps rooted unknown target without helper
- vector remove_at alias keeps rooted unknown target before index validation
- remove_at bool index keeps routed unknown target diagnostics
- bare vector remove_at template specialization keeps canonical unknown target without import
- bare vector remove_at arity mismatch keeps canonical unknown target without import
- bare vector remove_at validates through imported stdlib helper
- remove_at accepts ownership-sensitive vector element types once survivor motion is wired
- remove_at accepts nested ownership-sensitive vector element types once survivor motion is wired
- remove_at call validates through imported vector helper before user helper
- remove_at method keeps user-defined vector helper precedence
- vector remove_swap alias keeps rooted unknown target before index validation
- remove_swap bool index keeps routed unknown target diagnostics
- vector remove_swap alias keeps rooted unknown target without helper
- bare vector remove_swap template specialization keeps canonical unknown target without import
- bare vector remove_swap arity mismatch keeps canonical unknown target without import
- remove_swap accepts ownership-sensitive vector element types once survivor motion is wired
- remove_swap accepts nested ownership-sensitive vector element types once survivor motion is wired
- bare vector remove_swap validates through imported stdlib helper
- remove_swap call validates through imported vector helper before user helper
- remove_swap method keeps user-defined vector helper precedence
- vector helpers in expressions keep statement-only diagnostics
- vector helper named args on array targets report vector binding

### test_semantics_calls_and_flow_collections_container_error_and_result_helpers.cpp

- container error contract shape validates through Result.why
- containerErrorResult helper validates for value-carrying container results
- mapTryAt helper validates for supported i32 container result payloads
- mapTryAt helper validates for supported bool container result payloads
- mapTryAt helper validates for supported string container result payloads
- bare vector count call resolves through imported stdlib helper
- bare collection count resolves inside stdlib namespace helper
- args pack count method resolves inside stdlib vector namespace
- bare vector count call requires imported stdlib helper or explicit definition
- bare vector count wrapper call resolves through imported stdlib helper
- bare vector count wrapper call requires imported stdlib helper or explicit definition
- bare vector capacity wrapper call resolves through imported stdlib helper
- bare vector capacity wrapper call requires imported stdlib helper or explicit definition
- count helper rejects retired soa_vector binding
- count builtin validates on method calls
- count method on vector binding accepts same-path helper
- count method on vector binding requires helper
- count method rejects retired soa_vector binding
- imported count forms reject retired soa_vector binding
- explicit old-surface soa_vector count rejects retired type spelling
- explicit old-surface soa_vector count slash-method rejects retired type spelling
- explicit soa_vector count forms reject non-soa target
- public soa count helper validates through struct helper return receivers
- bare count helper rejects internal soa helper return receivers with current metadata diagnostic
- count method rejects internal soa helper return receivers through retired method path
- legacy soa_vector compatibility count helper shadow through public soa receivers
- soa_vector method count fallback keeps same-path helper shadow through struct helper return receivers compatibility
- experimental soa_vector stdlib helpers reject direct soa wildcard import
- public soa count helper validates on public wrapper bindings
- public soa get helper validates on public wrapper bindings
- public soa get helper rejects template arguments on non-soa receiver
- public soa get slash-method validates on public wrapper
- public soa to_aos slash-method validates on public wrapper
- public soa ref helper validates on public wrapper bindings
- public soa mutator helpers validate on public wrapper bindings
- borrowed helper-return experimental soa_vector helper surfaces rejected push path
- public soa to_aos helper validates on public wrapper bindings
- public soa count helper validates inside generic helper on public wrapper parameter
- wildcard-imported public soa helpers reject current metadata inference gap
- public soa wildcard import rejects without collections import
- experimental soa_vector stdlib helpers reject primitive element types
- experimental soa_vector stdlib helpers reject non-reflect struct element types
- experimental soa_vector stdlib non-empty helper rejects direct soa wildcard import
- experimental soa_vector stdlib wide reflect-enabled structs reject direct soa wildcard import
- experimental soa_vector stdlib from-aos helper validates on reflect-enabled struct elements
- experimental soa_vector stdlib to-aos helper validates on reflect-enabled struct elements
- experimental soa_vector stdlib to-aos method validates on wrapper surface
- experimental soa_vector stdlib non-empty to-aos helper validates on wrapper state
- experimental soa_vector stdlib non-empty to-aos method validates on wrapper state
- experimental soa_vector borrowed parameter read-only methods reject retired get_ref path
- experimental soa_vector inline location read-only methods reject rooted helper path
- experimental soa_vector inline location borrowed helper-return helper surfaces reject current ref_ref path
- experimental soa_vector borrowed helper-return helper surfaces reject current ref_ref path
- experimental soa_vector alias-only borrowed helper-return helpers reject direct soa wildcard import
- experimental soa_vector method-like borrowed helper-return helper surfaces reject current ref_ref path
- experimental soa_vector direct return method-like borrowed helper-return reads reject retired helper path
- experimental soa_vector direct helper shadows validate without duplicate reserve diagnostics
- experimental soa_vector inline location method-like borrowed helper-return helper surfaces reject current ref_ref path
- experimental soa_vector direct return inline location method-like borrowed helper-return reads reject retired helper path
- experimental soa_vector stdlib get helper rejects direct soa wildcard import
- experimental soa_vector stdlib get method rejects direct soa wildcard import
- experimental soa_vector stdlib ref helper rejects direct soa wildcard import
- experimental soa_vector stdlib ref method rejects direct soa wildcard import
- experimental soa_vector stdlib ref method pass-through rejects direct soa wildcard import
- experimental soa_vector standalone ref method rejects direct soa wildcard import
- experimental soa_vector standalone ref method push conflict rejects direct soa wildcard import
- experimental soa_vector standalone ref helper reserve conflict rejects direct soa wildcard import
- experimental soa_vector helper-return ref method rejects direct soa wildcard import
- experimental soa_vector helper-return ref method push conflict rejects direct soa wildcard import
- experimental soa_vector borrowed helper-return ref method rejects direct soa wildcard import
- experimental soa_vector inline location borrowed helper-return ref rejects direct soa wildcard import
- experimental soa_vector stdlib push and reserve helpers reject direct soa wildcard import
- experimental soa_vector stdlib push and reserve methods reject direct soa wildcard import
- experimental soa_vector stdlib single-field index syntax reports field_count diagnostic
- experimental soa_vector stdlib field-view method reports escape diagnostic
- experimental soa_vector borrowed local field-view method reports escape diagnostic
- experimental soa_vector borrowed local field-view call-form validates
- experimental soa_vector inline location field-view methods report field_count diagnostic
- experimental soa_vector inline location borrowed helper-return field-view methods report field_count diagnostic
- experimental soa_vector borrowed helper-return field-view call-form validates
- experimental soa_vector method-like borrowed helper-return field-view methods validate
- experimental soa_vector mutating field-view index reports field_count diagnostic
- experimental soa_vector mutating field-view call-form index reports field_count diagnostic
- experimental soa_vector mutating field-view method reports pending diagnostic
- experimental soa_vector mutating field-view call-form reports pending diagnostic
- experimental soa_vector mutating dereferenced borrowed helper-return field-view index reports field_count diagnostic
- experimental soa_vector mutating dereferenced borrowed helper-return field-view method reports pending diagnostic
- experimental soa_vector mutating inline location borrowed helper-return field-view indexes validate
- experimental soa_vector mutating method-like borrowed helper-return field-view indexes validate
- experimental soa_vector mutating inline location borrowed helper-return field-view methods report pending diagnostic
- experimental soa_vector mutating method-like borrowed helper-return field-view methods report pending diagnostic
- experimental soa_vector mutating ref field access rejects internal metadata validation
- experimental soa_vector bare get and ref field access rejects internal metadata validation
- experimental soa_vector stdlib reflected multi-field index syntax reports field_count diagnostic
- experimental soa_vector stdlib reflected call-form index syntax reports field_count diagnostic
- experimental soa_vector stdlib reflected inline location borrow index syntax reports field_count diagnostic
- experimental soa_vector stdlib reflected borrowed dereference index syntax reports field_count diagnostic
- experimental soa_vector stdlib reflected borrowed local index syntax reports field_count diagnostic
- experimental soa_vector stdlib reflected borrowed helper-return index syntax reports field_count diagnostic
- experimental soa_vector stdlib reflected dereferenced borrowed helper-return index syntax reports field_count diagnostic
- experimental soa_vector stdlib direct return borrowed helper-return reads reject retired count ref
- experimental soa_vector stdlib reflected method-like borrowed helper-return index syntax reports field_count diagnostic
- experimental soa_vector stdlib reflected inline location borrowed helper-return index syntax reports field_count diagnostic
- experimental soa_vector stdlib direct return inline location borrowed helper-return reads reject retired count ref
- experimental soa storage helpers validate on explicit column bindings
- experimental soa storage helpers validate ownership-sensitive elements
- experimental soa storage borrowed ref helper validates on explicit column bindings
- experimental soa storage borrowed view helper validates on explicit column bindings
- experimental soa storage borrowed view helper validates shared writes
- experimental soa storage borrow-slot helper validates reference return
- experimental vector borrow-slot helper validates reference return
- experimental two-column soa storage helpers validate on explicit column bindings
- experimental two-column soa storage helpers validate ownership-sensitive elements
- experimental three-column soa storage helpers validate on explicit column bindings
- experimental three-column soa storage helpers validate ownership-sensitive elements
- experimental four-column soa storage helpers validate on explicit column bindings
- experimental four-column soa storage helpers validate ownership-sensitive elements
- experimental five-column soa storage helpers validate on explicit column bindings
- experimental five-column soa storage helpers validate ownership-sensitive elements
- experimental six-column soa storage helpers validate on explicit column bindings
- experimental six-column soa storage helpers validate ownership-sensitive elements
- experimental seven-column soa storage helpers validate on explicit column bindings
- experimental seven-column soa storage helpers validate ownership-sensitive elements
- experimental eight-column soa storage helpers validate on explicit column bindings
- experimental eight-column soa storage helpers validate ownership-sensitive elements
- experimental nine-column soa storage helpers validate on explicit column bindings
- experimental nine-column soa storage helpers validate ownership-sensitive elements
- experimental ten-column soa storage helpers validate on explicit column bindings
- experimental ten-column soa storage helpers validate ownership-sensitive elements
- experimental eleven-column soa storage helpers validate on explicit column bindings
- experimental eleven-column soa storage helpers validate ownership-sensitive elements
- experimental twelve-column soa storage helpers validate on explicit column bindings
- experimental twelve-column soa storage helpers validate ownership-sensitive elements
- experimental thirteen-column soa storage helpers validate on explicit column bindings
- experimental thirteen-column soa storage helpers validate ownership-sensitive elements
- experimental fourteen-column soa storage helpers validate on explicit column bindings
- experimental fourteen-column soa storage helpers validate ownership-sensitive elements
- experimental fifteen-column soa storage helpers validate on explicit column bindings
- experimental fifteen-column soa storage helpers validate ownership-sensitive elements
- experimental sixteen-column soa storage helpers validate on explicit column bindings
- experimental sixteen-column soa storage helpers validate ownership-sensitive elements
- get helper rejects retired soa_vector binding
- get method rejects retired soa_vector binding
- imported get forms reject retired builtin soa_vector binding
- explicit old-surface soa_vector get rejects retired binding spelling
- explicit old-surface soa_vector get slash-method rejects retired binding spelling
- explicit old-surface soa_vector get_ref rejects retired binding spelling
- explicit old-surface soa_vector get_ref slash-method rejects retired binding spelling
- get root forms reject vector target
- canonical get helper validates through struct helper return receivers compatibility
- bare get helper through experimental soa helper return receivers rejects internal metadata validation
- get method validates through experimental soa helper return receivers
- get method fallback keeps same-path helper shadow through struct helper return receivers compatibility
- ref helper rejects retired soa_vector binding
- ref method rejects retired soa_vector binding
- imported ref local bindings reject retired builtin soa_vector binding
- explicit old-surface soa_vector ref rejects retired binding spelling
- explicit old-surface soa_vector ref slash-method rejects retired binding spelling
- explicit old-surface soa_vector ref_ref rejects retired binding spelling
- explicit old-surface soa_vector ref_ref slash-method rejects retired binding spelling
- ref root forms reject vector target
- canonical ref helper through struct helper return receivers rejects retired path
- bare ref helper through experimental soa helper return receivers rejects internal metadata validation
- ref method validates through experimental soa helper return receivers
- ref method fallback keeps same-path helper shadow through struct helper return receivers compatibility
- ref method fallback keeps same-path helper shadow for auto inference through struct helper return receivers compatibility
- ref call fallback auto inference rejects internal metadata validation through struct helper return receivers
- ref call fallback direct returns reject internal metadata validation through struct helper return receivers
- ref_ref fallback keeps same-path helper shadow through borrowed helper return receivers compatibility
- get call fallback auto inference rejects internal metadata validation through struct helper return receivers
- get call fallback direct returns reject internal metadata validation through struct helper return receivers
- count call fallback auto inference rejects internal metadata validation through struct helper return receivers
- count call fallback direct returns reject internal metadata validation through struct helper return receivers
- push helper shadow through helper-return expressions rejects internal metadata validation
- reserve helper shadow through helper-return expressions rejects internal metadata validation
- explicit same-path soa_vector helper shadows work for helper-return direct calls compatibility
- method-like same-path soa_vector helper shadows validate without duplicate canonical reserve diagnostics
- push and reserve bare and method forms reject internal metadata validation on experimental soa_vector bindings
- explicit soa_vector mutators reject retired builtin soa_vector binding
- imported soa_vector mutators reject retired builtin soa_vector binding
- explicit soa_vector reserve keeps canonical reject on vector target
- explicit soa_vector push slash-method keeps canonical reject on vector target
- push helper call-form rejects retired soa_vector user-helper parameter
- to_soa helper validates on vector binding
- to_soa method validates on vector binding
- explicit to_soa slash-method validates on vector binding
- to_aos helper rejects retired soa_vector binding
- explicit old-surface to_aos direct call rejects retired soa_vector binding
- to_aos method rejects retired soa_vector binding
- imported non-root to_aos forms reject retired soa_vector binding
- vector return accepts local builtin vector binding
- explicit old-surface to_aos slash-method rejects retired soa_vector binding
- to_aos rejects borrowed indexed retired soa_vector receiver
- to_soa and to_aos helpers reject removed canonical soa_vector to_aos bridge
- to_soa helper rejects retired soa_vector non-vector target
- to_soa helper call-form rejects retired soa_vector user-helper parameter
- to_soa helper method-form falls back to user helper
- non-root to_aos forms keep canonical reject on vector target
- explicit old-surface to_aos_ref direct call rejects retired soa_vector binding
- explicit old-surface to_aos_ref slash-method rejects retired soa_vector binding
- to_aos method fallback through struct helper return receivers rejects removed canonical bridge
- to_aos helper call-form falls back to user helper
- to_aos method fallback validates with same-path helper present on struct helper return receivers compatibility
- to_aos method fallback keeps same-path helper shadow for auto inference through struct helper return receivers compatibility
- to_aos helper method-form rejects retired soa_vector user-helper parameter
- to_aos helper-return builtin soa_vector forms reject non-templated retired path
- builtin soa_vector global helper-return read helpers reject non-templated retired path
- builtin soa_vector method-like helper-return read helpers reject primitive metadata first
- builtin soa_vector method-like helper-return read helpers reject non-reflect Particle metadata first
- aos and soa containers reject retired soa_vector parameter spelling
- ecs style soa_vector update loop rejects retired parameter spelling
- ecs style update loop rejects retired soa_vector parameter before conversion mismatch

### test_semantics_calls_and_flow_collections_count_helpers_and_bare_map_calls.cpp

- count builtin validates on array literals
- count helper validates on array binding
- bare map count call validates through builtin fallback without imported canonical helper
- bare map count call resolves through canonical helper definition
- imported canonical map count validates builtin map method receivers
- imported canonical map count keeps borrowed args pack count_ref diagnostics
- bare map count call rejects when only compatibility alias is present
- bare map count call keeps explicit root helper precedence
- bare map contains call requires imported canonical helper or explicit definition
- bare map contains call resolves through canonical helper definition
- bare map contains call rejects compatibility alias when canonical helper is absent
- bare map contains call keeps explicit root helper precedence
- bare map tryAt call requires imported canonical helper or explicit definition
- bare map tryAt call rejects compatibility helper fallback
- map binding rejects unsupported builtin Comparable key contract
- inferred map binding rejects unsupported builtin Comparable key contract
- experimental map custom comparable struct keys keep canonical map helper diagnostics
- experimental map method-call sugar keeps missing Map helper diagnostics
- canonical map Ref helper calls accept borrowed map references
- public stdlib map Ref wrappers validate through canonical borrowed helpers
- canonical map borrowed method-call sugar rejects missing ref template inference
- canonical map insert helpers validate on value and borrowed mutation surfaces
- canonical map ownership-sensitive values validate through canonical helpers
- canonical namespaced map insert validates on explicit experimental map bindings
- builtin canonical map insert method sugar validates before lowering ownership-sensitive values
- constructor-backed builtin map insert method sugar avoids insert builtin rewrite
- canonical map value methods validate ownership-sensitive values through map helpers
- canonical map borrowed helper calls validate ownership-sensitive values through ref helpers
- canonical map construction rejects nontrivial value relocation until container move semantics land
- canonical stdlib map wrappers resolve through explicit namespaced helpers
- public stdlib map wrapper bridge is retired
- retired public stdlib map wrapper calls report their original target
- canonical stdlib map helpers use standalone stdlib implementation
- canonical map count wrapper ignores removed alias helper
- canonical map access wrapper ignores removed alias helper
- experimental map direct-import shim stays retired
- experimental map bracket access stays unsupported on value and borrowed call receivers
- wrapper-returned experimental map bracket access stays unsupported
- experimental map bracket access on borrowed calls fails before key diagnostics
- canonical map missing ordering trait reports comparable less-than rejection
- canonical map methods include comparable less-than rejection
- canonical map Ref helper calls include comparable less-than rejection
- canonical map borrowed methods include builtin key rejection
- canonical map insert helper calls include comparable less-than rejection
- canonical map borrowed insert methods include comparable less-than rejection

### test_semantics_calls_and_flow_collections_experimental_map_auto_inference.cpp

- helper-wrapped map constructors infer canonical auto locals
- helper-wrapped map constructor auto inference keeps template conflict diagnostics
- implicit map constructor auto inference keeps template conflict diagnostics
- inferred canonical map returns rewrite canonical constructors
- inferred canonical map returns keep constructor mismatch diagnostics
- block-bodied inferred canonical map returns rewrite constructors
- block-bodied inferred canonical map returns keep mismatch diagnostics
- auto bindings inside inferred canonical map return blocks rewrite constructors
- auto bindings inside inferred canonical map return blocks keep mismatch diagnostics
- helper-wrapped inferred canonical map returns rewrite nested constructor arguments
- helper-wrapped inferred canonical map returns keep nested constructor mismatch diagnostics
- double helper-wrapped inferred canonical map returns rewrite nested constructor arguments
- double helper-wrapped inferred canonical map returns keep nested constructor mismatch diagnostics
- inferred canonical map call receivers resolve method sugar
- inferred canonical map call receivers keep mismatch diagnostics
- stdlib map constructors accept explicit canonical map struct fields
- stdlib map constructors keep mismatch diagnostics on canonical map struct fields
- stdlib map constructors reject inferred canonical map struct field mismatch
- stdlib map constructors keep mismatch diagnostics on inferred canonical map struct fields
- helper-wrapped inferred canonical map struct fields validate
- helper-wrapped inferred canonical map struct fields keep mismatch diagnostics
- helper-wrapped inferred canonical result map struct fields validate
- helper-wrapped inferred canonical result map struct fields keep mismatch diagnostics

### test_semantics_calls_and_flow_collections_experimental_map_deref_and_struct_storage.cpp

- helper-wrapped Result.ok dereferenced canonical result storage keeps init mismatch
- map constructors keep arg-pack count when soa helpers are imported compatibility
- map constructor mismatch wins over imported soa count alias compatibility
- helper-wrapped dereferenced Result.ok storage keeps mismatch diagnostics
- helper-wrapped map constructors on canonical map struct storage keep init mismatch
- helper-wrapped map struct storage fields keep mismatch diagnostics
- helper-wrapped Result.ok canonical result struct storage keeps init mismatch
- helper-wrapped Result.ok struct storage fields keep mismatch diagnostics
- helper-wrapped map constructors on dereferenced canonical map storage keep init mismatch
- helper-wrapped dereferenced map struct storage fields keep mismatch diagnostics
- map constructors accept dereferenced canonical map storage references
- dereferenced canonical map storage references keep mismatch diagnostics
- helper-wrapped Result.ok dereferenced result struct storage keeps init mismatch
- helper-wrapped dereferenced Result.ok struct storage fields keep mismatch diagnostics
- helper-wrapped Result.ok payloads infer canonical result auto bindings
- helper-wrapped Result.ok auto binding inference keeps template conflict diagnostics
- stdlib wrapper map constructor accepts explicit canonical map returns
- stdlib wrapper map constructor keeps mismatch diagnostics on explicit canonical map returns
- stdlib wrapper map constructor accepts explicit canonical map parameters
- stdlib wrapper map constructor keeps mismatch diagnostics on explicit canonical map parameters
- map constructor assigns into explicit canonical map targets
- canonical map constructor assignment keeps mismatch diagnostics on explicit canonical map bindings
- wrapper map constructor assignment keeps mismatch diagnostics on explicit canonical map parameters
- helper-wrapped map constructor assignments accept explicit canonical map targets
- helper-wrapped map constructor assignments keep mismatch diagnostics on explicit canonical map targets
- implicit map constructors infer canonical auto locals and auto returns

### test_semantics_calls_and_flow_collections_experimental_map_result_payloads.cpp

- helper-wrapped map constructors keep template conflict diagnostics on explicit canonical map parameters
- helper-wrapped Result.ok payloads accept explicit canonical map parameters
- helper-wrapped Result.ok payloads keep template conflict diagnostics on explicit canonical map parameters
- stdlib wrapper map constructor accepts explicit canonical map bindings
- stdlib wrapper map constructor keeps mismatch diagnostics on explicit canonical map bindings
- helper-wrapped map constructors accept explicit canonical map bindings
- helper-wrapped map constructors keep mismatch diagnostics on explicit canonical map bindings
- helper-wrapped Result.ok payloads accept explicit canonical map bindings
- helper-wrapped Result.ok payloads keep mismatch diagnostics on explicit canonical map bindings
- helper-wrapped Result.ok payload assignments accept explicit canonical map result targets
- helper-wrapped Result.ok payload assignments keep mismatch diagnostics on explicit canonical map result targets
- helper-wrapped map constructors accept canonical map dereference assignment targets
- helper-wrapped map dereference assignments keep mismatch diagnostics
- helper-wrapped Result.ok payloads accept canonical map result dereference targets
- helper-wrapped Result.ok dereference assignments keep mismatch diagnostics
- helper-wrapped map constructors reject canonical map uninitialized storage mismatch
- helper-wrapped map constructor uninitialized storage keeps mismatch diagnostics
- helper-wrapped Result.ok canonical map result uninitialized storage keeps init mismatch
- helper-wrapped Result.ok uninitialized storage keeps mismatch diagnostics
- helper-wrapped map constructors on dereferenced canonical map uninitialized storage keep init mismatch
- helper-wrapped dereferenced map storage keeps mismatch diagnostics

### test_semantics_calls_and_flow_collections_field_bound_map_compatibility_calls.cpp

- field-bound canonical map compatibility count calls require alias helpers
- field-bound canonical map stdlib namespaced count methods validate
- stdlib map constructor assignments accept explicit canonical map struct fields
- stdlib map constructor assignments keep mismatch diagnostics on canonical map struct fields
- helper-wrapped map constructor assignments accept inferred canonical map struct fields
- helper-wrapped map constructor assignments keep mismatch diagnostics on inferred canonical map struct fields
- helper-wrapped Result.ok payload assignments accept explicit canonical map result struct fields
- helper-wrapped Result.ok payload assignments keep mismatch diagnostics on explicit canonical map result struct fields
- helper-wrapped map constructor assignments accept dereferenced canonical map struct fields
- helper-wrapped dereferenced map field assignments keep mismatch diagnostics
- helper-wrapped Result.ok assignments accept dereferenced canonical result map struct fields
- helper-wrapped dereferenced Result.ok field assignments keep mismatch diagnostics
- stdlib namespaced map helpers keep Comparable diagnostics on canonical map value receivers
- stdlib namespaced map helpers keep canonical key diagnostics on map references
- map compatibility count call requires explicit alias definition
- map compatibility count auto inference requires explicit alias definition
- map compatibility contains call rejects visible canonical definition
- map compatibility contains call keeps explicit alias precedence
- map compatibility tryAt call rejects visible canonical definition
- map compatibility tryAt call keeps explicit alias precedence
- map wrapper temporary tryAt call validates canonical target classification
- map wrapper temporary tryAt auto inference accepts explicit helper binding as try input
- map wrapper temporary tryAt call requires canonical helper definition

### test_semantics_calls_and_flow_collections_map_compatibility_explicit_template_calls.cpp

- map compatibility explicit-template count call keeps non-templated alias diagnostics
- map compatibility explicit-template count method reports canonical return mismatch
- wrapper reference templated map count method rejects missing canonical helper
- wrapper reference templated map count method rejects missing canonical ref helper
- map slash-path explicit-template count method reports canonical return mismatch
- map canonical slash-path explicit-template access method stays on canonical unknown call target diagnostic
- map canonical explicit-template count call keeps canonical non-templated diagnostics
- map canonical implicit-template count call keeps canonical non-templated diagnostics
- map canonical implicit-template count call infers wrapper slash return envelope
- map canonical implicit-template count wrapper slash return keeps canonical diagnostics
- map canonical wrapper auto local preserves collection template info
- map canonical wrapper auto local keeps builtin count diagnostics
- map canonical reference wrapper auto local preserves borrowed access template info
- map canonical reference wrapper auto local keeps key diagnostics
- canonical map borrowed receiver validates direct stdlib access
- canonical map borrowed receiver keeps parameter key diagnostics
- canonical map borrowed receiver rejects direct stdlib contains
- canonical map borrowed receiver rejects direct stdlib contains before key diagnostics
- canonical map borrowed receiver validates direct stdlib tryAt
- canonical map borrowed receiver keeps tryAt key diagnostics
- explicit canonical map binding keeps builtin helper validation
- explicit canonical map parameter keeps builtin helper validation
- explicit canonical map parameter keeps builtin key diagnostics
- explicit canonical map parameter keeps print statement string validation
- canonical map reference parameter keeps print statement string validation
- wrapper-returned canonical map keeps print statement string validation
- wrapper-returned referenced canonical map keeps print statement string validation
- canonical map reference parameter keeps if string branch compatibility
- wrapper-returned canonical map keeps if string branch compatibility
- explicit canonical map access helpers accept canonical map values

### test_semantics_calls_and_flow_collections_namespaced_collection_statement_body_args.cpp

- array namespaced slash method spelling block-arg diagnostics keep divide target
- array namespaced vector helper call form accepts statement body arguments
- vector namespaced helper call form accepts statement body arguments
- stdlib canonical vector helper call form accepts statement body arguments
- array namespaced vector helper call form rejects expression body arguments
- vector namespaced helper call form expression body arguments currently pass semantics
- stdlib canonical vector helper call-form expression body arguments keep routed diagnostics
- stdlib canonical vector helper namespace body arguments keep unknown target
- array namespaced slash method pointer receiver diagnostics keep array-qualified pointer target
- stdlib namespaced method body-arg diagnostics normalize pointer receiver target
- array namespaced method expression body-arg diagnostics normalize pointer expressions to i32 target
- stdlib namespaced method expression body-arg diagnostics keep unknown vector count target
- array namespaced slash method reference receiver diagnostics keep array-qualified reference target
- stdlib namespaced method expression body-arg diagnostics keep unknown vector count target for references
- array namespaced slash method temporary pointer diagnostics keep array-qualified pointer target
- stdlib namespaced method expression body-arg diagnostics keep unknown vector count target for temporary pointers
- array namespaced method body-arg diagnostics normalize helper-returned reference receiver target
- stdlib namespaced method expression body-arg diagnostics fail on borrow storage before reference receiver normalization
- array namespaced method body-arg canonical-fallback helper fails on bare count target after rooted borrow
- stdlib namespaced method expression body-arg canonical-fallback helper keeps rooted borrow diagnostic
- array namespaced method expression body-arg helper-returned reference keeps rooted borrow diagnostic
- array namespaced method expression body-arg helper mismatch keeps rooted borrow diagnostic
- map method expression body-arg rejects missing referenced canonical ref helper
- map method expression body-arg referenced wrapper keeps canonical diagnostics
- templated canonical map count wrapper method sugar rejects without explicit alias
- bare map helper statement body arguments require canonical helper resolution
- bare map helper statement body arguments validate through canonical helper target
- bare map helper statement body arguments keep canonical mismatch diagnostics
- wrapper-returned bare map helper statement body arguments fall back to canonical helper target
- wrapper-returned bare map helper statement body arguments keep canonical mismatch diagnostics
- reference-wrapped map helper receiver statement body arguments use canonical ref helper target
- reference-wrapped map helper receiver statement body arguments keep canonical mismatch diagnostics
- map namespaced count method statement body arguments validate through canonical helper
- map namespaced at method statement body arguments validate through canonical helper
- map stdlib call form statement body arguments use canonical helper target

### test_semantics_calls_and_flow_collections_relocation_trivial_vector_elements.cpp

- push accepts non-relocation-trivial vector element types through imported stdlib helpers
- vector constructor rejects non-relocation-trivial vector element types
- push allows relocation-trivial string vector elements
- experimental vector ownership-sensitive helpers accept non-trivial elements
- canonical vector helpers route experimental vector receivers onto stdlib vector storage
- canonical vector pop method requires visible canonical helper
- canonical vector indexed removal helpers require visible canonical helpers
- canonical vector count helper validates with internal vector import
- push on mutable vector field access validates through canonical helper routing
- push rejects retired mutable soa_vector parameter spelling
- collection syntax parity validates for call and method forms
- collection indexing syntax parity keeps integer-index diagnostics
- bare vector push template specialization keeps canonical unknown target without import
- bare vector push arity mismatch keeps canonical unknown target without import
- push call keeps user-defined vector helper precedence
- push method keeps user-defined vector helper precedence
- vector reserve alias keeps rooted unknown target without helper
- vector reserve alias keeps rooted unknown target before effect validation
- reserve on array reports vector binding before effect requirement
- vector reserve alias keeps rooted unknown target before capacity validation
- reserve bool capacity keeps routed unknown target diagnostics
- bare vector reserve template specialization keeps canonical unknown target without import
- bare vector reserve arity mismatch keeps canonical unknown target without import
- bare vector reserve validates through imported stdlib helper
- reserve accepts nested non-relocation-trivial vector element types through imported stdlib helpers
- reserve rejects retired mutable soa_vector parameter spelling
- pop rejects retired soa_vector parameter spelling before helper target
- reserve call keeps user-defined vector helper precedence
- reserve method keeps user-defined vector helper precedence
- capacity method keeps same-path vector helper precedence
- vector pop alias requires same-path helper before mutability
- bare vector pop validates through imported stdlib helper
- pop allows non-drop-trivial vector element types
- pop allows drop-trivial string vector elements

### test_semantics_calls_and_flow_collections_soa_vector_builtins_named_args.cpp

- to_soa rejects named arguments while retired to_aos spelling rejects before named args
- retired soa_vector access forms reject before named arguments
- retired soa_vector index forms reject before integer index checks
- old-surface soa_vector access forms reject retired spelling before indices
- soa_vector conversion and access builtins reject template arguments
- soa_vector canonical get_ref rejects retired spelling before expression-only check
- soa_vector conversion and access builtins reject block arguments
- soa_vector conversion and access builtins enforce argument counts
- soa_vector builtin construction rejects retired spelling
- soa_vector builtin ref local bindings reject retired spelling
- soa_vector helper-return ref/ref_ref local bindings reject non-templated spelling
- soa_vector ref helper binding rejects retired spelling before user helper
- soa_vector builtin ref call argument escapes use pending diagnostic
- soa_vector helper-return ref/ref_ref call argument escapes reject non-templated spelling
- soa_vector builtin ref/ref_ref return escapes reject retired spelling
- soa_vector helper-return ref/ref_ref return escapes reject non-templated spelling
- soa_vector ref helper rejects retired spelling before same-path helper escapes
- soa_vector ref_ref helper rejects retired spelling before same-path helper escapes
- soa_vector helper-return bare ref rejects non-templated spelling before helper escapes
- soa_vector helper-return bare ref_ref rejects non-templated spelling before helper escapes
- soa_vector helper-return bare get rejects non-templated spelling before helper escapes
- soa_vector helper-return bare get_ref rejects non-templated spelling before helper escapes
- soa_vector builtin get_ref rejects non-templated helper-return spelling before auto inference
- soa_vector builtin get rejects non-templated helper-return spelling before field inference
- soa_vector builtin method get_ref rejects non-templated helper-return spelling before field inference
- uppercase SoaVector helper-return count keeps current meta diagnostic
- vector-target count get get_ref and ref keep same-path soa helpers
- vector-target to_aos keeps same-path helper shadow
- raw builtin soa_vector canonical to_aos bindings keep experimental vector contract
- vector-target old-explicit push and reserve keep same-path soa helpers
- vector-target method push and reserve keep same-path soa helpers
- canonical soa helper-return push and reserve keep same-path helper across escapes
- canonical soa method-like helper-return push and reserve keep same-path helper across escapes
- soa_vector builtin field view call argument escapes report escape diagnostics
- soa_vector builtin field view return escapes reject retired spelling
- legacy soa_vector builtin field-view bindings reject retired spelling
- canonical soa field-view bindings track borrow roots
- canonical soa field-view binding blocks structural mutation while live
- legacy soa_vector field-view binding rejects retired spelling before mutation borrow check
- legacy soa_vector field-view bindings reject non-templated helper-return spelling
- soa_vector field-view helper rejects retired spelling before returned field-view arithmetic
- soa_vector get helper call-form rejects retired labeled receiver
- soa_vector get method-form rejects retired spelling before helper preference
- soa_vector get helper call-form rejects retired named-argument helper
- soa_vector ref method-form rejects retired spelling before helper preference
- soa_vector field-view method statement rejects retired helper substrate
- soa_vector field-view call-form statement rejects retired helper substrate
- soa_vector field-view index syntax rejects retired helper substrate
- soa_vector field-view builtin rejects retired spelling before named arguments
- soa_vector field-view call-form rejects retired spelling before user helper fallback
- soa_vector field-view method rejects retired spelling before user helper fallback
- count rejects vector named arguments
- count method validates on array binding
- count method on map binding ignores explicit alias helper without canonical definition
- count method validates on string binding
- count wrapper temporaries infer i32 for chained methods
- wrapper vector count method resolves through imported stdlib helper
- wrapper vector count method requires helper
- count wrapper temporary chained method reports i32 path diagnostics
- access wrapper temporaries infer i32 for chained methods
- namespaced access wrapper temporaries infer i32 for chained methods
- reordered map access wrapper temporaries reject map storage mismatch
- access wrapper temporary chained method reports i32 path diagnostics
- namespaced access wrapper temporary chained method reports vector at mismatch
- slash-method access wrapper temporaries report missing namespaced at target

### test_semantics_calls_and_flow_collections_stdlib_map_constructor_parameter_shapes.cpp

- stdlib map constructors accept canonical map method-call parameters
- stdlib map constructors keep mismatch diagnostics on canonical map method-call parameters
- stdlib map constructors accept inferred canonical map default parameters
- stdlib map constructors keep mismatch diagnostics on inferred canonical map default parameters
- helper-wrapped map constructors accept inferred canonical map default parameters
- helper-wrapped map constructors keep mismatch diagnostics on inferred canonical map default parameters
- helper-wrapped inferred canonical map default parameters validate
- helper-wrapped inferred canonical map default parameters keep mismatch diagnostics
- canonical map helpers accept direct experimental constructor receivers
- canonical map helpers keep mismatch diagnostics on direct constructor receivers
- helper-wrapped inferred experimental result map default parameters validate
- helper-wrapped inferred experimental result map default parameters keep mismatch diagnostics
- helper-wrapped canonical map helpers accept direct experimental constructor receivers
- helper-wrapped canonical map helpers keep mismatch diagnostics on direct constructor receivers
- canonical map methods validate direct constructor receivers with explicit contains
- canonical map methods keep mismatch diagnostics on direct constructor receivers
- helper-wrapped canonical map helpers accept direct constructor receivers
- helper-wrapped canonical map methods keep mismatch diagnostics on direct constructor receivers
- field-bound canonical map helpers accept struct field receivers
- field-bound canonical map wrapper helpers accept struct field receivers
- field-bound canonical map bare count fallback rejects
- field-bound canonical map explicit at body arguments validate
- field-bound canonical map explicit at expression body arguments validate
- field-bound canonical map explicit at expression body arguments keep mismatch diagnostics

### test_semantics_calls_and_flow_collections_stdlib_map_constructor_resolution.cpp

- stdlib namespaced map constructor resolves through imported stdlib helper
- stdlib namespaced map constructor keeps same-path definition before wrapper fallback
- stdlib namespaced map constructor requires imported stdlib helper
- imported stdlib namespaced map helpers accept ordinary named arguments
- canonical namespaced map helpers accept borrowed canonical map receivers
- canonical namespaced map _ref helpers accept borrowed experimental map receivers
- canonical namespaced map access helpers accept experimental map values
- canonical namespaced map helpers keep builtin key diagnostics for borrowed canonical map receivers
- canonical namespaced map _ref helpers include builtin map key rejects for borrowed canonical map receivers
- imported stdlib namespaced map constructor keeps mismatch diagnostics
- stdlib namespaced map count requires an explicit canonical helper definition
- stdlib namespaced map count builtin rejects template args without imported helper
- stdlib namespaced map count builtin rejects template args through compile pipeline
- stdlib namespaced map contains requires imported stdlib helper or explicit definition
- stdlib namespaced map tryAt requires imported stdlib helper or explicit definition
- stdlib namespaced map tryAt does not inherit alias-only helper definition
- stdlib namespaced map access helpers accept imported stdlib wrappers
- stdlib namespaced map access keeps canonical target over alias
- collected diagnostics ignore imported canonical map access helper calls
- collected diagnostics keep unknown target for unsupported canonical map helper calls
- stdlib namespaced map ref helpers accept canonical map references
- stdlib namespaced map helpers accept experimental map value receivers
- stdlib wrapper map helpers accept experimental map value receivers
- canonical stdlib map helpers accept constructor receivers
- retired public mapPair bridge reports unknown target
- stdlib namespaced map constructor accepts explicit experimental map bindings
- stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map bindings
- stdlib namespaced map constructor accepts explicit experimental map returns
- stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map returns
- stdlib namespaced map constructor accepts explicit experimental map parameters
- stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map parameters
- helper-wrapped map constructors reject explicit experimental map parameters

### test_semantics_calls_and_flow_collections_user_array_helper_precedence.cpp

- capacity method keeps user-defined array helper precedence
- capacity call keeps user-defined array helper precedence
- count method keeps user-defined array helper precedence
- count call keeps user-defined array helper precedence
- bare count call rejects user-defined map alias helper precedence
- count method requires canonical map helper even when alias helper exists
- count call keeps user-defined string helper precedence
- count method keeps user-defined string helper precedence
- count method keeps user-defined vector helper precedence
- capacity method keeps user-defined vector helper precedence
- count call keeps user-defined vector helper precedence
- count call keeps templated user-defined vector helper precedence
- capacity call keeps templated user-defined vector helper precedence
- capacity call keeps user-defined vector helper precedence
- bare vector count auto inference resolves through imported stdlib helper
- bare vector count auto inference keeps canonical unknown target
- bare vector capacity auto inference resolves through imported stdlib helper
- bare vector capacity auto inference keeps canonical unknown target
- at call keeps user-defined array helper precedence
- at_unsafe call requires canonical map helper even when alias helper exists
- at method keeps user-defined array helper precedence
- at_unsafe call keeps user-defined array helper precedence
- at_unsafe method keeps user-defined array helper precedence
- at_unsafe method requires canonical map helper even when alias helper exists
- at call requires canonical map helper even when alias helper exists
- bare map at uses canonical helper signature for mismatch diagnostics
- at method requires canonical map helper even when alias helper exists
- at call keeps user-defined vector helper precedence
- at method keeps user-defined vector helper precedence
- at_unsafe call keeps user-defined string helper precedence
- at_unsafe method keeps user-defined string helper precedence
- at_unsafe call keeps user-defined vector helper precedence
- at_unsafe method keeps user-defined vector helper precedence
- at call keeps user-defined string helper precedence
- at method keeps user-defined string helper precedence
- vector push alias requires same-path helper before mutability
- vector push alias requires same-path helper before effects
- push on array reports vector binding before effect requirement
- bare vector push validates through imported stdlib helper

### test_semantics_calls_and_flow_collections_user_named_map_block_args.cpp

- user definition named map accepts block arguments
- collection builtin still rejects named arguments
- builtin at validates on array literal call target
- builtin at on canonical map call target requires imported canonical helper or explicit definition
- builtin at reordered canonical map receiver requires imported canonical helper or explicit definition
- user definition named vector call is not treated as builtin collection target
- user definition named map call is not treated as builtin collection target
- user definition named Map reordered receiver is not treated as builtin experimental map target
- user definition named Map call is not treated as builtin experimental map target

### test_semantics_calls_and_flow_collections_variadic_fileerror_and_file_handles.cpp

- variadic wrapped FileError packs accept canonical named free builtin at receivers
- variadic wrapped FileError packs still reject noncanonical named free builtin at labels
- variadic wrapped File handle packs accept canonical named free builtin at receivers
- variadic wrapped read File handle packs accept canonical named free builtin at read_byte try
- canonical map helper bodies reject conflicting imported count alias
- canonical map helper bodies keep import conflict ahead of missing canonical count fallback
- canonical map helper bodies keep import conflict ahead of missing canonical access fallback
- bare map at call requires imported canonical helper or explicit definition
- bare map at_unsafe auto inference requires imported canonical helper or explicit definition
- map namespaced count method compatibility alias resolves through canonical helper
- map stdlib namespaced count method resolves through canonical helper
- map stdlib namespaced count method supports auto inference
- bare map count method requires imported canonical helper or explicit definition
- bare map count method auto inference requires imported canonical helper or explicit definition
- canonical map count call does not inherit alias-only helper definition
- bare map contains method requires imported canonical helper or explicit definition
- canonical map contains call does not inherit alias-only helper definition
- bare map tryAt method auto inference keeps deterministic unknown method diagnostics
- bare map access methods require imported canonical helpers or explicit definitions
- bare map access method auto inference keeps deterministic unknown call target diagnostics
- map namespaced at method now validates through slash-path routing
- map namespaced at_unsafe method auto inference now validates through slash-path routing
- map namespaced contains method now validates through slash-path routing
- map stdlib namespaced contains method now validates through slash-path routing
- map namespaced tryAt method validates through slash-path routing
- map stdlib namespaced tryAt method validates through slash-path routing
- map stdlib namespaced at_unsafe method now validates through slash-path routing
- stdlib canonical map contains and tryAt helpers resolve in method-call sugar
- stdlib canonical map helpers resolve in method-call sugar
- stdlib canonical map insert resolves in method-call sugar
- stdlib canonical map insert resolves in direct-call form
- stdlib canonical map insert resolves on non-local field and borrowed receivers
- stdlib canonical map insert resolves on nested non-local and helper-return borrowed receivers
- stdlib canonical map insert resolves on helper-return borrowed method receivers
- stdlib canonical map helper resolves method-call sugar for slash return type

### test_semantics_calls_and_flow_collections_vector_alias_unknown_expected_forwarding.cpp

- vector namespaced unknown expected primitive call return keeps builtin count diagnostics
- vector namespaced alias rejects compatibility template forwarding when unknown expected meets primitive binding
- vector namespaced unknown expected primitive binding keeps alias diagnostics
- vector namespaced alias rejects compatibility template forwarding when unknown expected meets vector envelope binding
- vector namespaced unknown expected vector envelope mismatch keeps compatibility diagnostics
- vector namespaced count alias arity fallback keeps compatibility diagnostics
- vector namespaced non-bool mismatch fallback keeps alias diagnostics
- vector namespaced bool mismatch fallback keeps builtin count diagnostics
- vector namespaced count alias named arguments reject compatibility template forwarding
- vector namespaced count alias named arguments keep alias diagnostics
- vector namespaced capacity alias arity mismatch rejects compatibility template forwarding
- vector namespaced capacity alias type mismatch rejects compatibility template forwarding
- vector namespaced capacity alias named arguments reject compatibility template forwarding
- vector namespaced at alias arity mismatch rejects compatibility template forwarding
- vector namespaced at alias type mismatch rejects compatibility template forwarding
- vector namespaced at alias named arguments reject compatibility template forwarding
- vector namespaced at_unsafe alias arity mismatch rejects compatibility template forwarding
- vector namespaced at_unsafe alias type mismatch rejects compatibility template forwarding
- vector namespaced at_unsafe alias named arguments reject compatibility template forwarding
- array namespaced templated count alias is rejected
- array namespaced templated count alias missing marker is rejected
- stdlib namespaced templated vector count call reports routed alias template diagnostics
- stdlib namespaced vector count call accepts compatibility alias fallback
- stdlib namespaced templated vector count arity keeps builtin template-argument diagnostics
- vector namespaced templated alias call keeps builtin template diagnostics
- vector method templated call reports canonical argument mismatch past non-templated helper
- wrapper temporary templated vector method call rejects compatibility template forwarding
- vector method bare count rejects canonical-only helper path
- vector method explicit alias namespace is rejected when only canonical helper exists
- vector method explicit count alias namespace resolves when alias helper exists
- vector method explicit count alias namespace keeps auto inference on same-path helper
- vector method explicit count alias namespace auto inference still rejects canonical-only helper path
- vector method bare capacity rejects canonical-only helper path
- vector method explicit capacity alias namespace is rejected when only canonical helper exists
- vector method explicit capacity alias namespace resolves when alias helper exists
- vector method explicit capacity alias namespace keeps auto inference on same-path helper
- vector method explicit capacity alias namespace auto inference still rejects canonical-only helper path
- vector method bare at rejects canonical-only helper path
- vector method explicit at alias namespace is rejected when only canonical helper exists
- vector method explicit at alias namespace uses alias helper when alias helper exists
- vector method explicit at alias namespace keeps auto inference on same-path helper
- vector method explicit at alias namespace auto inference still rejects canonical-only helper path
- vector method bare at_unsafe rejects canonical-only helper path
- vector method explicit at_unsafe alias namespace is rejected when only canonical helper exists
- vector method explicit at_unsafe alias namespace resolves same-path helper when alias helper exists
- vector method explicit at_unsafe alias namespace keeps auto inference on same-path helper
- vector method explicit at_unsafe alias namespace auto inference still rejects canonical-only helper path
- vector method at uses alias helper signature when alias helper has string index
- vector method at_unsafe uses alias helper signature when alias helper has bool index
- vector method count rejects stdlib /std/collections/vector/count alias-only helper
- vector method capacity rejects stdlib /std/collections/vector/capacity alias-only helper

### test_semantics_calls_and_flow_collections_vector_capacity_alias_named_args.cpp

- vector namespaced capacity wrapper vector target without helper reports unknown target
- vector namespaced capacity builtin vector target without helper reports unknown target
- vector namespaced capacity stays rejected with canonical import
- vector namespaced capacity rejects named arguments as builtin alias
- array namespaced vector capacity alias rejects named arguments with unknown-target diagnostics
- array namespaced vector capacity alias call is rejected
- array namespaced vector at alias wrapper call is rejected
- array namespaced vector at_unsafe alias wrapper call is rejected
- namespaced vector count and capacity allow named args for user helper receiver
- stdlib namespaced vector access helper accepts named arguments through imported stdlib helper
- stdlib namespaced vector access helper requires imported stdlib helper
- stdlib namespaced vector at_unsafe helper requires imported stdlib helper
- vector namespaced access helper rejects named arguments
- vector namespaced access helper named arguments stay rejected with canonical helper in scope
- vector namespaced at_unsafe helper rejects named arguments
- stdlib namespaced vector capacity keeps non-vector target diagnostics
- vector namespaced capacity keeps non-vector target diagnostics
- vector namespaced capacity array target without helper reports unknown target
- rooted vector helper with named arguments rejects with unknown-target diagnostics
- stdlib namespaced vector helper with named arguments is statement-only in expressions
- array namespaced vector helper with named arguments is statement-only in expressions
- user definition named push is not treated as builtin vector helper
- struct push helper statement call is not treated as builtin vector helper
- struct push helper expression call is not treated as builtin vector helper
- struct push helper assign to immutable field is rejected
- struct push helper wrapper temporary statement call is not treated as builtin vector helper
- struct push helper wrapper temporary expression call is not treated as builtin vector helper
- user definition named push can be used in expressions
- vector helper call-form expression rejects user-defined method target
- vector helper call-form expression user shadow rejects positional reordered arguments
- vector helper call-form expression user shadow rejects bool positional reordered arguments
- vector helper call-form expression stays statement-only without user helper
- vector helper expression skips temp-leading positional receiver probing
- vector helper call-form expression user shadow rejects named arguments
- vector helper call-form expression user shadow rejects duplicate named receiver
- vector helper call-form expression rejects labeled named receiver
- vector helper call-form expression rejects auto binding from labeled receiver helper
- vector stdlib namespaced helper auto inference follows alias precedence
- vector stdlib namespaced helper auto inference keeps explicit parameter order
- vector stdlib namespaced helper keeps explicit parameter diagnostics over return mismatch

### test_semantics_calls_and_flow_collections_vector_helper_call_form_named_receivers.cpp

- vector helper call-form expression builtin stays statement-only with named arguments
- vector helper statement call-form user shadow accepts named arguments
- vector helper statement prefers labeled named receiver
- vector helper statement labeled receiver does not fall back to non-receiver label
- vector helper statement skips temp-leading positional receiver probing
- vector helper statement validates on variadic vector pack receivers
- vector helper statement validates on dereferenced variadic vector pack receivers
- vector helper statement user shadow resolves on variadic vector pack receivers
- vector at call-form helper shadow accepts reordered named arguments
- at call-form helper shadow prefers labeled named receiver
- at call-form labeled receiver does not fall back to non-receiver label
- vector at call-form helper shadow accepts positional reordered arguments
- map at call-form helper shadow accepts string positional reordered arguments
- map at call-form helper shadow prefers later map receiver over leading string
- map at_unsafe call-form helper shadow accepts string positional reordered arguments
- vector at_unsafe call-form helper shadow accepts reordered named arguments
- vector at call-form helper shadow skips temp-leading positional probing
- user definition named push with positional args is not treated as builtin
- user definition named push accepts named arguments
- vector method helper in expressions requires imported stdlib helper or explicit definition
- bare vector clear named args require imported stdlib helper
- user definition named count accepts named arguments
- user definition named capacity accepts named arguments
- capacity builtin still rejects named arguments
- user definition named at accepts named arguments
- user definition named at_unsafe accepts named arguments
- array access builtin still rejects named arguments
- user definition named vector accepts named arguments
- user definition named map accepts named arguments
- user definition named array accepts named arguments
- user definition named vector accepts block arguments
- user definition named array accepts block arguments

### test_semantics_calls_and_flow_collections_vector_helper_reordered_expression.cpp

- vector namespaced helper reordered expression stays statement-only
- stdlib namespaced helper reordered expression rejects compatibility receiver fallback
- stdlib namespaced helper reordered expression keeps compatibility reject diagnostics
- vector namespaced helper reordered statement keeps vector-binding diagnostics
- stdlib namespaced helper reordered statement rejects compatibility receiver fallback
- stdlib namespaced helper reordered statement keeps compatibility reject diagnostics
- explicit canonical push on templated canonical constructor helper keeps same-path helper resolution
- bare push on templated canonical constructor helper keeps vector-binding diagnostics
- explicit push on templated experimental constructor helper keeps helper return type
- bare push on templated experimental constructor helper keeps vector-binding diagnostics
- stdlib namespaced vector constructor resolves through imported stdlib helper
- stdlib namespaced vector constructor accepts named arguments through imported stdlib helper
- stdlib namespaced vector constructor supports named-argument temporary receivers
- rooted vector constructor alias auto binding is rejected even with stdlib import
- rooted vector constructor alias named-argument auto binding is rejected
- stdlib namespaced vector constructor accepts explicit builtin vector binding
- stdlib namespaced vector constructor accepts named-argument explicit builtin vector binding
- stdlib namespaced vector constructor requires imported stdlib helper
- array namespaced vector constructor alias is rejected
- array namespaced vector constructor alias named arguments keep unknown-call-target diagnostics
- stdlib namespaced vector access and count helpers are builtin-alias validated
- stdlib namespaced vector helpers accept explicit Vector bindings
- stdlib wrapper vector helpers accept explicit Vector bindings
- stdlib wrapper vector helpers reject explicit Vector type mismatch
- stdlib wrapper vector constructors accept explicit Vector destinations
- stdlib wrapper vector constructors accept public vector destinations
- stdlib wrapper vector constructors keep mismatch diagnostics on public vector destinations
- stdlib wrapper vector constructors keep mismatch diagnostics on explicit Vector destinations
- stdlib wrapper vector constructors infer experimental auto locals and auto returns
- helper-wrapped vector constructors infer experimental auto locals
- helper-wrapped vector constructor auto inference keeps template conflict diagnostics
- implicit vector constructor auto inference keeps template conflict diagnostics
- canonical vector helpers accept direct constructor receivers
- canonical vector helpers keep mismatch diagnostics on direct constructor receivers
- experimental vector methods accept direct constructor receivers
- experimental vector methods keep mismatch diagnostics on direct constructor receivers
- helper-wrapped canonical vector helpers accept direct constructor receivers
- helper-wrapped canonical vector helpers keep mismatch diagnostics on direct constructor receivers
- helper-wrapped experimental vector methods accept direct constructor receivers
- helper-wrapped experimental vector methods keep mismatch diagnostics on direct constructor receivers

### test_semantics_calls_and_flow_collections_vector_method_alias_struct_diagnostics.cpp

- vector unsafe method alias access field expression keeps removed alias diagnostics
- vector unsafe same-path method helper keeps builtin field diagnostics
- vector method access keeps builtin field diagnostics over struct helpers
- vector method access reports current receiver diagnostics over canonical helper
- vector unsafe method access keeps builtin field diagnostics over canonical helper
- map method access keeps canonical struct-return forwarding
- map method access field expression keeps canonical struct-return forwarding
- map method access ignores rooted alias struct-return helper
- map method access reports canonical builtin result type over alias helper
- map method alias access accepts matching receiver during inference
- map method alias access rejects missing receiver method during inference
- wrapper-returned map method alias access keeps explicit helper diagnostics during inference
- wrapper-returned map method alias access keeps primitive argument diagnostics during inference
- std-namespaced vector method alias access keeps helper receiver diagnostics during inference
- std-namespaced vector method alias access keeps helper missing-method diagnostics during inference
- vector method alias access keeps removed-alias diagnostics
- vector alias access auto wrapper keeps primitive receiver diagnostics
- vector alias access auto wrapper keeps primitive argument diagnostics
- templated stdlib canonical vector helpers reject method-call sugar template args on count
- templated slash-path vector helper methods stay on unknown method diagnostics
- templated slash-path vector helper arity failures stay on unknown method diagnostics
- templated stdlib canonical vector helper reports current argument mismatch diagnostics

### test_semantics_calls_and_flow_collections_vector_mutator_named_args.cpp

- push and reserve named args validate through imported stdlib helpers
- indexed removal and discard named args still reject builtin call syntax
- bare vector mutator named args require imported stdlib helpers
- vector helper expressions with named arguments stay statement-only
- vector helper method expressions with named arguments require helper resolution
- namespaced vector helper statement form requires same-path helper
- namespaced vector helper expression form stays statement-only
- stdlib namespaced vector helper accepts statement form
- namespaced vector mutator statement helpers are accepted
- bare vector mutator statement helper on builtin vector receiver requires same-path helper
- bare vector mutator method on builtin vector receiver requires same-path helper
- vector namespaced mutator statement helper on builtin vector receiver requires same-path helper
- vector namespaced mutator statement helper on builtin vector receiver ignores canonical-only helper
- stdlib namespaced vector mutator statement helper on builtin vector receiver ignores alias-only helper
- vector namespaced mutator slash method on builtin vector receiver requires same-path helper
- vector namespaced mutator slash method on builtin vector receiver ignores canonical-only helper
- lambda vector namespaced mutator slash method ignores canonical-only helper
- vector namespaced mutator slash method on builtin vector receiver accepts same-path helper
- namespaced vector mutator expression helpers stay statement-only
- stdlib namespaced vector helper statement rejects compatibility helper definition fallback
- stdlib namespaced vector push accepts named arguments through imported stdlib helper
- stdlib namespaced vector pop accepts named arguments through imported stdlib helper
- stdlib namespaced vector reserve accepts named arguments through imported stdlib helper
- stdlib namespaced vector push requires imported stdlib helper
- stdlib namespaced vector pop requires imported stdlib helper
- stdlib namespaced vector reserve requires imported stdlib helper
- stdlib namespaced vector clear accepts named arguments through imported stdlib helper
- stdlib namespaced vector remove_at accepts named arguments through imported stdlib helper
- stdlib namespaced vector remove_swap accepts named arguments through imported stdlib helper
- stdlib namespaced vector clear requires imported stdlib helper
- stdlib namespaced vector remove_at requires imported stdlib helper
- stdlib namespaced vector remove_swap requires imported stdlib helper
- stdlib namespaced vector clear method requires imported stdlib helper
- stdlib namespaced vector push method requires imported stdlib helper
- stdlib namespaced vector pop method requires imported stdlib helper
- stdlib namespaced vector reserve method requires imported stdlib helper
- stdlib namespaced vector remove_at method requires imported stdlib helper
- stdlib namespaced vector remove_swap method requires imported stdlib helper
- stdlib namespaced vector push expression requires imported stdlib helper
- stdlib namespaced vector push named args require imported stdlib helper in expressions
- array namespaced vector mutator alias named args stay statement-only in expressions
- namespaced vector helper duplicate named args stay statement-only in expressions
- array namespaced vector mutator alias duplicate named args stay statement-only in expressions
- array namespaced vector mutator alias statement is rejected
- vector mutator alias block args require same-path helpers
- stdlib namespaced vector helper duplicate named args stay statement-only in expressions
- vector helper method expression resolves through canonical stdlib helper
- vector helper method expression reports canonical helper argument mismatch

### test_semantics_calls_and_flow_collections_vector_namespaced_alias_calls.cpp

- vector namespaced call aliases validate count and access builtins
- rooted vector helper calls stay rejected with canonical import
- rooted vector count call stays rejected with canonical import
- rooted vector slash methods stay rejected with canonical import
- vector namespaced count alias keeps canonical helper diagnostics
- vector namespaced count alias falls back to canonical helper return
- vector namespaced call alias with explicit template args keeps builtin diagnostics
- vector namespaced count alias arity mismatch rejects compatibility template forwarding
- vector namespaced alias rejects compatibility template forwarding on bool type mismatch with same arity
- vector namespaced alias rejects compatibility template forwarding on non-bool type mismatch with same arity
- vector namespaced alias rejects compatibility template forwarding on struct type mismatch with same arity
- vector namespaced struct mismatch fallback keeps compatibility diagnostics
- vector namespaced alias rejects compatibility template forwarding for struct mismatch from constructor temporary
- vector namespaced constructor temporary mismatch keeps compatibility diagnostics
- vector namespaced alias rejects compatibility template forwarding for struct mismatch from method-call temporary
- vector namespaced method-call temporary mismatch keeps compatibility diagnostics
- vector namespaced alias rejects compatibility template forwarding for struct mismatch from chained method-call temporary
- vector namespaced chained method-call mismatch keeps compatibility diagnostics
- vector namespaced alias rejects compatibility template forwarding on array envelope element mismatch
- vector namespaced array envelope mismatch keeps compatibility diagnostics
- vector namespaced alias rejects compatibility template forwarding on map envelope mismatch
- vector namespaced map envelope mismatch keeps compatibility diagnostics
- vector namespaced alias rejects compatibility template forwarding on map envelope mismatch from call return
- vector namespaced map call-return mismatch keeps compatibility diagnostics
- vector namespaced alias rejects compatibility template forwarding on primitive mismatch from call return
- vector namespaced primitive call-return mismatch keeps compatibility diagnostics
- vector namespaced alias rejects compatibility template forwarding when unknown expected meets primitive call return

### test_semantics_calls_and_flow_collections_vector_stdlib_push_auto_inference.cpp

- vector stdlib namespaced push auto inference uses canonical helper definition
- vector statement push prefers imported canonical helper over same-path shadow
- vector statement push rejects same-path shadow without canonical import
- vector statement push rejects reordered positional call through canonical import
- vector namespaced count-capacity call-form without helpers fails on count first
- vector namespaced count auto inference builtin vector target without helper reports unknown target
- vector namespaced capacity auto inference builtin vector target without helper reports unknown target
- vector namespaced count-capacity auto inference accepts same-path helpers on builtin vector target
- vector namespaced count auto inference rejects map target without helper
- vector namespaced capacity auto inference rejects map target without helper
- vector namespaced count auto inference rejects string target without helper
- vector namespaced count auto inference rejects array target without helper
- vector namespaced count auto inference accepts same-path helpers on non-vector targets
- vector stdlib namespaced count helper auto inference keeps canonical precedence
- vector stdlib namespaced count helper auto inference keeps inferred return mismatch diagnostics
- vector stdlib namespaced count helper auto inference falls back to canonical helper return
- vector stdlib namespaced count auto inference rejects compatibility alias precedence for non-builtin arity
- vector stdlib namespaced count auto inference keeps non-builtin arity mismatch diagnostics
- vector stdlib namespaced count auto inference non-builtin arity falls back to canonical helper return
- vector stdlib namespaced count expression keeps canonical precedence
- vector stdlib namespaced count expression keeps return mismatch diagnostics
- vector stdlib namespaced count expression falls back to canonical helper return
- vector stdlib namespaced capacity expression keeps canonical precedence
- vector stdlib namespaced capacity expression keeps return mismatch diagnostics
- vector stdlib namespaced capacity expression falls back to canonical helper return
- vector stdlib namespaced count expression rejects compatibility alias precedence for non-builtin arity
- vector stdlib namespaced count expression keeps non-builtin arity mismatch diagnostics
- vector stdlib namespaced count expression non-builtin arity falls back to canonical helper return
- vector stdlib namespaced count expression rejects non-builtin compatibility alias fallback
- vector stdlib namespaced count expression compatibility alias fallback reports unknown target
- vector stdlib namespaced count auto inference rejects non-builtin compatibility arity fallback
- vector namespaced count non-builtin arity rejects array helper fallback
- vector namespaced count non-builtin arity diagnostics report builtin mismatch
- vector namespaced count auto inference non-builtin arity rejects array helper fallback
- map stdlib namespaced count expression keeps canonical helper return precedence
- map stdlib namespaced count expression succeeds when canonical helper matches return type
- map stdlib namespaced count expression non-builtin arity falls back to canonical helper return
- map stdlib namespaced count expression does not fall back to map alias helper
- map stdlib namespaced count expression keeps canonical diagnostics when alias helper exists
- map compatibility count call keeps explicit alias precedence with canonical templated helper present
- map compatibility count call keeps alias mismatch diagnostics with canonical templated helper present
- map stdlib namespaced count expression ignores templated alias helper fallback
- map compatibility count call does not inherit canonical templated helper
- map compatibility explicit-template count call keeps alias precedence with canonical templated helper

### test_semantics_calls_and_flow_collections_wrapper_returned_map_count_expression.cpp

- wrapper-returned slash-method map access count validates string result
- map stdlib namespaced count expression ignores templated alias fallback
- map stdlib namespaced count auto inference keeps canonical helper return precedence
- map stdlib namespaced count auto inference keeps canonical unknown target
- map stdlib namespaced count auto inference succeeds when canonical helper matches return type
- map stdlib namespaced count statement keeps canonical unknown target
- map stdlib namespaced count auto inference non-builtin arity falls back to canonical helper return
- vector stdlib namespaced capacity expression keeps canonical precedence
- vector stdlib namespaced capacity expression keeps return mismatch diagnostics
- vector stdlib namespaced capacity expression uses canonical helper definition
- vector stdlib namespaced capacity expression rejects compatibility alias precedence for non-builtin arity
- vector stdlib namespaced capacity expression keeps non-builtin arity mismatch diagnostics
- vector stdlib namespaced capacity expression uses canonical helper definition for non-builtin arity
- vector stdlib namespaced access expression follows alias precedence
- vector stdlib namespaced access expression reports alias return mismatch
- vector stdlib namespaced access expression uses canonical helper definition
- vector stdlib namespaced access expression uses alias precedence for non-builtin arity
- vector stdlib namespaced access expression reports alias mismatch for non-builtin arity
- vector stdlib namespaced access expression uses canonical helper definition for non-builtin arity
- vector namespaced capacity auto inference keeps non-vector target diagnostics
- access helper call-form expression infers auto binding from labeled receiver helper
- vector stdlib namespaced access helper auto inference keeps canonical precedence
- vector stdlib namespaced access helper auto inference reports alias mismatch
- vector stdlib namespaced access helper auto inference uses canonical helper definition
- map stdlib namespaced access helper auto inference keeps canonical precedence
- map stdlib namespaced access helper auto inference keeps canonical return
- map stdlib namespaced access helper auto inference falls back to canonical helper return
- stdlib namespaced map at requires imported stdlib helper or explicit definition
- stdlib namespaced map at does not inherit alias-only helper definition
- stdlib namespaced map at_unsafe requires imported stdlib helper or explicit definition
- stdlib namespaced map at_unsafe does not inherit alias-only helper definition
- soa access helper call-form expression infers auto binding from labeled receiver helper

### test_semantics_calls_and_flow_collections_wrapper_returned_map_method_resolution.cpp

- wrapper-returned canonical map method ignores alias helper when canonical helper is absent
- wrapper-returned canonical map method requires helper definition
- stdlib canonical map slash return type keeps canonical method diagnostics
- stdlib canonical map count method auto inference falls back to canonical helper return
- stdlib canonical map count method auto inference keeps return mismatch diagnostics
- stdlib canonical map count method auto inference keeps canonical precedence over alias helper
- stdlib canonical map count method auto inference keeps canonical mismatch diagnostics over alias helper
- stdlib canonical map count call auto inference keeps canonical precedence over alias helper
- stdlib canonical map count call auto inference keeps canonical mismatch diagnostics over alias helper
- stdlib canonical map access count shadow keeps canonical precedence over alias helper
- stdlib canonical map access count shadow currently validates mixed canonical and alias returns
- rejects stdlib canonical vector helper method-precedence forwarding in method-call sugar
- array namespaced vector helper alias keeps unknown-method diagnostics for auto inference
- array namespaced vector helper alias keeps unknown-method diagnostics for typed inference
- vector namespaced helper alias keeps unknown-method auto-inference diagnostics
- vector namespaced capacity alias keeps unknown-method auto-inference diagnostics
- array namespaced vector capacity alias keeps unknown-method diagnostics for auto inference
- stdlib namespaced vector helper alias uses same-path helper auto inference
- stdlib namespaced vector access alias uses same-path helper auto inference
- stdlib namespaced vector access alias method-call inference keeps return mismatch diagnostics
- stdlib namespaced vector helper alias method-call inference keeps return mismatch diagnostics
- stdlib namespaced vector capacity alias uses same-path helper auto inference
- stdlib namespaced vector capacity alias method-call inference keeps return mismatch diagnostics
- stdlib namespaced vector access unsafe alias uses same-path helper auto inference
- stdlib namespaced vector access unsafe alias method-call inference keeps return mismatch diagnostics
- stdlib namespaced vector access slash method uses imported helper on vector receiver
- stdlib namespaced vector access slash method uses imported helper diagnostics
- stdlib namespaced vector access unsafe slash method uses imported helper diagnostics
- stdlib namespaced vector count method local same-path overload set rejects duplicate definitions
- stdlib namespaced vector count method rejects map receiver without helper
- stdlib namespaced vector count method rejects array receiver without helper
- stdlib namespaced vector count method rejects string receiver without helper
- stdlib namespaced vector count method keeps local string same-path helper
- stdlib namespaced vector count method keeps local array same-path helper
- stdlib namespaced vector count method keeps local map same-path helper
- stdlib namespaced vector count method rejects incompatible local same-path helper
- vector namespaced count method rejects local string receiver without helper
- vector namespaced count method rejects local array receiver without helper
- vector namespaced count method rejects local map receiver without helper
- vector namespaced count method rejects local string same-path helper
- vector namespaced count method rejects local array same-path helper
- vector namespaced count method rejects local map same-path helper
- stdlib namespaced vector count method rejects wrapper map receiver without helper
- stdlib namespaced vector count method rejects wrapper map same-path helper
- stdlib namespaced vector count method keeps wrapper array same-path helper
- stdlib namespaced vector count method keeps wrapper string same-path helper
- stdlib namespaced vector count method rejects wrapper array receiver without helper
- stdlib namespaced vector count method rejects wrapper string receiver without helper
- vector namespaced count method on builtin vector receiver requires same-path helper
- stdlib namespaced vector count method on builtin vector receiver rejects rooted helper fallback
- vector namespaced count slash method wrapper vector chain keeps unknown-method diagnostics
- bare vector capacity method on builtin vector receiver validates without helper
- stdlib namespaced vector capacity method local same-path overload set rejects duplicate definitions
- stdlib namespaced vector capacity method rejects map receiver without helper
- stdlib namespaced vector capacity method rejects array receiver without helper
- stdlib namespaced vector capacity method rejects string receiver without helper
- stdlib namespaced vector capacity method rejects local string same-path helper
- stdlib namespaced vector capacity method rejects local array same-path helper
- stdlib namespaced vector capacity method rejects local map same-path helper
- vector namespaced capacity method rejects local string receiver without helper
- vector namespaced capacity method rejects local array receiver without helper
- vector namespaced capacity method on builtin vector receiver requires same-path helper
- stdlib namespaced vector capacity method on builtin vector receiver rejects rooted helper fallback
- stdlib namespaced vector capacity method rejects wrapper map receiver without helper
- stdlib namespaced vector capacity method rejects wrapper map same-path helper
- stdlib namespaced vector capacity method rejects wrapper array same-path helper
- stdlib namespaced vector capacity method rejects wrapper string same-path helper
- vector namespaced capacity method rejects wrapper map receiver without helper
- vector namespaced capacity slash method wrapper vector chain keeps unknown-method diagnostics
- array namespaced slash method spelling rejects statement body arguments
- stdlib namespaced vector helper alias rejects statement body arguments

### test_semantics_calls_and_flow_collections_wrapper_returned_map_string_branch_paths.cpp

- wrapper-returned referenced canonical map keeps if string branch compatibility
- explicit canonical map parameter keeps pathspace string diagnostics
- canonical map reference parameter keeps pathspace string diagnostics
- wrapper-returned canonical map keeps pathspace string diagnostics
- wrapper-returned referenced canonical map keeps pathspace string diagnostics
- canonical map reference parameter keeps if string branch diagnostics
- wrapper-returned canonical map keeps if string branch diagnostics
- wrapper-returned referenced canonical map keeps if string branch diagnostics
- wrapper-returned referenced canonical map keeps builtin key diagnostics
- wrapper-returned canonical map access count call auto inference keeps string helper shadow
- wrapper-returned canonical map method access count keeps builtin string helper shadow
- wrapper-returned direct canonical map access count keeps primitive diagnostics
- field-bound map access count keeps builtin string fallback
- wrapper-returned canonical map method access count keeps string receiver typing
- canonical vector access count call keeps builtin string helper shadow
- canonical vector unsafe access count call keeps primitive diagnostics
- canonical vector method access count keeps builtin string fallback
- canonical vector unsafe method access count keeps primitive receiver diagnostics
- slash-method vector access count keeps builtin string fallback
- slash-method vector access count keeps primitive diagnostics
- wrapper-returned vector access count keeps builtin string helper shadow
- wrapper-returned vector access count keeps primitive diagnostics
- wrapper-returned canonical map access count call auto inference keeps string helper mismatch diagnostics
- wrapper-returned referenced canonical map access count call auto inference keeps string helper shadow
- imported wrapper-returned canonical map reference access count keeps string receiver typing
- non-imported wrapper-returned canonical map reference access rejects missing at_ref
- wrapper-returned referenced canonical map access count call auto inference keeps string helper mismatch diagnostics

### test_semantics_calls_and_flow_collections_wrapper_temporary_access_resolution.cpp

- slash-method access wrapper temporary chained method reports i32 path diagnostics
- map wrapper temporary count call validates target classification
- map wrapper temporary count method validates target classification
- stdlib namespaced vector count validates on wrapper temporary vector target
- stdlib namespaced vector count wrapper temporary vector target requires helper
- stdlib namespaced vector capacity wrapper temporary vector target requires helper
- stdlib namespaced vector count rejects wrapper temporary map target
- wrapper temporary canonical vector count slash-method rejects map receiver
- wrapper temporary canonical vector capacity slash-method rejects map receiver
- map wrapper temporary access call validates map target classification
- map wrapper temporary unsafe access validates direct stdlib helper
- map wrapper temporary access keeps canonical key diagnostics
- map wrapper temporary public helper calls validate target classification
- declared canonical map access positional reorder keeps key diagnostics
- map wrapper temporary capacity reports vector target diagnostic
- map wrapper temporary capacity method reports vector target diagnostic
- count preserves missing receiver call-target diagnostics
- capacity preserves missing receiver call-target diagnostics
- at preserves missing receiver call-target diagnostics
- vector access methods resolve through imported stdlib helper
- wrapper vector access methods resolve through imported stdlib helper
- vector access methods keep imported helper argument diagnostics
- bare vector capacity call resolves through imported stdlib helper
- bare vector capacity call requires imported stdlib helper or explicit definition
- vector capacity compatibility alias rejects template arguments
- vector capacity compatibility alias keeps rooted block-arg target
- vector capacity compatibility alias rejects wrong argument count
- capacity method on vector binding requires imported stdlib helper
- capacity wrapper temporaries keep current chained method failure boundary
- wrapper vector capacity method keeps current imported compile boundary
- wrapper vector capacity method keeps unknown method without helper
- capacity method rejects extra arguments with rooted mismatch
- capacity rejects non-vector target
- capacity method rejects array target
- count method keeps unknown method on non-collection receiver
- count method keeps unknown method on non-collection wrapper temporary
- count call keeps unknown method on non-collection receiver
- count call keeps unknown method on non-collection wrapper temporary
- capacity method keeps unknown method on non-collection receiver
- capacity method keeps unknown method on non-collection wrapper temporary
- capacity call keeps unknown method on non-collection receiver
- capacity call keeps unknown method on non-collection wrapper temporary
- at call keeps unknown method on non-collection receiver
- at call keeps unknown method on non-collection wrapper temporary
- at_unsafe call keeps unknown method on non-collection wrapper temporary
- push call keeps unknown method on non-collection receiver
- reserve method keeps unknown method on wrapper temporary
- at method validates on array binding
- at_unsafe call validates on map binding

### test_semantics_calls_and_flow_collections_wrapper_temporary_templated_vector_methods.cpp

- wrapper temporary templated vector method rejects canonical-only helper path
- vector namespaced alias keeps builtin count diagnostics when only canonical templated helper exists
- vector namespaced helper bundle without aliases fails on count first
- stdlib namespaced vector count rejects template arguments as builtin alias
- vector namespaced capacity rejects template arguments as builtin alias
- stdlib namespaced vector count accepts named arguments through imported stdlib helper
- stdlib namespaced vector count requires imported stdlib helper or explicit definition
- vector namespaced count and stdlib namespaced capacity accept same-path helpers on builtin vector target
- vector namespaced count builtin vector target without helper reports unknown target
- stdlib namespaced vector count map target without helper reports unknown target
- stdlib namespaced vector count accepts same-path helper on map target
- vector namespaced count accepts same-path helper on map target
- vector namespaced count map target without helper reports unknown target
- stdlib namespaced vector count accepts same-path helper on string target
- stdlib namespaced vector count string target without helper reports unknown target
- vector namespaced count accepts same-path helper on string target
- vector namespaced count string target without helper reports unknown target
- stdlib namespaced vector count accepts same-path helper on array target
- stdlib namespaced vector count array target without helper reports unknown target
- vector namespaced count accepts same-path helper on array target
- vector namespaced count array target without helper reports unknown target
- vector namespaced count slash method on string target keeps string receiver diagnostic with same-path helper
- vector namespaced count slash method on map target keeps map receiver diagnostic with same-path helper
- vector namespaced count slash method on map target without helper reports map receiver method
- vector namespaced count slash method on string target without helper reports string receiver method
- vector namespaced count slash method on array target keeps array receiver diagnostic with same-path helper
- vector namespaced count slash method on array target without helper reports array receiver method
- vector namespaced capacity slash method on string target keeps string receiver diagnostic with same-path helper
- vector namespaced capacity slash method on map target keeps map receiver diagnostic with same-path helper
- vector namespaced capacity slash method on array target keeps array receiver diagnostic with same-path helper
- vector namespaced capacity slash method on map target without helper reports map receiver method
- vector namespaced capacity slash method on string target without helper reports string receiver method
- vector namespaced capacity slash method on array target without helper reports array receiver method
- vector namespaced access slash method on vector target without alias helper reports alias access method
- vector namespaced access slash method on vector target without canonical helper reports canonical access method
- bare vector access method vector target without helper reports unknown method
- bare vector access method wrapper target without helper reports unknown method
- array compatibility access slash method vector target without helper reports unknown method
- array compatibility access slash method vector chain stops before receiver typing
- array compatibility access slash method wrapper vector chain stops before receiver typing
- vector namespaced access slash method on array target without alias helper reports alias access method
- vector namespaced access slash method on string target without canonical helper reports canonical access method
- vector namespaced access slash method on string target without alias helper reports alias access method
- vector namespaced count accepts same-path helper on wrapper vector target
- array namespaced count accepts same-path helper on vector target
- array namespaced count accepts same-path helper on wrapper vector target
- array namespaced capacity accepts same-path helper on vector target
- array namespaced capacity accepts same-path helper on wrapper vector target
- array namespaced at accepts same-path helper on vector target
- array namespaced at accepts same-path helper on wrapper vector target
- array namespaced at_unsafe accepts same-path helper on vector target
- array namespaced at_unsafe accepts same-path helper on wrapper vector target
- array namespaced slash-method helper bundle on vector receivers fails on count first
- vector namespaced count wrapper vector target without helper reports unknown target
- stdlib namespaced vector count wrapper vector chain without helper reports unknown target
- stdlib namespaced vector count wrapper vector chain accepts same-path helper
- array namespaced count wrapper vector target without helper reports unknown target
- vector namespaced count rejects named arguments as builtin alias
- array namespaced vector count call rejects named arguments via unknown target
- array namespaced vector count builtin alias call is rejected
- stdlib namespaced vector capacity accepts named arguments through imported stdlib helper
- stdlib namespaced vector capacity requires imported stdlib helper or explicit definition
- stdlib namespaced vector capacity accepts same-path helper on map target
- vector namespaced capacity accepts same-path helper on map target
- vector namespaced capacity accepts same-path helper on array target
- vector namespaced capacity accepts same-path helper on builtin vector target
- vector namespaced capacity accepts same-path helper on wrapper vector target
- stdlib namespaced vector capacity wrapper vector chain without helper reports unknown target
- stdlib namespaced vector capacity wrapper vector chain accepts same-path helper
- array namespaced capacity wrapper vector target without helper reports unknown target

### test_semantics_calls_and_flow_collections_wrapper_temporary_tryat_contains_inference.cpp

- map wrapper temporary tryAt auto inference requires canonical helper definition
- map wrapper temporary contains call validates canonical target classification
- map wrapper temporary contains auto inference uses canonical helper return
- map wrapper temporary contains call requires canonical helper definition
- map wrapper temporary contains auto inference requires canonical helper definition
- map compatibility at call requires explicit alias definition
- stdlib-owned map compatibility at call falls back to canonical helper
- map compatibility at call keeps explicit alias precedence
- map compatibility at_unsafe auto inference requires explicit alias definition
- map compatibility at_unsafe keeps explicit alias precedence
- stdlib namespaced map constructor does not resolve map alias helper fallback
- map unnamespaced count call resolves through canonical helper
- map unnamespaced count auto inference resolves through canonical helper
- map unnamespaced count call ignores explicit same-path alias helper
- map unnamespaced count call requires imported canonical helper or explicit definition
- map unnamespaced count auto inference requires imported canonical helper or explicit definition
- bare map at call resolves through canonical helper
- bare map at wrapper call resolves through canonical helper
- bare map at wrapper call requires imported canonical helper or explicit definition
- bare map at call ignores compatibility helper when canonical helper is absent
- bare map at call prefers canonical helper over compatibility alias
- bare map at_unsafe auto inference resolves through canonical helper
- bare map at_unsafe auto inference ignores compatibility helper when canonical helper is absent
- bare map at_unsafe auto inference prefers canonical helper over compatibility alias
- bare vector at call resolves through imported stdlib helper
- bare vector at wrapper call resolves through same-path helper
- bare vector at call prefers canonical helper over compatibility alias
- bare vector at call rejects named arguments even through imported stdlib helper
- bare vector at call requires imported stdlib helper or explicit definition
- bare vector at auto inference resolves through imported stdlib helper
- bare vector at auto inference requires imported stdlib helper or explicit definition
- bare vector at_unsafe auto inference resolves through imported stdlib helper
- bare vector at_unsafe auto inference requires imported stdlib helper or explicit definition

## semantics/capabilities (207 tests, 10 files)

### test_semantics_capabilities_builtins.cpp

- struct transforms are rejected on executions
- mut transforms are rejected on executions
- copy transforms are rejected on executions
- restrict transforms are rejected on executions
- visibility transforms are rejected on executions
- static transforms are rejected on executions
- reflection transforms are rejected on executions
- builtin arithmetic calls validate
- builtin negate calls validate
- builtin comparison calls validate
- builtin comparison accepts bool operands
- builtin comparison accepts bool and signed int
- builtin comparison rejects bool with u64
- builtin less_than calls validate
- builtin equal calls validate
- builtin not_equal calls validate
- builtin greater_equal calls validate
- builtin less_equal calls validate
- builtin and calls validate
- builtin and rejects float operands
- builtin and rejects struct operands
- builtin or calls validate
- builtin not calls validate
- builtin comparison accepts float operands
- builtin comparison accepts string operands
- builtin comparison rejects pointer operands
- builtin comparison rejects struct operands
- builtin comparison rejects mixed signed/unsigned operands
- builtin comparison rejects mixed int/float operands
- builtin clamp calls validate
- builtin clamp rejects mixed signed/unsigned operands
- builtin arithmetic arity mismatch fails
- builtin negate arity mismatch fails
- builtin comparison arity mismatch fails
- builtin less_than arity mismatch fails
- builtin equal arity mismatch fails
- builtin greater_equal arity mismatch fails
- builtin and arity mismatch fails
- builtin not arity mismatch fails
- builtin clamp arity mismatch fails
- void return can omit return statement
- local binding names are visible

### test_semantics_capabilities_lifecycle.cpp

- struct definitions reject non-binding statements
- handle transform marks struct definitions
- gpu_lane transform marks struct definitions
- struct transform rejects parameters
- lifecycle helpers require struct context
- lifecycle helpers allow struct parents
- lifecycle helpers allow nested definitions
- lifecycle helpers allow field-only struct parents
- lifecycle helpers provide this
- lifecycle helpers require mut for assignment
- lifecycle helpers must return void
- Create helpers reject parameters
- Copy helper accepts shorthand parameter
- Copy helper accepts Reference<Self> parameter
- Copy helper accepts resolved Reference parameter
- Copy helper requires reference parameter
- mut transform is rejected on non-helpers
- lifecycle helpers reject duplicate mut
- lifecycle helpers reject mut template arguments
- lifecycle helpers reject mut arguments
- lifecycle helpers reject non-struct parents
- this is not available outside lifecycle helpers
- lifecycle helpers accept placement variants

### test_semantics_capabilities_structs_core.cpp

- execution effects transform validates
- execution capabilities transform validates
- execution capabilities rejects template arguments
- execution capabilities rejects invalid capability
- execution capabilities rejects duplicate capability
- execution effects rejects template arguments
- execution effects rejects invalid capability
- execution effects rejects duplicate capability
- execution effects rejects duplicates
- execution capabilities rejects duplicates
- capabilities transform validates asset and gpu identifiers
- capabilities transform rejects duplicate gpu capability
- align_bytes validates integer argument
- align_bytes accepts digit separators
- align_bytes rejects non-integer argument
- align_bytes rejects wrong argument count
- align_kbytes rejects wrong argument count
- align_kbytes rejects template arguments
- align_kbytes validates integer argument
- align_kbytes accepts hex literal
- align_kbytes rejects non-integer argument
- struct transform validates without args
- no_padding transform validates without args
- no_padding rejects alignment padding
- platform_independent_padding rejects implicit padding
- platform_independent_padding allows explicit alignment
- struct layout rejects explicit canonical map field
- no_padding rejects explicit canonical map field before padding diagnostics
- struct alignment rejects smaller field requirement
- struct alignment rejects smaller struct requirement
- static fields do not affect struct alignment
- struct constructors ignore static fields
- pod transform validates without args
- pod transform rejects handle tag
- pod transform rejects handle fields
- handle and gpu_lane tags conflict on definition
- handle and gpu_lane tags conflict on field
- struct transform rejects template arguments
- struct transform rejects arguments

### test_semantics_capabilities_structs_metadata.cpp

- reflection core type primitives validate
- meta path core type primitives validate
- field_count rejects non-struct type targets
- field_count rejects non-reflect struct targets
- field_count accepts generate-enabled struct targets
- field metadata core primitives validate
- field metadata queries reject non-constant index
- field metadata queries reject out-of-range index
- field metadata queries reject non-struct type targets
- field metadata queries reject non-reflect struct targets
- generate SoaSchema emits reflected dynamic field-dispatch helpers
- generate SoaSchema chunk helpers split wide reflected schemas deterministically
- generate SoaSchema rejects helper collisions deterministically
- has_transform query validates
- has_transform rejects non-constant transform-name argument
- has_transform rejects invalid argument count
- has_trait query validates
- has_trait rejects non-constant trait-name argument
- has_trait rejects unsupported trait name
- has_trait rejects invalid template argument count
- unsupported reflection metadata queries are rejected
- runtime reflection object queries are rejected
- struct transform rejects return transform
- struct transform rejects return statements
- placement transforms are rejected
- struct definitions allow missing field initializers
- struct definitions allow initialized fields
- struct fields infer omitted envelopes from struct initializers
- struct fields reject ambiguous omitted envelopes
- recursive struct layouts are rejected

### test_semantics_capabilities_structs_reflect_clear.cpp

- generate Clear emits reflection helper definition
- generate Clear for empty reflected struct emits no-op helper
- generate Clear ignores static fields
- generate Compare Hash64 Clear helpers keep canonical v1.1 order
- generate Clear rejects existing helper collision
- generate CopyFrom emits reflection helper definition
- generate CopyFrom for empty reflected struct emits no-op helper
- generate CopyFrom ignores static fields
- generate Compare Hash64 Clear CopyFrom helpers keep canonical v1.1 order
- generate CopyFrom rejects existing helper collision

### test_semantics_capabilities_structs_reflect_compare.cpp

- generate Compare emits reflection helper definition
- generate Compare for empty reflected struct returns zero
- generate Compare ignores static fields
- generate Compare rejects existing helper collision
- generate Hash64 emits reflection helper definition
- generate Hash64 for empty reflected struct returns seed literal
- generate Hash64 ignores static fields
- generate Compare and Hash64 helpers keep canonical v1.1 order
- generate Hash64 rejects existing helper collision
- generate Compare rejects unsupported field envelope deterministically
- generate Hash64 rejects unsupported field envelope deterministically
- generate Compare unsupported field diagnostics keep source order

### test_semantics_capabilities_structs_reflect_default.cpp

- generate Default emits reflection helper definition
- generate Default supports static-only structs
- generate Default rejects existing helper collision
- generate IsDefault emits reflection helper definition
- generate IsDefault supports static-only structs
- generate IsDefault rejects existing helper collision
- generate Clone emits reflection helper definition
- generate Clone supports static-only structs
- generate Clone rejects existing helper collision
- generate DebugPrint emits reflection helper definition
- generate DebugPrint supports static-only structs
- generate DebugPrint accepts non-printable field types
- generate DebugPrint rejects existing helper collision

### test_semantics_capabilities_structs_reflect_equal.cpp

- reflection transforms validate on struct definitions
- reflection transforms validate on field-only structs
- reflect transform rejects non-struct definitions
- generate transform implies reflection on struct definitions
- generate transform rejects unsupported reflection generators
- generate transform reports deferred ToString generator
- generate transform rejects duplicate reflection generators
- generated reflection helpers are public
- generated reflection helpers are importable via wildcard
- generate helper pack emits canonical helper order
- generate Equal emits reflection helper definition
- reflected Equal structs accept operator equality
- reflected Equal operator equality resolves through generated helper
- struct equality still rejects reflected structs without Equal generation
- generate Equal for empty reflected struct returns true
- generate Equal ignores static fields
- generate Equal rejects existing helper collision
- generate NotEqual emits reflection helper definition
- generate NotEqual for empty reflected struct returns false
- generate NotEqual ignores static fields
- generate NotEqual rejects existing helper collision

### test_semantics_capabilities_structs_reflect_serde.cpp

- generate Serialize emits reflection helper definition
- generate Serialize for empty reflected struct returns version-only payload
- generate Serialize ignores static fields
- generate Serialize rejects existing helper collision
- generate Serialize rejects unsupported field envelope deterministically
- generate Deserialize emits reflection helper definition
- generate Deserialize for empty reflected struct keeps version guards only
- generate Deserialize ignores static fields
- generate Compare Hash64 Clear CopyFrom Validate Serialize Deserialize helpers keep canonical v2 order
- generate Deserialize rejects existing helper collision
- generate Deserialize rejects unsupported field envelope deterministically

### test_semantics_capabilities_structs_reflect_validate.cpp

- generate Validate emits reflection helper definition with field hooks
- generate Validate for empty reflected struct emits ok-only helper
- generate Validate ignores static fields
- generate Compare Hash64 Clear CopyFrom Validate helpers keep canonical v2 order
- generate Validate rejects existing helper collision
- generate Validate rejects existing field hook collision

## semantics/entry (250 tests, 11 files)

### test_semantics_entry_builtins_numeric.cpp

- infers return type from builtin clamp
- infers return type from builtin min
- infers return type from builtin abs
- infers return type from builtin saturate
- clamp argument count fails
- min argument count fails
- abs argument count fails
- saturate argument count fails
- clamp rejects mixed int/float operands
- min rejects mixed int/float operands
- max rejects mixed signed/unsigned operands
- sign rejects bool operand
- saturate rejects bool operand
- assign through non-mut pointer fails
- not argument count fails
- boolean operators accept bool operands
- boolean operators reject integer operands
- boolean operators reject unsigned operands
- not accepts bool operand
- boolean operators reject float operands
- convert requires template argument
- infers void return type without transform

### test_semantics_entry_entry.cpp

- missing entry definition fails
- entry definition rejects non-arg parameter
- entry definition rejects multiple parameters
- entry definition without args parameter is allowed
- entry definition accepts array<string> parameter
- entry definition rejects default args parameter
- entry definition rejects templated definition
- entry default effects enable io_out
- non-entry defaults remain pure

### test_semantics_entry_executions.cpp

- argument count mismatch fails
- execution target must exist
- execution argument count mismatch fails
- execution accepts bracket-labeled arguments
- execution validates imported canonical map helper arguments
- execution rejects unknown named argument
- execution rejects duplicate named argument
- execution body arguments are rejected
- execution rejects copy transform
- execution rejects mut transform
- execution rejects restrict transform
- execution rejects unsafe transform
- execution rejects placement transforms
- execution rejects duplicate effects transforms
- execution rejects duplicate capabilities transforms
- execution rejects invalid effects capability
- execution rejects invalid capability name
- execution rejects duplicate effects capability
- execution rejects duplicate capability
- execution rejects effects template arguments
- execution rejects capabilities template arguments
- execution capabilities require matching effects
- execution accepts effects and capabilities
- execution rejects struct transform
- execution rejects visibility transform
- execution rejects alignment transforms

### test_semantics_entry_math_imports.cpp

- math builtin requires import
- math builtin resolves with import
- math matrix stdlib constructor requires import
- math matrix stdlib constructor resolves with import
- math matrix stdlib constructor supports trailing defaults
- math quaternion stdlib constructor requires import
- math quaternion stdlib constructor resolves with import
- math quaternion stdlib constructor supports trailing defaults
- math quat_to_mat3 helper requires import
- math quat_to_mat3 helper resolves with import
- math quat_to_mat3 helper resolves with explicit import
- math quat_to_mat4 helper requires import
- math quat_to_mat4 helper resolves with import
- math quat_to_mat4 helper resolves with explicit import
- math mat3_to_quat helper requires import
- math mat3_to_quat helper resolves with import
- math mat3_to_quat helper resolves with explicit import
- math binding accepts implicit matrix quaternion conversion
- math helper rejects implicit matrix quaternion conversion with stable diagnostic
- math return rejects implicit matrix quaternion conversion with stable diagnostic
- math trig builtin requires import
- math trig builtin resolves with import
- math trig builtin resolves with explicit import
- math trig explicit import currently exposes sibling builtin names
- math-qualified builtin works without import
- math constant requires import
- math constants resolve with import
- math constant resolves with explicit import
- math constant explicit import currently exposes sibling constants
- math import rejects root definition conflicts
- explicit math import rejects root definition conflicts
- math import rejects root conflicts after definitions
- explicit math import rejects root conflicts after definitions
- math-qualified constant works without import
- math-qualified non-math builtin fails
- import rejects unknown math builtin
- import rejects math builtin conflicts
- import aliases namespace definitions
- import aliases namespace types and methods

### test_semantics_entry_operators.cpp

- arithmetic rejects bool operands
- arithmetic rejects string operands
- arithmetic rejects struct operands
- arithmetic plus accepts matching matrix operands
- arithmetic plus accepts matching quaternion operands
- arithmetic plus rejects mismatched matrix shapes
- arithmetic plus rejects mixed matrix quaternion operands
- arithmetic minus accepts matching matrix operands
- arithmetic minus accepts matching quaternion operands
- arithmetic minus rejects mismatched matrix shapes
- arithmetic minus rejects mixed matrix quaternion operands
- arithmetic multiply accepts matrix scalar scaling
- arithmetic multiply accepts scalar quaternion scaling
- arithmetic multiply accepts matrix vector products
- arithmetic multiply accepts matrix matrix products
- arithmetic multiply accepts quaternion quaternion products
- arithmetic multiply accepts quaternion vector rotation shape
- arithmetic multiply rejects mismatched matrix shapes
- arithmetic multiply rejects vector matrix ordering
- arithmetic multiply rejects quaternion vec4 operands
- arithmetic divide accepts matrix scalar division
- arithmetic divide accepts quaternion scalar division
- arithmetic divide rejects matrix matrix operands
- arithmetic divide rejects scalar quaternion operands
- arithmetic negate rejects bool operands
- arithmetic negate rejects unsigned operands
- arithmetic rejects mixed signed/unsigned operands
- arithmetic multiply rejects mixed signed/unsigned operands
- arithmetic rejects mixed int/float operands

### test_semantics_entry_parameters.cpp

- parameter default literal is allowed
- parameter default pure expression is allowed
- parameter qualifiers are allowed
- omitted parameter envelopes are inferred through parser shorthand
- omitted parameter envelopes reject templated definitions through parser shorthand
- parameter restrict matches binding type
- parameter restrict rejects mismatched type
- parameter accepts software numeric type
- software numeric parameter rejects non-numeric argument
- parameter default vector literal requires heap_alloc effect
- parameter default vector literal allows heap_alloc effect
- parameter default empty vector literal is allowed
- if statement sugar allows empty else block in statement position
- parameter default expression rejects name references
- parameter default expression rejects user-defined calls
- parameter default expression rejects user-defined array call
- parameter default expression allows builtin array literal
- parameter default expression rejects block arguments
- parameter default expression rejects bindings
- parameter default expression rejects named arguments
- parameter default rejects multiple arguments
- infers parameter type from default initializer
- typed variadic parameter accepts trailing positional arguments
- typed variadic parameter allows empty trailing pack
- typed variadic parameter accepts mismatched trailing argument
- implicit variadic pack infers homogeneous element type
- implicit variadic pack rejects heterogeneous element types
- typed variadic parameter accepts spread args pack forwarding
- implicit variadic pack infers from spread args pack
- spread argument requires variadic parameter
- spread argument requires args pack value
- execution spread argument requires args pack value
- variadic pointer string pack declaration is rejected
- variadic reference string pack declaration is rejected

### test_semantics_entry_resolution.cpp

- unknown identifier fails with parameter scope
- binding inference from expression enables method call validation
- namespace blocks may be reopened for sibling resolution
- call resolves to definition declared later
- execution resolves to definition declared later

### test_semantics_entry_return_inference.cpp

- infers return type without transform
- return auto infers value type
- return auto conflicts on mixed types
- return auto conflicts on mixed call sites
- return auto conflicts on named/default call path
- infers array return type without transform
- restrict matches inferred binding type during return inference
- restrict matches inferred binding type inside block return inference
- implicit void definition without return validates
- return auto without return is diagnostic
- return inference requires inferable binding types
- infers float return type without transform
- infers return type from builtin plus
- infers return type from builtin negate
- implicit auto inference supports nested helper calls
- infers struct return type without transform
- return auto infers struct constructor return type
- return struct mismatch is diagnostic

### test_semantics_entry_tags.cpp

- pod and handle tags conflict on definitions
- pod definitions reject handle fields
- pod and gpu_lane tags conflict on definitions
- handle and gpu_lane tags conflict on definitions
- fields reject handle and gpu_lane together
- fields reject pod and handle together
- fields reject pod and gpu_lane together
- handle tag does not replace binding type
- handle tag accepts template arg without changing type
- pod tag rejects template args on bindings
- duplicate static tags reject on bindings
- duplicate visibility tags reject on bindings
- visibility tags accept on definitions
- duplicate visibility tags reject on definitions
- static tag rejects on definitions
- copy tag rejects on definitions
- restrict tag accepts definition predicates

### test_semantics_entry_transforms.cpp

- unsupported return type fails
- array return requires template argument
- template vector and map returns are allowed
- canonical stdlib map returns are allowed
- vector return rejects wrong template arity
- map return rejects wrong template arity
- map return rejects unsupported builtin Comparable key contract
- canonical stdlib map return rejects direct template arguments
- vector return accepts array element type during semantics validation
- map return accepts array value type during semantics validation
- return transform rejects duplicate return
- effects transform aggregates distinct effects
- capabilities transform rejects duplicates

### test_semantics_entry_transforms_references.h

- reference return allows direct parameter reference
- reference return allows parameter-rooted borrow expression
- reference return rejects local reference escape
- reference return rejects local borrow escape
- reference return rejects derived parameter reference
- reference return rejects mismatched parameter-rooted borrow target
- reference return requires matching template target
- unsafe definitions allow overlapping mutable references
- unsafe reference rejects safe-call boundary escape
- unsafe reference allows unsafe-call boundary
- unsafe parameter reference allows named safe-call boundary
- unsafe reference rejects named safe-call boundary escape through call result
- unsafe transform rejects duplicate markers
- unsafe transform rejects arguments
- unsafe scope accepts reference location initialization
- unsafe scope allows pointer to reference conversion initializer
- unsafe scope rejects mismatched pointer to reference conversion
- unsafe pointer-converted reference rejects safe-call boundary escape
- unsafe reference rejects safe-call boundary escape through call result
- unsafe reference rejects safe-call boundary escape through if expression
- unsafe reference rejects safe-call boundary escape through block return expression
- unsafe reference rejects safe-call boundary escape through match expression
- unsafe parameter reference allows safe-call boundary
- unsafe reference rejects assignment escape to outer binding
- unsafe reference allows assignment through local alias
- unsafe reference allows assignment through parameter-rooted pointer alias
- unsafe reference rejects assignment escape through parameter alias
- unsafe reference rejects pointer-write escape through parameter alias
- unsafe reference rejects direct pointer-write escape to parameter
- safe reference rejects assignment escape to parameter
- safe reference rejects assignment escape through match expression
- safe reference allows assignment from parameter to parameter
- borrow checker rejects assign before pointer-alias last use
- borrow checker allows assign after pointer-alias last use
- borrow checker rejects move before pointer-alias last use
- borrow checker allows move after pointer-alias last use
- borrow checker rejects increment before pointer-alias last use
- borrow checker allows increment after pointer-alias last use

## semantics/manual (179 tests, 6 files)

### test_semantics_manual_borrows.h

- borrow checker rejects overlapping mutable and immutable references with future use
- borrow checker rejects overlapping mutable references with future use
- borrow checker allows overlapping immutable references
- borrow checker rejects assign to borrowed binding
- borrow checker rejects move from borrowed binding
- borrow checker allows assign after borrow scope ends
- borrow checker tracks references through location(ref)
- borrow checker allows assign after last use in same scope
- borrow checker allows mutable borrow after immutable last use
- borrow checker rejects assign before last reference use
- borrow checker rejects mutation before last reference use
- borrow checker allows mutation after reference last use
- borrow checker rejects pointer dereference assign before last reference use
- borrow checker allows pointer dereference assign after last reference use
- borrow checker rejects pointer dereference mutation before last reference use
- borrow checker allows pointer dereference mutation after last reference use
- borrow checker allows pointer arithmetic dereference assign without borrow
- borrow checker rejects pointer arithmetic assign before last reference use
- borrow checker allows pointer arithmetic assign after last reference use
- borrow checker rejects pointer arithmetic mutation before last reference use
- borrow checker allows pointer arithmetic mutation after last reference use
- borrow checker allows assign through location of mutable reference
- borrow checker allows pointer arithmetic write through location of mutable reference
- borrow checker rejects assign through location of immutable reference
- borrow checker rejects mutation through location of immutable reference
- borrow checker reports immutable location root for assign
- borrow checker reports immutable location arithmetic root for mutation
- borrow checker allows assign through location of mutable binding
- borrow checker rejects location(value) assign before last reference use
- borrow checker allows location(value) assign after reference last use
- borrow checker rejects location(value) mutation before last reference use
- borrow checker allows location(value) mutation after reference last use

### test_semantics_manual_calls.h

- missing return statement fails
- method calls require a receiver
- recursive return inference requires annotation
- bare zero-arg expression rewrites to call
- bare zero-arg statement rewrites to call
- local binding conflicts with bare zero-arg expression
- unique local binding still reads as value
- bare zero-arg import alias ambiguity rejects deterministically
- bare zero-arg statement conflicts with local binding
- non-zero-arg callable bare name stays diagnostic
- compile-time type local validates and is erased
- typeof symbol type local validates and is erased
- typeof symbol accepts parameter and earlier type local
- type locals annotate later local and struct field envelopes
- local generated structs consume enclosing type locals
- local generated struct identity is stable for template specialization
- local generated structs reject escapes and shadowing
- local generated struct escape diagnostic uses specialized path
- local generated structs reject forward facts and recursive storage
- type local envelopes reject forward and runtime value use
- typeof symbol rejects missing and runtime argument forms
- typeof symbol rejects unsupported callable and ambiguity
- type local is not a runtime value
- type local rejects runtime initializer
- type local rejects binding qualifiers
- type local shares duplicate binding namespace
- top-level type local rejects as binding syntax
- named arguments not allowed on builtin calls in manual AST
- execution unknown named argument fails in manual AST
- duplicate parameter name fails
- parameters must use binding syntax
- parameters reject block arguments
- call argument name count mismatch fails
- positional arguments may follow named arguments
- assign requires exactly two arguments
- convert requires exactly one argument
- array literal requires exactly one template argument
- if expression blocks require a value expression
- if statement allows empty branch blocks
- execution positional argument after named is allowed
- execution named arguments reorder
- execution duplicate named arguments fail
- execution arguments accept collection literals

### test_semantics_manual_core.h

- duplicate definitions fail
- return transform requires template argument
- return transform rejects multiple template arguments
- import math root is rejected
- return transform rejects arguments in manual AST

### test_semantics_manual_templates.h

- monomorphizes template definition bindings
- typeof symbol works after template monomorphization
- template argument count mismatch fails
- template arguments required for templated calls
- monomorphizes integer template arguments distinctly
- integer template arguments cannot substitute type positions
- type pack specializations bind zero one and many arguments
- type pack specialization requires ordinary arguments before pack
- type pack parameters cannot be used as scalar types before expansion
- type pack storage expands into deterministic struct fields
- type pack storage rejects invalid placements and shapes
- type pack helper bindings expand parameters locals and return envelopes
- type pack reflection helpers follow expanded field order
- implicit auto template inference for calls
- implicit auto template inference uses defaults
- implicit auto template inference honors named arguments
- implicit auto template inference supports named arguments with defaults
- implicit auto template inference for omitted parameters
- implicit auto template inference uses defaults for omitted parameters
- implicit auto parameters reject templated definitions when omitted
- match call behaves like if
- match cases validate
- match requires else block
- match rejects incompatible case patterns

### test_semantics_manual_types.h

- uninitialized not allowed in array element types
- uninitialized not allowed as user template arg
- uninitialized canonical map template arg validates as stdlib type
- canonical map template arg without uninitialized validates
- uninitialized struct fields are allowed
- nested uninitialized remains disallowed in pointer targets
- nested uninitialized remains disallowed in reference targets
- uninitialized return types are rejected
- implicit auto parameters reject templated definitions
- struct definition cannot return a value
- conflicting return types fail
- duplicate return transforms fail
- binding transform template arguments fail
- binding requires exactly one type
- binding defaults to int when omitted
- return blocks are rejected
- if trailing blocks are rejected
- if block wrappers do not require special names
- block arguments accept bindings on statement calls
- binding blocks are rejected
- literal statements are allowed
- assign target must be a mutable binding
- execution argument name count mismatch fails
- execution return transform rejects
- duplicate binding names fail
- binding cannot shadow parameter
- unknown call target fails
- return not allowed in expression context
- expression call accepts empty block arguments
- expression call accepts block arguments
- block expression yields last expression
- block expression requires a value
- block statement can contain return
- return rejects named arguments
- if rejects named arguments in manual AST
- if branches returning satisfy missing return checks
- if missing else return fails missing return checks
- for condition binding bool passes
- for condition binding requires bool in manual AST

### test_semantics_manual_uninitialized.h

- uninitialized binding accepts constructor
- uninitialized parameters are rejected
- uninitialized initializer type mismatch fails
- uninitialized helpers are statement-only
- uninitialized init/drop allowed as statements
- uninitialized take infers return kind
- uninitialized borrow infers return kind
- uninitialized init rejects non-storage binding
- uninitialized init rejects value type mismatch
- uninitialized init validates array element types
- uninitialized init validates vector element types
- uninitialized init validates map key/value types
- uninitialized init requires uninitialized storage
- uninitialized drop requires initialized storage
- uninitialized storage must be dropped before end of scope
- uninitialized return requires drop
- uninitialized non-void return requires drop
- uninitialized drop before non-void return passes
- uninitialized if expression requires drop on all branches
- uninitialized if expression drops on both branches
- uninitialized block expression propagates take
- uninitialized for condition take with reinit step passes
- uninitialized for condition take requires reinit before next check
- uninitialized while condition take with reinit body passes
- uninitialized while condition take requires reinit before next check
- uninitialized loop body take with reinit passes
- uninitialized loop body take requires reinit before next iteration
- uninitialized repeat body take with reinit passes
- uninitialized repeat body take requires reinit before next iteration
- uninitialized repeat count expression effects are tracked
- uninitialized loop literal one executes body exactly once
- uninitialized loop literal zero preserves initialized state
- uninitialized repeat literal one executes body exactly once
- uninitialized repeat false executes zero iterations
- uninitialized repeat true executes body once
- uninitialized loop literal one leaves storage uninitialized

## semantics/result_helpers (110 tests, 4 files)

### test_semantics_result_helpers.cpp

- Result.error in if conditions is bool-compatible
- direct Result.ok expressions participate in Result helpers
- stdlib result value sum validates explicit ok and error variants
- stdlib result overloads accept target-typed variants
- status-only stdlib Result sum overloads by template arity
- stdlib Result helper constructors keep template arguments
- stdlib result value sum participates in Result.error
- stdlib result value sum participates in Result.why
- stdlib result value sum accepts Result.ok compatibility
- qualified stdlib Result sum participates in try
- qualified stdlib Result try checks error type
- stdlib result value sum accepts Result.map compatibility
- stdlib result value sum accepts Result.and_then compatibility
- stdlib result value sum accepts Result.map2 compatibility
- stdlib result compatibility combinators accept direct sum-return sources
- stdlib result value sum rejects default construction
- status-only Result pick reports compatibility diagnostic
- Result.error rejects non-result argument
- Result.why explicit string binding accepts FileError results
- Result.why explicit string binding accepts non-FileError results
- Result.why infers string binding
- Result.error infers bool binding
- templated helper overload families resolve by exact arity
- templated helper overload families still reject duplicate same-arity definitions
- Result.map infers auto Result binding from lambda body
- Result.map lambda body accepts sum typed locals
- Result.map struct payload satisfies target typed Result binding
- Result.and_then and Result.map2 infer nested auto Result bindings
- Result.and_then infers direct Result.ok lambda return
- direct Result.ok keeps target payload mismatch diagnostic

### test_semantics_result_helpers_errors.cpp

- stdlib image error result helpers construct status and value results
- stdlib ImageError why wrapper covers direct and Result-based access
- stdlib ImageError constructor wrappers expose type-owned error values
- stdlib ImageError why wrapper rejects non image errors
- stdlib ImageError constructor wrappers reject unexpected arguments
- stdlib image error result helpers reject non image errors
- stdlib ImageError receiver methods reject unexpected arguments
- stdlib container error result helpers construct status and value results
- stdlib ContainerError camelCase constructor helpers expose type-owned error values
- exact ContainerError import keeps method helpers
- stdlib container error status helper rejects non container errors
- stdlib ContainerError status helper rejects non container errors
- stdlib ContainerError receiver methods reject unexpected arguments
- stdlib ContainerError camelCase constructor helpers reject unexpected arguments
- stdlib ContainerError why wrapper rejects non container errors

### test_semantics_result_helpers_file.cpp

- stdlib file error result helpers construct status and value results
- stdlib file error result helpers reject non file errors
- stdlib FileError why wrapper covers direct and Result-based access
- builtin FileError why method works without imported wrapper
- stdlib FileError eof wrapper covers direct and method access
- stdlib FileError camelCase eof wrapper covers direct and method access
- builtin FileError why method rejects explicit arguments without imported wrapper
- stdlib FileError eof constructor wrapper returns FileError values
- stdlib FileError root why alias is removed
- stdlib FileError root result aliases are removed
- stdlib FileError package status alias is removed
- stdlib FileError package value alias is removed
- stdlib FileError result methods reject unexpected arguments
- stdlib FileError root eof aliases are removed
- stdlib FileError eof wrapper rejects unexpected arguments
- stdlib FileError status helper rejects non file errors
- stdlib File helpers cover imported method and slash-call wrappers
- stdlib File snake_case direct write_line overload mix rejects mismatched cached value types
- stdlib File camelCase helpers cover imported method and slash-call wrappers
- stdlib File camelCase helpers stay valid through on_error result flows
- stdlib File camelCase readByte stays valid in nested stdlib namespaces
- exact stdlib file imports keep file and FileError method helpers
- stdlib File nine-value helpers cover imported method and slash-call wrappers
- stdlib File broader helper calls fall back to builtin variadics
- stdlib File text slash-call helpers require stdlib import
- stdlib File open slash-call helpers require stdlib import
- stdlib File camelCase open slash-call helpers require stdlib import
- stdlib File close slash-call helper infers template args from File receiver
- stdlib-owned definitions keep direct file helper imports visible
- stdlib-owned definitions keep transitive file helper imports visible

### test_semantics_result_helpers_gfx.cpp

- stdlib gfx error result helpers construct status and value results
- experimental stdlib gfx package status alias is removed
- stdlib GfxError status helper rejects non gfx errors
- stdlib GfxError receiver methods reject unexpected arguments
- canonical stdlib gfx error result helpers construct status and value results
- exact canonical GfxError import keeps method helpers
- canonical stdlib gfx error helpers return explicit strings
- canonical stdlib GfxError constructors expose type-owned error values
- canonical stdlib gfx package value alias is removed
- canonical root GfxError compatibility wrappers are removed
- canonical stdlib GfxError receiver methods reject unexpected arguments
- canonical stdlib GfxError constructors reject unexpected arguments
- canonical stdlib gfx error why helper rejects non gfx errors
- stdlib gfx Buffer helpers cover experimental method and slash-call wrappers
- canonical stdlib gfx Buffer helpers cover method and slash-call wrappers
- stdlib gfx Buffer readback helpers cover experimental method and slash-call wrappers
- canonical stdlib gfx Buffer readback helpers cover method and slash-call wrappers
- stdlib gfx Buffer compute access helpers cover experimental method and slash-call wrappers
- canonical stdlib gfx Buffer compute access helpers cover method and slash-call wrappers
- experimental stdlib gfx Buffer arg-pack method receivers cover value, borrowed, and pointer packs
- stdlib gfx Buffer allocation helpers cover experimental slash-call wrappers
- canonical stdlib gfx Buffer allocation helpers cover slash-call wrappers
- stdlib gfx Buffer upload helpers cover experimental slash-call wrappers
- canonical stdlib gfx Buffer upload helpers cover slash-call wrappers
- canonical stdlib gfx Buffer Result try consumers validate with explicit typed bindings
- canonical stdlib gfx Buffer slash-call helpers require stdlib import
- canonical stdlib gfx Buffer upload slash-call helpers require stdlib import
- canonical stdlib gfx Buffer readback slash-call helpers require stdlib import
- canonical stdlib gfx Buffer load slash-call helpers require stdlib import
- canonical stdlib gfx Buffer store slash-call helpers require stdlib import
- canonical stdlib gfx Buffer allocate slash-call helpers require stdlib import
- canonical stdlib gfx Buffer allocate helper rejects non-integer size
- canonical stdlib gfx Buffer allocate helper rejects unsupported element type
- canonical stdlib gfx Buffer readback helper rejects unsupported element type
- canonical stdlib gfx Buffer upload helper rejects unsupported element type

## semantics/root (228 tests, 17 files)

### test_semantics_condensation_dag.cpp

- condensation dag preserves acyclic chain order
- condensation dag collapses cycles and deduplicates cross-component edges
- condensation dag topological order stays deterministic across disconnected roots

### test_semantics_definition_partitioner.cpp

- semantic validation result sink preserves first-error adapter
- definition partitioner emits deterministic balanced contiguous chunks
- definition partitioner handles zero and oversized partition counts deterministically
- definition partitioner repeat runs are stable and cover each declaration once
- two-chunk definition validation flag preserves success parity
- benchmark semantic entrypoint preserves production semantic-product output
- semantic validation plan keeps diagnostics stable with publication requested
- two-chunk definition validation flag preserves diagnostic ordering parity
- n-chunk definition validation preserves diagnostic ordering parity
- n-chunk definition validation diagnostics stay ordered across repeated runs
- n-chunk semantic product output order stays stable across repeated runs
- single-thread and n-chunk semantic product outputs are equivalent
- semantic memory inline fixture semantic product is equivalent across worker counts
- definition validation diagnostics are equivalent across worker counts 1,2,4
- semantic product output is equivalent across worker counts 1,2,4
- worker-local definition validation context keeps semantic product equivalent across worker counts 1,2,4
- worker-local definition validation context keeps diagnostics equivalent across worker counts 1,2,4
- semantic product index families are equivalent across worker counts 1,2,4

### test_semantics_definition_prepass.cpp

- semantic validation pass manifest pins ordered pipeline phases
- semantic validation pass manifest classifies compatibility and facts
- semantic validation pass manifest exposes public handoff boundaries
- definition prepass indexes declarations and records duplicates deterministically
- semantic validation rejects invalid type-pack metadata
- semantic product formatting distinguishes type-pack parameters
- definition prepass is read-only over the program and repeat-run stable
- definition validation diagnostics keep source-order parity via prepass iteration
- semantic validation plan captures prepass inputs deterministically
- semantic validation plan partitions definition workers by public prepass

### test_semantics_enum.cpp

- enum desugars to struct static bindings
- enum rejects non-literal values

### test_semantics_imports.cpp

- import brings immediate children into root
- import resolves definitions declared before import
- collection wildcard import does not publish legacy vector wrapper helpers
- exact vector import does not publish fixed arity vector wrapper helpers
- direct experimental vector wildcard import is rejected
- direct experimental vector exact import is rejected
- direct experimental map wildcard import is rejected
- direct experimental map exact import is rejected
- internal vector wildcard import preserves backing Vector identity
- definition ast transform hook resolves local symbol metadata
- definition ast transform hook resolves imported public symbol metadata
- definition ast transform hook rewrites local definition body
- definition ast transform hook rewrites imported public definition body
- definition ast transform hook rejects unsupported FunctionAst result
- definition ast transform ct-eval adapter rejects unknown helper targets
- definition ast transform ct-eval adapter rejects contradictory helper input
- definition ast transform hook rejects imported private symbol
- definition ast transform hook rejects unsupported signature
- definition ast transform hook rejects text phase attachment
- definition ast transform hook rejects ambiguous imported symbols
- import resolves struct types and constructors
- import resolves methods on struct types
- implicit auto inference crosses imported call graph
- import aliases a single definition
- import does not alias nested definitions
- import aliases explicit nested definition path
- import does not alias namespace blocks
- import rejects namespace-only path
- import rejects /std/math without wildcard after semicolon
- import accepts whitespace-separated paths
- import accepts semicolon-separated paths
- import accepts wildcard math and util paths
- import accepts multiple explicit math paths
- import accepts explicit math min and max
- import rejects unknown wildcard path
- import rejects unknown single-segment path
- import rejects missing definition
- import rejects private definition path
- import rejects wildcard with only private children
- import conflicts with existing root definitions
- import conflicts with root builtin names
- import rejects explicit root builtin alias
- import conflicts between namespaces
- import resolves execution targets
- import accepts multiple paths in one statement
- import accepts comma-separated wildcards
- duplicate imports are ignored
- import works after definitions
- import ignores nested definitions
- import rejects nested definitions in root
- exact file imports keep bare bridge aliases
- exact collection imports keep bare bridge aliases
- exact map imports keep canonical wrapper access helpers visible
- exact vector imports keep bare bridge aliases
- exact vector and map imports keep bridge parity together
- wildcard collection imports keep bare vector and map bridge aliases
- wildcard collection imports keep bare map count alias
- exact vector import keeps map bridge aliases unavailable
- exact map import keeps vector bridge aliases unavailable
- stdlib-owned definitions keep direct collection helper imports visible
- stdlib-owned definitions keep exact map helper imports visible
- stdlib-owned definitions keep direct collection shim imports visible
- exact gfx imports keep bare bridge aliases

### test_semantics_imports_gfx.h

- import resolves std gfx experimental type surface
- import resolves std gfx canonical type surface
- import resolves std collections experimental map wildcard surface
- import resolves std gfx experimental substrate boundary
- import resolves std gfx experimental method wrapper surface
- import resolves std gfx canonical method wrapper surface
- canonical gfx imports reject wasm-browser targets without runtime substrate
- experimental gfx imports reject glsl targets without runtime substrate
- experimental gfx result wrappers reject bare explicit struct bindings
- experimental gfx window constructor entry point validates through stdlib helper
- experimental gfx window constructor still rejects bare explicit struct binding
- experimental gfx profile literals keep deterministic reject
- experimental gfx device constructor entry point validates through stdlib helper
- experimental gfx device constructor still rejects bare explicit struct binding
- canonical gfx window constructor entry point validates through stdlib helper
- canonical gfx profile literals keep deterministic reject
- canonical gfx device constructor entry point validates through stdlib helper
- experimental gfx Buffer allocation helper validates through builtin rewrite
- canonical gfx Buffer allocation helper validates through builtin rewrite
- experimental gfx pipeline entry point validates through stdlib helper
- canonical gfx pipeline entry point validates through stdlib helper
- experimental gfx pipeline entry point rejects unsupported vertex_type
- canonical gfx pipeline entry point rejects unsupported vertex_type
- import rejects missing std gfx experimental path

### test_semantics_lambdas.cpp

- lambda expressions validate without captures
- lambda captures allow outer bindings
- lambda captures accept ref qualifier
- lambda without capture rejects outer binding
- lambda rejects unknown capture name
- lambda rejects invalid capture qualifier
- lambda parameter default rejects user-defined array call
- lambda parameter default allows builtin array literal
- lambda rejects conflicting capture-all tokens
- lambda capture-all allows explicit known names
- lambda capture-all rejects explicit unknown names
- lambda calls reject named arguments

### test_semantics_maybe.cpp

- maybe some constructs present sum value
- maybe none constructs default sum value
- maybe default and explicit none are equivalent
- maybe some supports struct values
- maybe some returns from helper
- maybe none used in branches
- local generic maybe snake helpers reject monomorphized pick target
- local generic maybe camelCase helpers reject monomorphized pick target
- stdlib maybe import resolves specialized helper methods
- maybe direct constructor accepts explicit present variant
- maybe target-typed initializer accepts unique inferred payload
- maybe pick payload supports implicit helper inference
- stdlib maybe helper methods publish rooted semantic-product targets
- imported stdlib maybe helpers validate namespaced sum bodies
- maybe mutable struct helpers are retired on sum values

### test_semantics_move.cpp

- move marks binding as moved
- assign reinitializes moved binding
- move marks experimental vector binding as moved
- assign reinitializes moved experimental vector binding
- move requires binding name
- move rejects reference bindings

### test_semantics_numeric_software.cpp

- rejects integer binding and return
- rejects decimal binding and return
- rejects complex binding and return
- rejects mixed software and fixed arithmetic
- rejects mixed software numeric categories
- rejects ordered comparisons on complex
- rejects mixed complex and real comparisons

### test_semantics_strongly_connected_components.cpp

- strongly connected components keep acyclic nodes separate in node order
- strongly connected components merge a single cycle into one component
- strongly connected components keep multiple cycles in deterministic component order
- strongly connected components stay deterministic across disconnected subgraphs

### test_semantics_struct_helpers.cpp

- struct helper method call uses implicit this
- struct helper accepts explicit this argument
- static helper rejects method-call sugar
- implicit auto helper inference works through method-call sugar
- static helper allows direct calls
- static helper allows type-qualified dot calls
- generic struct helper resolves local templated root helper with implicit this reference
- mut helper allows assignment to this
- mut is rejected on static helpers

### test_semantics_sum_types.cpp

- sum declarations publish deterministic type and variant metadata
- sum declarations reject duplicate variant names
- sum declarations publish unit variant metadata
- sum declarations reject call-shaped unit variants
- generic sum declarations reject source call-shaped unit variants
- sum declarations reject field-like variant modifiers
- sum declarations reject unsupported payload envelopes
- generic sum declarations monomorphize payload metadata
- generic sum declarations reject invalid template arity
- generic sum overload families resolve by template arity
- generic sum overload families reject duplicate template arity
- generic sum declarations reject recursive inline payloads
- generic sum construction reports payload inference ambiguity
- explicit sum construction accepts labeled and positional payloads
- explicit sum construction works for fields and auto inference
- target-typed generic sum construction accepts explicit payload variants
- inferred sum construction accepts unique target-typed payloads
- inferred sum construction resolves imported sums deterministically
- explicit sum construction rejects invalid variant shapes
- sum construction supports unit variants and unit pick arms
- explicit sum construction rejects payload and target mismatches
- inferred sum construction rejects missing and ambiguous payloads
- pick rejects payload binders on unit variants
- pick validates exhaustive sum arms and payload binders
- pick statement arms can mutate through payload-specific branches
- pick rejects invalid sum arm shapes
- pick validates branch payload types and value compatibility

### test_semantics_symbol_interner.cpp

- symbol interner assigns deterministic first-seen ids
- symbol interner lookup and resolve handle missing values
- symbol interner clears ids and restarts numbering
- symbol interner repeat runs keep identical single-thread ids
- symbol interner worker snapshot keeps local id order
- symbol interner worker snapshot keeps earliest explicit origin per local id
- symbol interner merge prefers earliest origin over worker order
- symbol interner two-worker merge helper is input-order independent
- symbol interner two-worker merge helper is repeat-run deterministic
- symbol interner two-worker merge tie-breaks equal worker ids lexicographically
- symbol interner N-worker merge preserves canonical first-seen ordering
- symbol interner N-worker merge is permutation and repeat-run deterministic
- symbol interner N-worker merge tie-breaks equal worker ids lexicographically

### test_semantics_traits.cpp

- builtin numeric traits are satisfied
- missing trait requirement reports error
- trait signature mismatch reports error
- indexable builtins match element type
- indexable mismatch reports error

### test_semantics_tsan_smoke.cpp

- thread sanitizer smoke keeps multithread semantic-product deterministic
- thread sanitizer smoke keeps multithread diagnostic order deterministic

### test_semantics_uninitialized_fields.cpp

- uninitialized field helpers accept this field access
- uninitialized field init rejects non-storage field
- uninitialized field take infers return kind
- uninitialized field borrow infers return kind
- uninitialized local borrow validates reference binding
- uninitialized field borrow validates reference binding
- uninitialized dereferenced borrow validates reference binding
- uninitialized dereferenced borrow rejects reference binding type mismatch

## semantics/type_resolution (204 tests, 4 files)

### test_semantics_type_packs.cpp

- type pack index resolves return type and field access
- type pack index supports borrowed field access
- type pack index reports invalid index diagnostics

### test_semantics_type_resolution_graph.cpp

- type resolution graph builder keeps stable node and edge order for return and local auto dependencies
- type resolution graph exposes explicit invalidation contracts for local and cross definition edits
- type resolution graph builder records requirement edges for explicit return contracts
- type resolution dependency dag helper groups mutual-recursion graph components
- return dependency order exposes lowering-ready definition components
- return dependency order marks recursive definition components
- type resolution graph snapshot honors budget env vars
- type resolution graph perf soak (disabled by default)
- type resolution graph layer ordering stays consistent
- type resolution graph local binding invalidation follows dependency chain
- type resolution graph control-flow invalidation follows dependency chain
- type resolution graph local and initializer invalidation fanout stays definition-local
- type resolution graph control-flow invalidation fanout reports dependent return facts
- type resolution graph definition signature invalidation fanout reaches callers only
- type resolution graph import alias invalidation fanout reaches alias consumers
- type resolution graph receiver invalidation fanout reaches method consumers
- type resolution graph receiver invalidation avoids ambiguous method targets
- type resolution graph local binding invalidation counts bindings
- type resolution graph control-flow invalidation counts if calls
- type resolution graph initializer-shape invalidation tracks bindings
- type resolution graph definition signature invalidation counts definitions
- type resolution graph import alias invalidation counts imports
- type resolution graph receiver invalidation counts method calls
- type resolution dependency dag helper preserves acyclic graph dependency order
- type resolution graph builder resolves imported public call targets
- type resolution graph dump stays stable for a simple call chain
- type resolution graph dump stays stable for mutual recursion
- type resolution graph dump stays stable for namespace import resolution
- type resolution graph dump stays stable for template specialization expansion
- type resolution call binding snapshot shares template-specialization preparation
- type resolution local binding snapshot keeps shared try metadata after local flow
- type resolution local try metadata stays aligned with try snapshot metadata
- type resolution local binding snapshot marks direct-call metadata only for direct initializers
- type resolution local binding snapshot captures if-join local auto bindings with branch locals
- type resolution local binding snapshot preserves if-join incompatibility diagnostics with branch locals
- type resolution local binding snapshot captures omitted struct initializer envelope facts
- type resolution local binding snapshot preserves omitted struct envelope ambiguity diagnostics
- type resolution local binding snapshot prefers graph-backed vector helper aliases
- type resolution local binding snapshot keeps canonical helper path when alias is absent
- type resolution local binding snapshot rejects imported rooted soa_vector to_aos helper return without same-path helper
- type resolution local binding snapshot keeps rooted soa_vector to_aos helper return with same-path helper

### test_semantics_type_resolution_graph_snapshots.cpp

- require transforms publish evaluated builtin type predicate facts
- require transforms publish evaluated value predicate facts
- require value predicates use integer template arguments
- require value predicates reject failing integer template arguments
- require value predicates reject non-constant operands
- require facts publish phase-qualified compile-time effects
- generic semantic product handoff snapshots requirement and branch facts
- rejected generic semantic facts stop before product publication
- require builtin type predicates reject mismatched calls
- require builtin type predicates accept local generated structs
- require capability predicates accept trait constructor lifecycle and call facts
- require capability predicates reject failed trait checks with type facts
- require field predicates cannot observe private fields outside owner
- require call predicates resolve overload-style helpers deterministically
- require builtin type predicates diagnose invalid operands and reserved names
- require pure user predicates drive semantic facts
- require pure user predicates reject impure and unsupported bodies
- statement ct_if selects predicate branches before validation
- generic statement ct_if selects branches after specialization
- statement ct_if scopes selected branch generated structs
- statement ct_if branch generated structs do not leak
- statement ct_if branch generated escape names selected branch
- expression ct_if selects value branches before validation
- generic expression ct_if selects branches after specialization
- expression ct_if diagnoses selected branch type mismatch
- statement ct_if diagnoses invalid predicate conditions
- ct_if flow diagnostics distinguish invalid user predicates
- ct_if unsatisfied predicates select else without diagnostics
- duplicate require transforms fail closed before publication
- requirement constrained overload selects the viable same arity candidate
- requirement constrained overload uses local argument facts
- requirement constrained overload preserves no viable diagnostics
- requirement constrained overload reports value predicate rejection
- requirement constrained overload reports ambiguous candidates
- type resolution try operand metadata stays aligned with query snapshots
- templated fallback adapter seam classifies Result value and error envelope
- templated fallback adapter seam rejects missing Result envelope arguments
- explicit template-arg graph facts publish resolved specialization facts
- explicit template-arg graph facts publish builtin container template facts
- explicit template-arg graph facts keep mismatch diagnostics for invalid arity
- implicit template-arg graph facts publish inferred argument facts
- implicit template-arg graph facts publish helper-routing scope
- implicit template-arg graph facts keep conflict diagnostics
- explicit template-arg graph facts are consumed by inference cache
- explicit template-arg graph facts are consumed by transform rewrites
- implicit template-arg graph facts are consumed by inference cache
- type-resolution preparation reports template fact-cache hits
- type resolution graph snapshot records timing metrics
- type resolution local query metadata stays aligned with query snapshots
- type resolution local call metadata stays aligned with call snapshot
- type resolution query binding metadata stays aligned with call snapshot
- type resolution query call metadata stays aligned with call snapshot
- semantic product publishes resolved direct-call targets
- semantic product publishes resolved direct-call targets for local binding reads
- semantic product publishes resolved method-call targets
- semantic product publishes stdlib surface ids for direct, method, and bridge routing
- semantic product normalizes experimental vector bridge helper aliases
- semantic product publishes soa_vector bridge choices for canonical and experimental helpers
- semantic product method-call targets stay separated by receiver type
- semantic product keeps helper-return soa_vector mutator targets on alias wrappers
- semantic product keeps nested helper-return soa_vector mutator targets on alias wrappers
- semantic product keeps nested helper-return soa_vector read targets on alias wrappers
- semantic product keeps helper-return soa_vector read targets on alias wrappers
- semantic product keeps helper-return borrowed soa_vector read targets on canonical wrappers compatibility
- semantic product keeps method-like borrowed soa_vector read targets on canonical wrappers compatibility
- semantic product keeps borrowed soa_vector ref_ref targets on same-path helpers compatibility
- semantic product keeps builtin soa_vector ref_ref targets on same-path helpers
- semantic product validates direct return method-like borrowed helper-return experimental soa_vector reads
- semantic product keeps helper-return SoaVector mutator initializer facts on wrappers compatibility
- semantic product keeps helper-return borrowed soa_vector direct-call targets on canonical wrappers compatibility
- semantic product keeps helper-return borrowed soa_vector field views on canonical reads compatibility
- semantic product keeps borrowed local soa_vector field views on canonical reads compatibility
- semantic product keeps method-like borrowed soa_vector field views on canonical reads compatibility
- semantic product keeps helper-return soa_vector direct-call targets on alias wrappers
- semantic product keeps method-like direct soa_vector shadows on alias wrappers
- semantic product direct-call targets carry interned path ids
- semantic product method-call targets carry interned path ids
- semantic product publishes specialized SoaColumn field access targets
- semantic product publishes explicit SoaColumn helper parameter binding facts
- semantic product query facts prefer local bindings over math constants
- semantic product query facts include sum pick call targets
- semantic product query facts carry interned text ids
- semantic product binding facts carry interned text ids
- semantic product binding facts include sum typed locals
- semantic product return facts carry interned text ids
- semantic product local auto facts carry interned text ids
- semantic product try facts carry interned text ids
- semantic product try facts accept qualified stdlib Result spelling
- semantic product on_error facts carry interned text ids
- semantic product publishes same-path collection bridge routing choices
- semantic product publishes canonical collection bridge routing choices
- semantic product bridge routing choices carry interned path ids
- semantic product publishes callable effect and capability summaries
- semantic product callable summaries reuse interned return kind ids
- semantic product publishes struct and enum metadata
- semantic product publishes binding and return facts
- semantic product publishes array extent facts
- semantic product publishes graph-backed local auto query try and on_error facts
- semantic product publishes graph-backed local auto method-call facts
- semantic product publishes graph-backed collection helper direct-call facts
- semantic product publishes graph-backed collection constructor local-auto surface ids
- semantic product publishes vector map and soa_vector collection specializations
- semantic product keeps vector and map bridge parity
- semantic product keeps graph-backed local auto facts for nested borrowed array access helpers
- semantic product source locations stay aligned with AST-owned lowering facts
- semantic product semantic ids stay deterministic across repeated validation runs
- semantic product semantic ids ignore unrelated definition ordering
- local generated type identity stays deterministic in semantic product
- local generated type semantic ids ignore unrelated definition order
- local generated type paths are pinned in boundary dumps
- semantic product ownership surfaces keep deterministic source order
- semantic product lowering preserves debug source-map provenance
- semantic product lowering keeps semantic meaning while source locations stay AST-owned
- type resolution local Result.ok metadata stays aligned with wrapped call snapshots
- semantic product formatter emits deterministic lowering-facing sections
- semantic product formatter resolves module direct-call indices deterministically
- semantic product formatter resolves module method-call indices deterministically
- semantic product formatter resolves module bridge-path-choice indices deterministically
- semantic product formatter keeps bridge-path-choice text parity for flat vs module-index storage
- semantic product formatter resolves module callable-summary indices deterministically
- semantic product formatter keeps callable-summary text parity for flat vs module-index storage
- semantic product formatter resolves module binding-fact indices deterministically
- semantic product formatter keeps binding-fact text parity for flat vs module-index storage
- semantic product formatter resolves module return-fact indices deterministically
- semantic product formatter keeps return-fact text parity for flat vs module-index storage
- semantic product formatter resolves module local-auto-fact indices deterministically
- semantic product formatter keeps local-auto-fact text parity for flat vs module-index storage
- semantic product formatter resolves module query-fact indices deterministically
- semantic product formatter keeps query-fact text parity for flat vs module-index storage
- semantic product formatter resolves module try-fact indices deterministically
- semantic product formatter resolves module on-error-fact indices deterministically
- semantic product formatter keeps try/on-error text parity for flat vs module-index storage
- semantic product formatter keeps first dedup-slice text parity for flat vs module-index storage
- semantic product formatter keeps second dedup-slice text parity for flat vs module-index storage
- semantic product formatter exact golden is stable
- semantic product dump helper matches formatter output

### test_semantics_type_resolution_return_solver.cpp

- graph type resolver infers acyclic helper return kinds
- graph type resolver reports self-recursive return inference cycles explicitly
- graph type resolver reports mutual recursion cycle members in deterministic order
- graph type resolver preserves unresolved acyclic diagnostic
- graph type resolver preserves conflicting return diagnostic
- graph type resolver preserves conflicting collection template return diagnostic
- graph type resolver infers direct-call auto binding from struct-return helper
- graph type resolver infers direct-call auto binding from auto map-return helper
- graph type resolver infers direct-call auto binding from imported map-return helper
- graph type resolver infers direct-call auto binding from collection-return helper
- graph type resolver infers block-valued auto binding from helper-returned struct
- graph type resolver infers control-flow auto binding from helper-returned collection
- graph type resolver infers if-join return from branch-local auto collection values
- graph type resolver infers omitted struct field envelope from block-valued helper
- graph type resolver infers omitted struct field envelope from control-flow helper
- graph type resolver preserves ambiguous omitted struct field envelope diagnostic
- graph type resolver answers collection receiver queries through shared return-binding inference
- graph type resolver answers result queries through shared return-binding inference
- graph type resolver answers map receiver queries through shared type-text helper
- graph type resolver infers map value return kinds through shared infer helper
- graph type resolver preserves nested string helper inference through shared collection receiver classifiers
- graph type resolver infers auto return kinds through shared collection receiver classifiers
- graph type resolver shares borrowed indexed collection plumbing for soa_vector auto returns
- default semantics path carries grounded mutual recursion

## text_filter (174 tests, 7 files)

### test_text_filter_helpers.cpp

- character classification helpers
- unary prefix detection
- right operand start detection
- exponent sign detection
- quoted helper functions
- matching parens skip quoted text
- token boundary helpers
- strip and normalize unary operands
- template list heuristics
- literal suffix helpers
- rewrite unary helpers
- rewrite unary helpers with parens
- rewrite unary address-of rejects logical and
- rewrite unary mutation helpers

### test_text_filter_pipeline_basics.cpp

- pipeline passes through text
- pipeline preserves quoted import paths
- pipeline no longer treats legacy include paths as import directives
- pipeline no longer preserves versioned legacy include payload strings
- pipeline preserves import with version attribute
- pipeline preserves import with comments
- pipeline preserves import with tight comment
- pipeline preserves import with payload comments
- implicit-utf8 runs in import-like paths
- pipeline preserves single-quoted import paths
- pipeline preserves line comments
- filters ignore line comments
- pipeline preserves block comments
- filters ignore block comments
- append_operators only modifies opted-in lists
- append_operators does not duplicate operators
- pipeline isolates lambda bodies from transforms

### test_text_filter_pipeline_collections.cpp

- rewrites array literal braces
- rewrites soa literal braces
- rewrites soa literal brackets
- map equals pairs use generic assignment rewrite
- map generic assignment preserves trailing equality
- map nested assignment stays generic
- rewrites nested expressions inside array literal
- map typed string keys use generic assignment rewrite
- map nested values use generic assignment rewrite
- fails on unterminated collection literal
- fails on unterminated block comment in map constructor expression
- map whitespace pairs stay ordinary around line comments
- fails on unterminated template list in collection literal
- rewrites plus operator with exponent float
- does not add suffix without implicit filter

### test_text_filter_pipeline_implicit_i32.cpp

- adds i32 suffix to bare integer literal
- adds i32 suffix to negative integer literal
- adds i32 suffix to hex integer literal
- adds i32 suffix to negative hex integer literal
- rewrites plus with hex literals
- rewrites plus with comma-separated literal
- does not change suffixed integer literals
- does not add suffix to i64 or u64 literals with implicit filter
- implicit i32 filter skips float with trailing dot
- implicit i32 filter skips float with suffix after dot
- implicit i32 filter skips float with exponent after dot
- does not rewrite numbers inside strings
- does not rewrite numbers inside raw strings
- does not add suffix to float literal
- does not add suffix to float literal without suffix
- does not add suffix to float literal with exponent
- does not add suffix before identifier characters
- does not add suffix before dot access
- does not rewrite collection literal name prefixes

### test_text_filter_pipeline_implicit_utf8.cpp

- implicit utf8 appends to single-quoted strings
- implicit utf8 preserves explicit suffix
- implicit utf8 preserves raw_ascii suffix
- implicit utf8 can be disabled
- rewrites plus operator with raw string literals
- rewrites multiply operator with unary minus operand
- rewrites assign with unary minus operand
- rewrites multiply with negative numeric literal
- rewrites minus operator without spaces
- rewrites assign operator without spaces
- rewrites multiply operator without spaces
- rewrites greater_than operator without spaces
- rewrites equal operator without spaces
- rewrites not_equal operator without spaces
- rewrites less_than operator without spaces
- rewrites greater_equal operator without spaces
- rewrites less_equal operator without spaces
- rewrites and operator without spaces
- rewrites or operator without spaces
- rewrites not operator without spaces
- rewrites not operator with numeric literal
- rewrites not operator before parentheses
- rewrites not operator before templated call
- rewrites chained not operators
- rewrites chained not operators with implicit i32
- rewrites not operator before negative literal
- rewrites not operator before negative literal with implicit i32
- rewrites unary minus before name
- rewrites prefix increment operator
- rewrites postfix increment operator
- rewrites prefix decrement operator
- rewrites postfix decrement operator
- rewrites plus with templated call on left
- rewrites plus with templated call on right
- rewrites unary minus before parentheses
- rewrites location before name
- rewrites dereference before name
- does not rewrite template list syntax
- does not rewrite nested template lists
- rewrites spaced slash
- rewrites spaced plus
- rewrites spaced minus
- rewrites spaced multiply
- rewrites spaced less_than
- rewrites spaced comparisons and boolean ops
- does not rewrite spaced not operator
- does not rewrite negative numeric literal
- does not rewrite path tokens
- does not rewrite line comments
- does not rewrite block comments
- rewrites unary minus after block comments
- rewrites address-of after block comments
- does not rewrite operators around block comments
- does not rewrite operators after block comments
- does not rewrite operators before block comments

### test_text_filter_pipeline_rewrites.cpp

- rewrites divide operator without spaces
- does not rewrite slash paths
- does not rewrite slash-method helper paths after dot
- does not rewrite slash-method helper paths after identifier receivers
- does not rewrite slash paths at the start of later statements
- does not rewrite slash path calls with slash path arguments across later statements
- rewrites plus operator without spaces
- rewrites plus operator with spaces
- rewrites assign operator with spaces
- rewrites assignment with divide precedence
- rewrites multiply before plus
- rewrites chained assignment right associative
- operator rewrite keeps sum unit variant before payload envelope
- operator rewrite preserves type-pack template syntax
- rewrites plus operator with float literals
- keeps array literal braces
- rewrites array literal brackets to braces
- array bracket literal rewrite does not consume following indexing
- keeps map braces as ordinary call syntax
- leaves map bracket syntax outside collection rewrites
- keeps vector literal braces
- rewrites vector literal brackets to braces
- map bracket syntax only gets generic operator rewrites
- map brace equals uses generic assignment rewrite
- map brace semicolon keeps generic assignment rewrite
- map brace call key uses generic assignment rewrite
- map whitespace payload is not pair-rewritten
- map generic operator rewrite preserves line comments
- map generic operator rewrite preserves block comments
- filter order does not make map collection-owned
- default filters use generic map assignment rewrite
- map equals without commas uses generic assignment rewrite
- map bracket string keys only get generic rewrites
- does not rewrite bracket collections without template list
- map assignment in values stays generic
- map string keys use generic assignment rewrite
- map generic assignment preserves nested comparisons
- map generic assignment preserves named args
- nested map syntax is not collection-pair rewritten
- map whitespace-heavy assignments stay generic
- rewrites plus operator with call operands
- rewrites multiply with parenthesized operand
- rewrites plus operator with negative literal
- rewrites plus operator with unary minus operand
- rewrites plus operator with leading unary minus name
- rewrites plus operator with string literals

### test_text_filter_transform_rules.cpp

- transform registry registration preserves deterministic order
- transform registry supports same name in multiple phases
- default transform lookups follow registry phase metadata
- transform rules reject non-absolute root wildcard match
- transform rules reject empty path for root wildcard match
- apply semantic transform rules returns early when empty
- apply semantic transform rules handles executions
- apply semantic transform rules skips unmatched execution

## vm/debug (26 tests, 3 files)

### test_vm_debug_session.cpp

- vm debug stop reasons are stable and fully covered
- vm debug command transitions enforce session model legality
- vm debug stop reasons transition running state deterministically
- vm debug stop reasons are rejected outside running state
- vm debug session step transcript is deterministic
- vm debug session continue and pause controls are deterministic
- vm debug snapshot payload tracks locals and operand stack across steps
- vm debug snapshot payload exposes concrete locals for non-top frames
- vm debug session validates control-state preconditions
- vm debug runtime hooks cover all event kinds
- vm debug fault diagnostics include mapped source stack traces
- vm debug session owns argv text after startup
- vm and debug session share numeric opcode behavior
- vm and debug session share control flow opcode behavior
- vm debug hook event ordering is deterministic and replayable

### test_vm_debug_session_breakpoints.h

- vm debug session breakpoints validate and support multi-hit flows
- vm resolves source breakpoints for single and multi-hit mappings
- vm source breakpoint resolution reports ambiguous span diagnostics
- vm source breakpoint resolution filters by source unit
- vm debug session adds source breakpoints and hits all mapped locations
- vm debug breakpoints follow executed branch path deterministically
- vm debug opcode matrix matches vm execute for expanded families

### test_vm_debug_session_protocol.h

- vm debug adapter emits deterministic protocol transcripts
- vm debug adapter launch keeps owned argv text alive
- vm debug adapter reports invalid debug protocol queries
- vm debug adapter exposes caller locals for non-top frames

## vm/root (5 tests, 1 files)

### test_vm_execution_kernel_boundary.cpp

- vm execution kernel owns frame calls and numeric opcodes
- vm execution kernel delegates runtime-only print opcodes to host
- vm execution kernel keeps argv and local memory behind host boundary
- vm execution kernel exposes file opcode host boundary
- vm execution kernel avoids runtime-only dependencies
