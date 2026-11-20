#include "primec/support/TextFilterPipeline.h"

#include "TextFilterPipelineInternal.h"

#include <utility>

namespace primec {

namespace {

// Filters `input`; `origins`, when set, receives the source offset of every output byte.
bool applyTracked(const std::string &input,
                  std::string &output,
                  std::vector<std::uint32_t> *origins,
                  std::string &error,
                  const TextFilterOptions &options) {
  error.clear();
  Text result;
  if (options.enabledFilters.empty() && options.rules.empty() && !options.allowEnvelopeTransforms) {
    result = Text::fromSource(input, origins != nullptr);
  } else {
    const Text source = Text::fromSource(input, origins != nullptr);
    if (!applyPerEnvelope(source,
                          result,
                          error,
                          options.enabledFilters,
                          options.rules,
                          false,
                          "",
                          "",
                          options.enabledFilters)) {
      output = input;
      return false;
    }
  }
  if (origins != nullptr) {
    origins->resize(result.size());
    for (size_t i = 0; i < result.size(); ++i) {
      (*origins)[i] = result.originAt(i);
    }
  }
  output = std::move(static_cast<std::string &>(result));
  return true;
}

} // namespace

bool TextFilterPipeline::apply(const std::string &input,
                               std::string &output,
                               std::string &error,
                               const TextFilterOptions &options) const {
  return applyTracked(input, output, nullptr, error, options);
}

bool TextFilterPipeline::applyWithOrigins(const std::string &input,
                                          std::string &output,
                                          std::vector<std::uint32_t> &origins,
                                          std::string &error,
                                          const TextFilterOptions &options) const {
  return applyTracked(input, output, &origins, error, options);
}

} // namespace primec
