#include "primec/Diagnostics.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.diagnostics.codes");

TEST_CASE("import diagnostic code stays stable") {
  CHECK(primec::diagnosticCodeString(primec::DiagnosticCode::ImportError) == "PSC1001");
}

TEST_CASE("import diagnostic record emits stable code") {
  const primec::DiagnosticRecord record =
      primec::makeDiagnosticRecord(primec::DiagnosticCode::ImportError, "import failed", "main.prime");
  CHECK(record.code == "PSC1001");
  CHECK(record.message == "import failed");
}

TEST_SUITE_END();
