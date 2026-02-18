#include "primec/Emitter.h"

#include "EmitterHelpers.h"

#include <functional>
#include <sstream>
#include <unordered_map>
#include <utility>


namespace primec {
using namespace emitter;

std::string Emitter::toCppName(const std::string &fullPath) const {
  std::string name = "ps";
  for (char c : fullPath) {
    if (c == '/') {
      name += "_";
    } else {
      name += c;
    }
  }
  return name;
}

#include "EmitterExprLambda.h"
#include "EmitterExprControl.h"
#include "EmitterExprCalls.h"

} // namespace primec
