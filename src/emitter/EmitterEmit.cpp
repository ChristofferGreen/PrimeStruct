#include "primec/Emitter.h"
#include "primec/Ir.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "EmitterHelpers.h"
#include "EmitterEmitSetupLifecycleHelperStep.h"
#include "EmitterEmitSetupMathImport.h"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>


namespace primec {
using namespace emitter;

#include "EmitterEmitSetup.h"
#include "EmitterEmitSetupReturnInference.h"
#include "EmitterEmitPrelude.h"
#include "EmitterEmitBody.h"

} // namespace primec
