#include "primec/TextFilterPipeline.h"

#include "TextFilterPipelineInternal.h"

namespace primec {

bool TextFilterPipeline::apply(const std::string &input,
                               std::string &output,
                               std::string &error,
                               const TextFilterOptions &options) const {
  output = input;
  error.clear();
  if (options.enabledFilters.empty() && options.rules.empty()) {
    if (!options.allowEnvelopeTransforms) {
      return true;
    }
  }
  return applyPerEnvelope(
      input, output, error, options.enabledFilters, options.rules, false, "", "", options.enabledFilters);
}

} // namespace primec
