# Borrow Checker Negative Examples

These examples are intentionally invalid and are meant to demonstrate borrow-checker
and reference-escape diagnostics.

They are not part of the standard "compile all examples" IR sweep.
The test suite validates them via `--emit-diagnostics` and asserts expected JSON
fragments from their `.expected.txt` files.
