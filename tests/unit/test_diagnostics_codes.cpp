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

TEST_CASE("parser diagnostic stability contract stays source locked") {
  const primec::DiagnosticStabilityContract contract =
      primec::diagnosticStabilityContract(primec::DiagnosticCode::ParseError);

  CHECK(primec::diagnosticCodeString(primec::DiagnosticCode::ParseError) == "PSC1003");
  CHECK(contract.code == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.message == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.primarySpan == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.notes == primec::DiagnosticStabilityTier::Stable);
  CHECK(primec::diagnosticStabilityTierString(contract.message) == "stable");
}

TEST_CASE("unclassified diagnostic fields stay implementation tier") {
  const primec::DiagnosticStabilityContract contract =
      primec::diagnosticStabilityContract(primec::DiagnosticCode::SemanticError);

  CHECK(contract.code == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.message == primec::DiagnosticStabilityTier::Implementation);
  CHECK(contract.primarySpan == primec::DiagnosticStabilityTier::Implementation);
  CHECK(contract.notes == primec::DiagnosticStabilityTier::Implementation);
  CHECK(primec::diagnosticStabilityTierString(contract.message) == "implementation");
}

TEST_CASE("semantic unknown-call diagnostics stay source locked") {
  const primec::DiagnosticStabilityContract contract =
      primec::diagnosticStabilityContract(primec::DiagnosticCode::SemanticError,
                                          "unknown call target: missing_call");
  const primec::DiagnosticStabilityContract otherSemanticContract =
      primec::diagnosticStabilityContract(primec::DiagnosticCode::SemanticError,
                                          "argument count mismatch for /take_two");

  CHECK(primec::diagnosticCodeString(primec::DiagnosticCode::SemanticError) == "PSC1005");
  CHECK(contract.code == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.message == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.primarySpan == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.notes == primec::DiagnosticStabilityTier::Stable);
  CHECK(otherSemanticContract.message == primec::DiagnosticStabilityTier::Implementation);
  CHECK(otherSemanticContract.primarySpan == primec::DiagnosticStabilityTier::Implementation);
  CHECK(otherSemanticContract.notes == primec::DiagnosticStabilityTier::Implementation);
}

TEST_SUITE_END();
