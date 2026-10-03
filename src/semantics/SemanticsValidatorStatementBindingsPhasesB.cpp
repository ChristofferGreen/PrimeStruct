#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_set>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorStatementBindingsState.h"
#include "SemanticsValidatorStatementBindingsReferenceState.h"

namespace primec::semantics {
using namespace statementBindingsHelpers;

PhaseStatus SemanticsValidator::validateBindingPhase5([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, ValidateBindingState &st) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] const Expr &initializer = *st.initializer;
  [[maybe_unused]] auto &resolveNamedBinding = st.resolveNamedBinding;
  [[maybe_unused]] auto &resolvePointerRoot = st.resolvePointerRoot;
  if (info.typeName == "Reference") {
    ValidateBindingReferenceState st2;
    if (validateBindingReferencePhase1(params, locals, stmt, namespacePrefix, handled, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (validateBindingReferencePhase2(params, locals, stmt, namespacePrefix, handled, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (validateBindingReferencePhase3(params, locals, stmt, namespacePrefix, handled, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (validateBindingReferencePhase4(params, locals, stmt, namespacePrefix, handled, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (validateBindingReferencePhase5(params, locals, stmt, namespacePrefix, handled, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (validateBindingReferencePhase6(params, locals, stmt, namespacePrefix, handled, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
  }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
