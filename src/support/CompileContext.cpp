#include "primec/support/CompileContext.h"

namespace primec {

namespace {
thread_local CompileContext *tls_currentContext = nullptr;
} // namespace

CompileContext &CompileContext::current() {
  if (tls_currentContext != nullptr) {
    return *tls_currentContext;
  }
  // Per-thread default (removed at the end of the TODO-5359/5360 migration).
  thread_local CompileContext defaultContext;
  return defaultContext;
}

CompileContext::Scope::Scope(CompileContext &context) : previous_(tls_currentContext) {
  tls_currentContext = &context;
}

CompileContext::Scope::~Scope() { tls_currentContext = previous_; }

} // namespace primec
