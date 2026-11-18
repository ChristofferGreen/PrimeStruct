#include "ImportedDeclarations.h"

#include "primec/semantics/ImportedDefinitions.h"
#include "primec/support/SourceLocationMapper.h"

#include <optional>

namespace primec {

bool checkImportedDeclarationSites(const Program &program,
                                   const ExpandedSource &source,
                                   std::string &error) {
  for (const Definition &definition : program.definitions) {
    if (!isImportedDefinition(definition)) {
      continue;
    }
    const std::optional<SourceUnitLocation> location =
        mapExpandedSourceLocation(source, definition.sourceLine, definition.sourceColumn);
    if (!location.has_value() || location->unitKind != SourceUnitKind::Library) {
      error = "imported is only valid in a library interface: " + definition.fullPath;
      return false;
    }
    // The module key of a library unit is `<library name>:<interface hash>`.
    const std::string library = location->moduleKey.substr(0, location->moduleKey.rfind(':'));
    // A marker without exactly one library name is a diagnostic of semantic validation.
    if (!importedLibraryName(definition).empty() && importedLibraryName(definition) != library) {
      error = "imported(" + importedLibraryName(definition) + ") in the interface of library " +
              library + ": " + definition.fullPath;
      return false;
    }
  }
  return true;
}

} // namespace primec
