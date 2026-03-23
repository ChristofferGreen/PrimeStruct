#pragma once

template <typename RewriteCurrentFn>
void rewriteExperimentalConstructorReturnTree(Expr &candidate, RewriteCurrentFn &&rewriteCurrent) {
  rewriteCurrent(candidate);

  for (auto &arg : candidate.args) {
    if (arg.kind == Expr::Kind::Call) {
      rewriteExperimentalConstructorReturnTree(arg, rewriteCurrent);
    }
  }

  bool sawExplicitReturn = false;
  size_t implicitReturnIndex = candidate.bodyArguments.size();
  for (size_t argIndex = 0; argIndex < candidate.bodyArguments.size(); ++argIndex) {
    if (isReturnCall(candidate.bodyArguments[argIndex])) {
      sawExplicitReturn = true;
      break;
    }
    if (!candidate.bodyArguments[argIndex].isBinding) {
      implicitReturnIndex = argIndex;
    }
  }

  for (size_t argIndex = 0; argIndex < candidate.bodyArguments.size(); ++argIndex) {
    Expr &arg = candidate.bodyArguments[argIndex];
    if (isReturnCall(arg) || (!sawExplicitReturn && argIndex == implicitReturnIndex)) {
      rewriteExperimentalConstructorReturnTree(arg, rewriteCurrent);
    }
  }
}
