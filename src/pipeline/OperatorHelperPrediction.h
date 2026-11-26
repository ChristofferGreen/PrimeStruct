#pragma once

#include "primec/ast/Ast.h"

#include <string>
#include <vector>

namespace primec {

// A cheap syntactic estimate of the `/std/math` operator helpers a parsed program's operator sites lower to
// (Program::operatorRewriteDemand is the exact answer, known only after validation). It follows the math type of
// parameters, typed bindings, brace constructors and nested operators, and names the helper of each operator call
// with such an operand. The result is sorted and without repeats. A site it cannot type is left out: the validator
// still finds it, and the pipeline then compiles once more with the helper taken.
std::vector<std::string> predictOperatorRewriteDemand(const Program &program);

} // namespace primec
