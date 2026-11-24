#include "IrLowererRealCallBlockerText.h"

#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

#include <unordered_set>

namespace primec::ir_lowerer {

namespace {

// The text of the `return<T>` transform, or empty when the definition has none.
std::string returnTypeText(const Definition &def) {
  for (const Transform &transform : def.transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1) {
      return trimTemplateTypeText(transform.templateArgs.front());
    }
  }
  return {};
}

void collectVectorReturnForms(const Expr &expr,
                              std::unordered_set<std::string> &ownedVectors,
                              bool &allReturnsOwned) {
  if (expr.isLambda) {
    return;
  }
  if (expr.isBinding && expr.args.size() == 1) {
    bool isMutVector = false;
    bool isVector = false;
    for (const Transform &transform : expr.transforms) {
      isMutVector = isMutVector || transform.name == "mut";
      isVector = isVector || (transform.name == "vector" && transform.templateArgs.size() == 1);
    }
    if (isVector && isMutVector) {
      ownedVectors.insert(expr.name);
    }
  }
  if (expr.kind == Expr::Kind::Call && expr.name == "return") {
    if (expr.args.size() != 1 || expr.args.front().kind != Expr::Kind::Name ||
        ownedVectors.count(expr.args.front().name) == 0) {
      allReturnsOwned = false;
    }
  }
  for (const Expr &arg : expr.args) {
    collectVectorReturnForms(arg, ownedVectors, allReturnsOwned);
  }
  for (const Expr &bodyArg : expr.bodyArguments) {
    collectVectorReturnForms(bodyArg, ownedVectors, allReturnsOwned);
  }
}

} // namespace

bool vectorResultReturnsOwnedLocals(const Definition &def) {
  std::unordered_set<std::string> ownedVectors;
  bool allReturnsOwned = true;
  for (const Expr &stmt : def.statements) {
    collectVectorReturnForms(stmt, ownedVectors, allReturnsOwned);
  }
  return allReturnsOwned;
}

const char *addressAbiBlockerCode(AddressAbiBlocker blocker) {
  switch (blocker) {
  case AddressAbiBlocker::None:
    return "";
  case AddressAbiBlocker::HostDefinition:
    return "host-definition";
  case AddressAbiBlocker::StructDefinition:
    return "struct-definition";
  case AddressAbiBlocker::SumDefinition:
    return "sum-definition";
  case AddressAbiBlocker::ComputeDefinition:
    return "compute-definition";
  case AddressAbiBlocker::OnErrorHandler:
    return "on-error-handler";
  case AddressAbiBlocker::RequireContract:
    return "require-contract";
  case AddressAbiBlocker::CollectionsHelper:
    return "collections-helper";
  case AddressAbiBlocker::LifecycleHelper:
    return "lifecycle-helper";
  case AddressAbiBlocker::ReceiverStruct:
    return "receiver-struct";
  case AddressAbiBlocker::ClosureReferenceCapture:
    return "closure-reference-capture";
  case AddressAbiBlocker::Parameter:
    return "parameter-type";
  case AddressAbiBlocker::SumParameter:
    return "sum-parameter";
  case AddressAbiBlocker::CollectionResult:
    return "collection-result";
  case AddressAbiBlocker::VectorResultNotOwned:
    return "vector-result-not-owned";
  case AddressAbiBlocker::UnsupportedResult:
    return "unsupported-result";
  case AddressAbiBlocker::UsesTry:
    return "uses-try";
  case AddressAbiBlocker::GeneratedTailValue:
    return "generated-tail-value";
  }
  return "";
}

std::string describeAddressAbiBlocker(AddressAbiBlocker blocker, const Definition &def) {
  switch (blocker) {
  case AddressAbiBlocker::None:
    return {};
  case AddressAbiBlocker::HostDefinition:
    return "it is a `host` function, which is lowered at each call site";
  case AddressAbiBlocker::StructDefinition:
    return "it is a `struct` definition";
  case AddressAbiBlocker::SumDefinition:
    return "it is a `sum` definition";
  case AddressAbiBlocker::ComputeDefinition:
    return "it is a `compute` function, which runs on a GPU";
  case AddressAbiBlocker::OnErrorHandler:
    return "it has an `on_error` handler";
  case AddressAbiBlocker::RequireContract:
    return "it has a `require` contract";
  case AddressAbiBlocker::CollectionsHelper:
    return "it is a helper of the canonical collections, which are expanded inline";
  case AddressAbiBlocker::LifecycleHelper:
    return "it is a `Create`, `Destroy`, `Copy` or `Move` helper, which callers expand where a "
           "value is made, "
           "copied or destroyed";
  case AddressAbiBlocker::ReceiverStruct:
    return "its receiver is a struct that holds a collection, which the address ABI does not pass";
  case AddressAbiBlocker::ClosureReferenceCapture:
    return "it is the `call` of a closure that captures a place by `Reference` or `Pointer`, so it "
           "runs in the "
           "frame that made the closure";
  case AddressAbiBlocker::Parameter:
    return "a parameter has a type the address ABI does not pass";
  case AddressAbiBlocker::SumParameter:
    return "a parameter is a sum, which a real call does not take: a function with a sum parameter "
           "stays inline";
  case AddressAbiBlocker::CollectionResult:
    return "its result is a struct of the canonical collections, which a real call does not return";
  case AddressAbiBlocker::VectorResultNotOwned:
    return "its result `" + returnTypeText(def) +
           "` is not always a `mut` vector local the function built, which a real call returns by "
           "copying its record: a function that returns a parameter, a field or another "
           "expression stays inline";
  case AddressAbiBlocker::UnsupportedResult: {
    const std::string result = returnTypeText(def);
    return "its result " + (result.empty() ? std::string("type") : "`" + result + "`") +
           " is not one a real call returns: a scalar, `void`, `never`, a `string`, a `Pointer`, a "
           "struct and, for a function of a library, a sum, a packed `Result` and a `vector<T>` "
           "are; a `Reference` is not";
  }
  case AddressAbiBlocker::UsesTry:
    return "its body uses `try`";
  case AddressAbiBlocker::GeneratedTailValue:
    return "its body ends in a generated tail value instead of a `return` statement";
  }
  return {};
}

} // namespace primec::ir_lowerer
