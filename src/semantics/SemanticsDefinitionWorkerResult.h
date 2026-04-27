#pragma once

#include "SemanticPublicationSurface.h"
#include "primec/Semantics.h"
#include "primec/SymbolInterner.h"

#include <cstdint>
#include <string>

namespace primec::semantics {

struct SemanticDefinitionWorkerCounters {
  uint64_t callsVisited = 0;
  uint64_t peakLocalMapSize = 0;
};

struct SemanticDefinitionWorkerResultBundle {
  uint32_t partitionKey = 0;
  bool ok = false;
  std::string error;
  SemanticDiagnosticInfo diagnostics;
  SemanticDefinitionWorkerCounters counters;
  SemanticPublicationSurface publicationFacts;
  WorkerSymbolInternerSnapshot publicationStringSnapshot;
};

} // namespace primec::semantics
