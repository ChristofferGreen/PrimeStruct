#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec {

bool rewriteExperimentalSoaFieldViewIndexes(Program &program, std::string &error);
bool rewriteExperimentalSoaFieldViewHelpers(Program &program, std::string &error);
bool rewriteExperimentalSoaFieldViewCarrierIndexes(Program &program, std::string &error);
bool rewriteExperimentalSoaFieldViewAssignTargets(Program &program, std::string &error);

} // namespace primec
