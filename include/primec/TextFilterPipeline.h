#pragma once

#include <string>

namespace primec {

class TextFilterPipeline {
public:
  bool apply(const std::string &input, std::string &output, std::string &error) const;
};

} // namespace primec
