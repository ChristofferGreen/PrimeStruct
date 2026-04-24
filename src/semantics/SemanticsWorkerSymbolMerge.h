#pragma once

#include "SemanticPublicationSurface.h"
#include "primec/SemanticsDefinitionPrepass.h"

namespace primec::semantics {

void appendSemanticPublicationStringOrigins(
    SymbolInterner &interner,
    const Program &program,
    const DefinitionPrepassSnapshot &definitionPrepassSnapshot,
    const std::vector<CollectedCallableSummaryEntry> &callableSummaries,
    const std::vector<OnErrorSnapshotEntry> &onErrorFacts);

} // namespace primec::semantics
