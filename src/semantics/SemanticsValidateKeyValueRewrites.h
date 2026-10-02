#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec {

bool rewriteBorrowedExperimentalKeyValueMethods(Program &program, std::string &error);
bool rewriteExperimentalKeyValueValueMethods(Program &program, std::string &error);
// Rewrites method-style key/value insert calls into their builtin forms.
bool rewriteBuiltinKeyValueInsertMethods(Program &program, std::string &error);

} // namespace primec
