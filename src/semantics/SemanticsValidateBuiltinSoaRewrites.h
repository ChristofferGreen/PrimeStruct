#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec {

bool rewriteBuiltinSoaConversionMethods(Program &program, std::string &error);
bool rewriteBuiltinSoaToAosCalls(Program &program, std::string &error);
bool rewriteBuiltinSoaAccessCalls(Program &program, std::string &error);
bool rewriteBuiltinSoaCountCalls(Program &program, std::string &error);
bool rewriteBuiltinSoaMutatorCalls(Program &program, std::string &error);

} // namespace primec
