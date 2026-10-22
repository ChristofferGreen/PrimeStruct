# Finished TODOs - undated

- [x] TODO-5541: Host functions take and return `String`
  - owner: ai
  - status: done
  - created_at: 2026-10-08
  - phase: Strings and slices (docs/spec/strings-and-views.md)
  - parallel_track: string-host
  - scope: The host boundary for owned text, independent of the `string` view. A `[host]` definition may name the stdlib `String` as a parameter or result type. A `String` parameter crosses as `IrHostValueKind::Text`: the compiler passes the string's data block and the VM hands the host a pointer to its NUL-terminated bytes without a copy. A `String` result crosses as `Text` too: the host returns text, the VM copies it into a fresh string-heap block, and the call builds the `String` (length, capacity, data, owning) in the program. The embedding API gains `HostText` (result) and `HostTextView` (parameter) next to the existing `std::string` forms. PSIR v30. docs/Embedding.md and docs/spec/vm-design.md updated. A module-table `string` literal in `String` argument position and `[host("link_name")]` are deferred to TODO-5571, which needs them.
  - acceptance:
    - embed tests: String arguments and results round-trip, empty text, a loop of 10000 host text calls keeps the string heap constant, discarded results are freed; full release gate green
  - stop_rule: the host boundary on the VM and the embedding API; the native UI port is TODO-5571 and the C++ emitters are TODO-5564.
