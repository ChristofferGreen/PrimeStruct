# Borrow Checker Negative Examples

These examples are intentionally invalid and are meant to demonstrate borrow-checker
and reference-escape diagnostics, including a pointer that would cross to a
spawned task (`04_spawn_argument_not_sendable`), a view captured by a closure that becomes a
`Function` (`05_function_capture_not_sendable`), and a vector used after `push` moved it
(`06_use_after_push_move`), and container storage helpers that only `[unsafe]` code may call
(`07_container_slot_helper_requires_unsafe`, `08_container_free_requires_unsafe`, `11_soa_slot_helper_requires_unsafe`), a pointer used after its
root moved (`09_pointer_used_after_move`), and a view whose container is changed through a `Reference` argument
(`10_view_changed_through_reference_argument`), and a view of `dereference(<Reference parameter>)` that is
cleared through the parameter (`12_view_of_dereferenced_parameter_cleared`).

They are not part of the standard "compile all examples" IR sweep.
The test suite validates them via `--emit-diagnostics` and asserts expected JSON
fragments from their `.expected.txt` files.
