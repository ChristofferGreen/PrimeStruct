#pragma once

#include <cstddef>

namespace primec::semantics {

// Number of definition validation worker threads (SemanticsValidator with a
// definition worker count above 1) that have started and not yet finished
// destroying their thread_local state. It is 0 whenever no validation is
// running: every worker is joined before validation returns.
std::size_t liveDefinitionValidationWorkerThreadsForTesting();

} // namespace primec::semantics
