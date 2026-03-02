#pragma once

#include <string>

namespace primec {
struct Options;

enum class OptionsParserMode {
  Primec,
  Primevm,
};

bool parseOptions(int argc, char **argv, OptionsParserMode mode, Options &out, std::string &error);

} // namespace primec
