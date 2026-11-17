#pragma once

#include "primec/pipeline/BuildGraphJson.h"

#include <string>
#include <vector>

namespace primec {

// Build options (docs/BuildSystemPlan.md section 5): `primec build --option name=value` and the options a script
// declares with `Build.option` (/std/build).

// One `--option name=value` of the command line.
struct BuildOptionRequest {
  std::string name;
  std::string value;
};

// Splits `text` (the argument of `--option`) at its first `=`. The name is a letter or `_` followed by letters, digits,
// `_`, `-` and `.`; the value may be empty and may contain `=`. Returns false and sets `error` otherwise.
bool parseBuildOptionArgument(const std::string &text,
                              BuildOptionRequest &request,
                              std::string &error);

// Sorts `requests` by name (so the command line order does not matter) and reports a name given twice. Returns false and
// sets `error` for a duplicate.
bool normalizeBuildOptions(std::vector<BuildOptionRequest> &requests, std::string &error);

// The `name=value` program arguments for `requests`, in order (the script reads them from argument 2 on).
std::vector<std::string>
buildOptionProgramArguments(const std::vector<BuildOptionRequest> &requests);

// The diagnostics for a graph export against the command line: each declared option whose value was not of its type,
// and, when `complete` (the script has no later stage that could still declare options), each requested option that no
// script declared. Empty when everything is fine. Every line starts with `primec build: ` and ends with a newline.
std::string checkBuildOptions(const std::vector<BuildOptionRequest> &requests,
                              const std::vector<BuildOption> &declared,
                              bool complete);

// `--list-options`: one line per option, `name: type, default <default> - description` (the description is left out
// when empty), sorted by name; `no options` when nothing is declared.
std::string formatBuildOptionList(const std::vector<BuildOption> &declared);

} // namespace primec
