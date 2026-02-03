#include "primec/TextFilterPipeline.h"

namespace primec {

bool TextFilterPipeline::apply(const std::string &input, std::string &output, std::string &error) const {
  output = input;
  error.clear();
  return true;
}

} // namespace primec
