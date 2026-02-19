# Raytracer Language Gaps

This captures the concrete language/runtime gaps hit while implementing the raytracer example, plus suggested fixes.

- Allow `Reference<array<T>>` bindings and parameter types (currently rejected as unsupported reference target type).
- Allow `location(...)` on parameters (currently only local bindings are accepted), or introduce a first-class `out`/`ref` parameter convention that does not require `location`.
- Clarify/resolve parsing ambiguity where a binding like `[array<f32>] v{...}` after a call can be misread as a template call without a separator; likely requires stricter statement boundaries or a canonical semicolon rule.
