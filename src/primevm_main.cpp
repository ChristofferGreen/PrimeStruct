#include "primec/AstPrinter.h"
#include "primec/IncludeResolver.h"
#include "primec/IrLowerer.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/Options.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/TextFilterPipeline.h"
#include "primec/Vm.h"

#include <cctype>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::vector<std::string> defaultTextFilters() {
  return {"operators", "collections", "implicit-utf8"};
}

std::vector<std::string> defaultEffectsList() {
  return {"io_out", "io_err"};
}

std::string trimWhitespace(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

void replaceAll(std::string &text, std::string_view from, std::string_view to) {
  if (from.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

std::vector<std::string> parseTextFilters(const std::string &text) {
  std::vector<std::string> filters;
  size_t start = 0;
  auto addUnique = [&](const std::string &name) {
    for (const auto &existing : filters) {
      if (existing == name) {
        return;
      }
    }
    filters.push_back(name);
  };
  while (start <= text.size()) {
    size_t end = text.find(',', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string token = trimWhitespace(text.substr(start, end - start));
    if (!token.empty()) {
      if (token == "default") {
        for (const auto &name : defaultTextFilters()) {
          addUnique(name);
        }
      } else if (token == "none") {
        filters.clear();
      } else {
        addUnique(token);
      }
    }
    if (end == text.size()) {
      break;
    }
    start = end + 1;
  }
  return filters;
}

bool extractSingleTypeToReturn(std::vector<std::string> &filters) {
  bool enabled = false;
  auto it = filters.begin();
  while (it != filters.end()) {
    if (*it == "single_type_to_return") {
      enabled = true;
      it = filters.erase(it);
    } else {
      ++it;
    }
  }
  return enabled;
}

std::vector<std::string> parseDefaultEffects(const std::string &text) {
  std::vector<std::string> effects;
  size_t start = 0;
  auto addUnique = [&](const std::string &name) {
    for (const auto &existing : effects) {
      if (existing == name) {
        return;
      }
    }
    effects.push_back(name);
  };
  while (start <= text.size()) {
    size_t end = text.find(',', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string token = trimWhitespace(text.substr(start, end - start));
    if (!token.empty()) {
      if (token == "default") {
        for (const auto &name : defaultEffectsList()) {
          addUnique(name);
        }
      } else if (token == "none") {
        effects.clear();
      } else {
        addUnique(token);
      }
    }
    if (end == text.size()) {
      break;
    }
    start = end + 1;
  }
  return effects;
}

bool parseArgs(int argc, char **argv, primec::Options &out) {
  bool noTransforms = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--") {
      for (int j = i + 1; j < argc; ++j) {
        out.programArgs.push_back(argv[j]);
      }
      break;
    }
    if (arg == "--emit=vm") {
      continue;
    }
    if (arg == "--entry" && i + 1 < argc) {
      out.entryPath = argv[++i];
    } else if (arg.rfind("--entry=", 0) == 0) {
      out.entryPath = arg.substr(std::string("--entry=").size());
    } else if (arg == "--dump-stage" && i + 1 < argc) {
      out.dumpStage = argv[++i];
    } else if (arg.rfind("--dump-stage=", 0) == 0) {
      out.dumpStage = arg.substr(std::string("--dump-stage=").size());
    } else if (arg == "--include-path" && i + 1 < argc) {
      out.includePaths.push_back(argv[++i]);
    } else if (arg.rfind("--include-path=", 0) == 0) {
      out.includePaths.push_back(arg.substr(std::string("--include-path=").size()));
    } else if (arg == "-I" && i + 1 < argc) {
      out.includePaths.push_back(argv[++i]);
    } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
      out.includePaths.push_back(arg.substr(2));
    } else if (arg == "--text-filters" && i + 1 < argc) {
      out.textFilters = parseTextFilters(argv[++i]);
    } else if (arg.rfind("--text-filters=", 0) == 0) {
      out.textFilters = parseTextFilters(arg.substr(std::string("--text-filters=").size()));
    } else if (arg == "--transform-list" && i + 1 < argc) {
      out.textFilters = parseTextFilters(argv[++i]);
    } else if (arg.rfind("--transform-list=", 0) == 0) {
      out.textFilters = parseTextFilters(arg.substr(std::string("--transform-list=").size()));
    } else if (arg == "--no-transforms") {
      out.textFilters.clear();
      noTransforms = true;
    } else if (arg == "--default-effects" && i + 1 < argc) {
      out.defaultEffects = parseDefaultEffects(argv[++i]);
    } else if (arg.rfind("--default-effects=", 0) == 0) {
      out.defaultEffects = parseDefaultEffects(arg.substr(std::string("--default-effects=").size()));
    } else if (!arg.empty() && arg[0] == '-') {
      return false;
    } else {
      if (!out.inputPath.empty()) {
        return false;
      }
      out.inputPath = arg;
    }
  }
  out.singleTypeToReturn = extractSingleTypeToReturn(out.textFilters);
  if (noTransforms) {
    out.textFilters.clear();
    out.singleTypeToReturn = false;
  }
  if (out.entryPath.empty()) {
    out.entryPath = "/main";
  } else if (out.entryPath[0] != '/') {
    out.entryPath = "/" + out.entryPath;
  }
  return !out.inputPath.empty();
}
} // namespace

int main(int argc, char **argv) {
  primec::Options options;
  if (!parseArgs(argc, argv, options)) {
    std::cerr << "Usage: primevm <input.prime> [--entry /path] [--include-path <dir>] [--text-filters <list>] "
                 "[--transform-list <list>] [--no-transforms] [--default-effects <list>] "
                 "[--dump-stage pre_ast|ast|ir] "
                 "[-- <program args...>]\n";
    return 2;
  }

  std::string source;
  std::string error;
  primec::IncludeResolver includeResolver;
  if (!includeResolver.expandIncludes(options.inputPath, source, error, options.includePaths)) {
    std::cerr << "Include error: " << error << "\n";
    return 2;
  }

  primec::TextFilterPipeline textPipeline;
  primec::TextFilterOptions textOptions;
  textOptions.enabledFilters = options.textFilters;
  std::string filteredSource;
  if (!textPipeline.apply(source, filteredSource, error, textOptions)) {
    std::cerr << "Transform error: " << error << "\n";
    return 2;
  }

  if (!options.dumpStage.empty() && options.dumpStage == "pre_ast") {
    std::cout << filteredSource;
    return 0;
  }

  primec::Lexer lexer(filteredSource);
  primec::Parser parser(lexer.tokenize());
  parser.setSingleTypeToReturn(options.singleTypeToReturn);
  primec::Program program;
  if (!parser.parse(program, error)) {
    std::cerr << "Parse error: " << error << "\n";
    return 2;
  }

  if (!options.dumpStage.empty()) {
    if (options.dumpStage == "ast") {
      primec::AstPrinter printer;
      std::cout << printer.print(program);
      return 0;
    }
    if (options.dumpStage == "ir") {
      primec::IrPrinter printer;
      std::cout << printer.print(program);
      return 0;
    }
    std::cerr << "Unsupported dump stage: " << options.dumpStage << "\n";
    return 2;
  }

  primec::Semantics semantics;
  if (!semantics.validate(program, options.entryPath, error, options.defaultEffects)) {
    std::cerr << "Semantic error: " << error << "\n";
    return 2;
  }

  primec::IrLowerer lowerer;
  primec::IrModule ir;
  if (!lowerer.lower(program, options.entryPath, ir, error)) {
    std::string vmError = error;
    replaceAll(vmError, "native backend", "vm backend");
    std::cerr << "VM lowering error: " << vmError << "\n";
    return 2;
  }

  primec::Vm vm;
  std::vector<std::string_view> args;
  args.reserve(1 + options.programArgs.size());
  args.push_back(options.inputPath);
  for (const auto &arg : options.programArgs) {
    args.push_back(arg);
  }
  uint64_t result = 0;
  if (!vm.execute(ir, result, error, args)) {
    std::cerr << "VM error: " << error << "\n";
    return 3;
  }

  return static_cast<int>(static_cast<int32_t>(result));
}
