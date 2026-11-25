#include "primec/ast/Ast.h"
#include "primec/ir_lowerer/IrLowererErrorLocator.h"
#include "primec/support/DiagnosticNames.h"
#include "primec/support/Diagnostics.h"

#include "third_party/doctest.h"

#include <optional>

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

TEST_CASE("lowering variadic reference-pack diagnostics stay source locked") {
  const primec::DiagnosticStabilityContract contract =
      primec::diagnosticStabilityContract(primec::DiagnosticCode::LoweringError,
                                          primec::VariadicArgsReferenceForwardingDiagnosticMessage);
  const primec::DiagnosticStabilityContract otherLoweringContract =
      primec::diagnosticStabilityContract(primec::DiagnosticCode::LoweringError,
                                          "native backend requires typed parameters on /main");

  CHECK(primec::diagnosticCodeString(primec::DiagnosticCode::LoweringError) == "PSC2001");
  CHECK(contract.code == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.message == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.primarySpan == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.notes == primec::DiagnosticStabilityTier::Stable);
  CHECK(otherLoweringContract.message == primec::DiagnosticStabilityTier::Implementation);
  CHECK(otherLoweringContract.primarySpan == primec::DiagnosticStabilityTier::Implementation);
  CHECK(otherLoweringContract.notes == primec::DiagnosticStabilityTier::Implementation);
}

namespace {

primec::TemplateInstanceLookup instanceLookup() {
  return [](const std::string &path) -> std::optional<primec::TemplateInstanceName> {
    if (path == "/std/result/Result__arity2__ta77c01") {
      return primec::TemplateInstanceName{"/std/result/Result__arity2", {"i32", "/pkg/Oops"}};
    }
    if (path == "/std/collections/vector/Vector__t4f2a") {
      return primec::TemplateInstanceName{"/std/collections/vector/Vector", {"i32"}};
    }
    if (path == "/std/collections/vector/Vector__t9b9b") {
      return primec::TemplateInstanceName{"/std/collections/vector/Vector",
                                          {"/std/result/Result__arity2__ta77c01"}};
    }
    return std::nullopt;
  };
}

} // namespace

TEST_CASE("a generated template instance reads as the type the program wrote") {
  const primec::TemplateInstanceLookup lookup = instanceLookup();
  CHECK(primec::userFacingDiagnosticText("/std/result/Result__arity2__ta77c01", lookup) ==
        "Result<i32, Oops>");
  CHECK(primec::userFacingDiagnosticText("/std/collections/vector/Vector__t4f2a", lookup) ==
        "Vector<i32>");
}

TEST_CASE("a member of a generated template instance keeps its own name") {
  const primec::TemplateInstanceLookup lookup = instanceLookup();
  CHECK(primec::userFacingDiagnosticText("call=/std/collections/vector/Vector__t4f2a/push failed",
                                         lookup) == "call=Vector<i32>/push failed");
  CHECK(primec::userFacingDiagnosticText(
            "no payload: /std/result/Result__arity2__ta77c01/Error (i32)", lookup) ==
        "no payload: Result<i32, Oops>/Error (i32)");
}

TEST_CASE("a generated name inside the argument of an instance is spelled out in turn") {
  const primec::TemplateInstanceLookup lookup = instanceLookup();
  CHECK(primec::userFacingDiagnosticText("bad /std/collections/vector/Vector__t9b9b here",
                                         lookup) == "bad Vector<Result<i32, Oops>> here");
}

TEST_CASE("text without a known generated name is unchanged") {
  const primec::TemplateInstanceLookup lookup = instanceLookup();
  CHECK(primec::userFacingDiagnosticText("unknown call target: /main", lookup) ==
        "unknown call target: /main");
  CHECK(primec::userFacingDiagnosticText("/pkg/Thing__tdeadbeef is not an instance", lookup) ==
        "/pkg/Thing__tdeadbeef is not an instance");
  CHECK(primec::userFacingDiagnosticText("a __t b and __tz", lookup) == "a __t b and __tz");
  CHECK(primec::userFacingDiagnosticText("", lookup) == "");
}

TEST_CASE("a lowering failure is located at the innermost expression that has a position") {
  primec::Expr call;
  call.sourceLine = 9;
  call.sourceColumn = 3;
  primec::Expr argument;
  argument.sourceLine = 9;
  argument.sourceColumn = 8;
  primec::Expr synthesized;

  primec::ir_lowerer::LoweringErrorLocator locator;
  // The innermost expression fails first; the expressions around it report the same message afterwards.
  locator.noteFailure(synthesized, "boom");
  CHECK_FALSE(locator.spanOf("boom").has_value());
  locator.noteFailure(argument, "boom");
  locator.noteFailure(call, "boom");
  const std::optional<primec::DiagnosticSpan> span = locator.spanOf("boom");
  REQUIRE(span.has_value());
  CHECK(span->line == 9);
  CHECK(span->column == 8);
  CHECK_FALSE(locator.spanOf("another message").has_value());
}

TEST_CASE("a lowering failure that was dropped does not locate a later failure") {
  primec::Expr dropped;
  dropped.sourceLine = 2;
  dropped.sourceColumn = 1;
  primec::Expr later;
  later.sourceLine = 30;
  later.sourceColumn = 5;

  primec::ir_lowerer::LoweringErrorLocator locator;
  locator.noteFailure(dropped, "tried and dropped");
  locator.noteFailure(later, "the real failure");
  CHECK_FALSE(locator.spanOf("tried and dropped").has_value());
  const std::optional<primec::DiagnosticSpan> span = locator.spanOf("the real failure");
  REQUIRE(span.has_value());
  CHECK(span->line == 30);
  CHECK(span->column == 5);
}

TEST_CASE("a lowering error spells the template instances of the program as written") {
  primec::Program program;
  primec::Definition instance;
  instance.fullPath = "/std/collections/vector/Vector__t4f2a";
  instance.instanceOfTemplate = "/std/collections/vector/Vector";
  instance.instanceTemplateArgs = {"i32"};
  program.definitions.push_back(instance);

  CHECK(primec::ir_lowerer::userFacingLoweringError(
            "cannot push to /std/collections/vector/Vector__t4f2a", program) ==
        "cannot push to Vector<i32>");
  CHECK(primec::ir_lowerer::userFacingLoweringError(
            "cannot push to /std/collections/vector/Vector__t77", program) ==
        "cannot push to /std/collections/vector/Vector__t77");
}

TEST_CASE("a missing semantic-product fact is reported as an internal compiler error") {
  const std::string marked = primec::ir_lowerer::markInternalLoweringError(
      "missing semantic-product callable summary: /main");
  CHECK(marked == "internal compiler error (please report it): missing semantic-product callable "
                  "summary: /main");
  // marking twice changes nothing
  CHECK(primec::ir_lowerer::markInternalLoweringError(marked) == marked);
}

TEST_CASE("a message about the program is not marked as an internal compiler error") {
  CHECK(primec::ir_lowerer::markInternalLoweringError("unknown call target: /missing") ==
        "unknown call target: /missing");
  CHECK(primec::ir_lowerer::markInternalLoweringError("") == "");
}

TEST_SUITE_END();
