# Refactoring helpers (TODO-5418)

Tools used to split the giant `src/semantics` files and functions (TODO-5384/5385).
They rewrite source in place and are meant to be run on a clean tree with the
release gate as the check. Run each from the repo root.

| Tool | Does |
| --- | --- |
| `split_file.py SRC HELPERS NS SUFFIX:LINE...` | Moves a leading anonymous namespace into `HELPERS.h` (named namespace `NS`, functions made `inline`) and cuts the remaining members into `SRC<Suffix>.cpp` parts starting at the given lines. |
| `split_anon_namespace.py SRC HELPERS NS SUFFIX:LINE...` | For files that are one big anonymous namespace: types and function declarations go to the header, definitions to parts. |
| `extract_lambda.py SRC FN SUFFIX HEADER NAME[=Ret]...` | Moves top-level lambdas of `FN` that only capture the function's parameters into free functions. |
| `phase_split.py CONFIG.json` | Splits the statements of a giant function body into phase functions sharing a state struct (see the config keys in its docstring). |
| `register_split.py ORIG NEWPART...` | Adds the new parts to `CMakeLists.txt` after `ORIG` and drops `ORIG` from the size allowlist. |
| `list_statements.py SRC BLOCK_LINE [N]` | Prints candidate cut lines for a block, roughly every N lines. |
| `../finish_todo.py TODO-N 'result'` | Finishes a TODO block (not a refactor tool; used with them). |

## Rules learned the hard way

- **RAII scope guards must live in the state struct.** An `std::optional<Scope>` declared in one phase is destroyed when that
  phase returns (`EntryArgStringScope` and `EffectScope` broke entry-argument tests). Use `force_state`.
- **By-value parameters are copied into the state**; lambdas stored in the state would otherwise capture a dead copy.
- **Reference locals become pointers** in the state (`const Expr &x = ...` -> `const Expr *x`, alias `*st.x`).
- **`auto` locals need an explicit type** in the config `types` map; lambdas need a return type in `lambda_ret`.
- **Default arguments stay on the declaration**, not the out-of-line definition.
- **Anonymous-namespace helpers become a named-namespace header** (inline functions) so every part sees one definition.
- **`return` inside a nested lambda must not be rewritten**; the tool tracks lambda bodies, check the diff.
- **Surface-audit exemptions are rows in `scripts/surface_audit_exemptions.txt`**, not comments: a split adds one row per new unit that needs it.
- After a split run `scripts/generate_test_inventory.py` if tests moved, and the full gate; semantic-product dumps must stay byte-identical.

Only `split_file.py`, `register_split.py` and `../finish_todo.py` have fixture tests; the others were validated
by the real refactors and fail loudly on shapes they do not understand.
