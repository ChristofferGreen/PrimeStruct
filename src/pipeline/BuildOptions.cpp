#include "primec/pipeline/BuildOptions.h"

#include <algorithm>
#include <cctype>

namespace primec {

namespace {

bool isNameStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isNameChar(char c) {
  return isNameStart(c) || std::isdigit(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '.';
}

} // namespace

bool parseBuildOptionArgument(const std::string &text,
                              BuildOptionRequest &request,
                              std::string &error) {
  const size_t equals = text.find('=');
  if (equals == std::string::npos) {
    error = "--option expects name=value, got \"" + text + "\"";
    return false;
  }
  const std::string name = text.substr(0, equals);
  const bool valid = !name.empty() && isNameStart(name[0]) &&
                     std::all_of(name.begin(), name.end(), [](char c) { return isNameChar(c); });
  if (!valid) {
    error = "--option name \"" + name +
            "\" is not a name (letters, digits, _, - and ., starting with a letter or _)";
    return false;
  }
  request.name = name;
  request.value = text.substr(equals + 1);
  return true;
}

bool normalizeBuildOptions(std::vector<BuildOptionRequest> &requests, std::string &error) {
  std::stable_sort(requests.begin(),
                   requests.end(),
                   [](const BuildOptionRequest &left, const BuildOptionRequest &right) {
                     return left.name < right.name;
                   });
  for (size_t i = 1; i < requests.size(); ++i) {
    if (requests[i].name == requests[i - 1].name) {
      error = "--option " + requests[i].name + " was given twice";
      return false;
    }
  }
  return true;
}

std::vector<std::string>
buildOptionProgramArguments(const std::vector<BuildOptionRequest> &requests) {
  std::vector<std::string> arguments;
  for (const BuildOptionRequest &request : requests) {
    arguments.push_back(request.name + "=" + request.value);
  }
  return arguments;
}

std::string checkBuildOptions(const std::vector<BuildOptionRequest> &requests,
                              const std::vector<BuildOption> &declared,
                              bool complete) {
  std::string diagnostics;
  for (const BuildOption &option : declared) {
    if (!option.error.empty()) {
      diagnostics +=
          "primec build: option " + option.name + " (" + option.type + ") " + option.error + "\n";
    }
  }
  if (!complete) {
    return diagnostics;
  }
  for (const BuildOptionRequest &request : requests) {
    const bool known =
        std::any_of(declared.begin(), declared.end(), [&](const BuildOption &option) {
          return option.name == request.name;
        });
    if (known) {
      continue;
    }
    std::string names;
    for (const BuildOption &option : declared) {
      names += (names.empty() ? "" : ", ") + option.name;
    }
    diagnostics +=
        "primec build: unknown option " + request.name + " (" +
        (names.empty() ? std::string("no script declares options") : "declared: " + names) + ")\n";
  }
  return diagnostics;
}

std::string formatBuildOptionList(const std::vector<BuildOption> &declared) {
  if (declared.empty()) {
    return "no options\n";
  }
  std::string text;
  for (const BuildOption &option : declared) {
    text += option.name + ": " + option.type + ", default " +
            (option.type == "String" ? "\"" + option.defaultText + "\"" : option.defaultText);
    if (!option.description.empty()) {
      text += " - " + option.description;
    }
    text += "\n";
  }
  return text;
}

} // namespace primec
