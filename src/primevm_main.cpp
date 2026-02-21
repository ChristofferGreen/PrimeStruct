#include "primec/AstPrinter.h"
#include "primec/IncludeResolver.h"
#include "primec/IrLowerer.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/Options.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRegistry.h"
#include "primec/TransformRules.h"
#include "primec/Vm.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {
std::vector<std::string> defaultTextFilters() {
  return {"collections", "operators", "implicit-utf8", "implicit-i32"};
}

std::vector<std::string> defaultSemanticTransforms() {
  return {"single_type_to_return"};
}

std::vector<std::string> defaultEffectsList() {
  return {"io_out"};
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

bool shouldAutoIncludeStdlib(const std::string &source) {
  return source.find("import /math") != std::string::npos;
}

bool appendStdlibSources(const std::vector<std::string> &includePaths,
                         std::string &source,
                         std::string &error) {
  std::error_code ec;
  std::unordered_set<std::string> seen;
  bool appended = false;
  for (const auto &pathText : includePaths) {
    std::filesystem::path root(pathText);
    if (root.filename() != "stdlib") {
      continue;
    }
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
      continue;
    }
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root, ec)) {
      if (ec) {
        error = "failed to scan stdlib: " + root.string();
        return false;
      }
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      if (entry.path().extension() != ".prime") {
        continue;
      }
      std::filesystem::path absolute = std::filesystem::absolute(entry.path(), ec);
      if (ec) {
        absolute = entry.path();
      }
      std::string absoluteText = absolute.string();
      if (!seen.insert(absoluteText).second) {
        continue;
      }
      std::ifstream file(absoluteText);
      if (!file) {
        error = "failed to read stdlib file: " + absoluteText;
        return false;
      }
      std::ostringstream buffer;
      buffer << file.rdbuf();
      source.append("\n");
      source.append(buffer.str());
      appended = true;
    }
  }
  if (!appended) {
    error = "stdlib import requested but no stdlib files were found";
    return false;
  }
  return true;
}

void addDefaultStdlibInclude(const std::string &inputPath, std::vector<std::string> &includePaths) {
  if (inputPath.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::path resolved = std::filesystem::absolute(inputPath, ec);
  if (ec) {
    resolved = std::filesystem::path(inputPath);
  }
  std::filesystem::path dir = resolved;
  if (!std::filesystem::is_directory(dir, ec)) {
    dir = dir.parent_path();
  }
  for (std::filesystem::path current = dir; !current.empty(); current = current.parent_path()) {
    std::filesystem::path candidate = current / "stdlib";
    if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
      std::filesystem::path absoluteCandidate = std::filesystem::absolute(candidate, ec);
      std::string candidateText = absoluteCandidate.string();
      for (const auto &path : includePaths) {
        std::filesystem::path existing = std::filesystem::absolute(path, ec);
        if (!ec && std::filesystem::equivalent(existing, absoluteCandidate, ec)) {
          return;
        }
        if (path == candidateText) {
          return;
        }
      }
      includePaths.push_back(candidateText);
      return;
    }
    if (current == current.root_path()) {
      break;
    }
  }
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

void addUniqueTransform(std::vector<std::string> &list, const std::string &name) {
  for (const auto &existing : list) {
    if (existing == name) {
      return;
    }
  }
  list.push_back(name);
}

bool parseTransformListForPhase(const std::string &text,
                                const std::vector<std::string> &defaults,
                                bool expectText,
                                std::vector<std::string> &out,
                                std::string &error) {
  out.clear();
  size_t pos = 0;
  auto nextToken = [&](std::string &token) -> bool {
    while (pos < text.size()) {
      char c = text[pos];
      if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';') {
        ++pos;
        continue;
      }
      break;
    }
    if (pos >= text.size()) {
      return false;
    }
    size_t start = pos;
    while (pos < text.size()) {
      char c = text[pos];
      if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';') {
        break;
      }
      ++pos;
    }
    token = text.substr(start, pos - start);
    return true;
  };
  std::string token;
  while (nextToken(token)) {
    if (!token.empty()) {
      if (token == "default") {
        for (const auto &name : defaults) {
          addUniqueTransform(out, name);
        }
      } else if (token == "none") {
        out.clear();
      } else {
        const bool isText = primec::isTextTransformName(token);
        const bool isSemantic = primec::isSemanticTransformName(token);
        if (!isText && !isSemantic) {
          error = "unknown transform: " + token;
          return false;
        }
        if (expectText) {
          if (!isText) {
            error = "unsupported text transform: " + token;
            return false;
          }
        } else {
          if (!isSemantic) {
            error = "unsupported semantic transform: " + token;
            return false;
          }
        }
        addUniqueTransform(out, token);
      }
    }
  }
  return true;
}

bool parseTransformListAuto(const std::string &text,
                            std::vector<std::string> &textOut,
                            std::vector<std::string> &semanticOut,
                            std::string &error) {
  textOut.clear();
  semanticOut.clear();
  size_t pos = 0;
  auto nextToken = [&](std::string &token) -> bool {
    while (pos < text.size()) {
      char c = text[pos];
      if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';') {
        ++pos;
        continue;
      }
      break;
    }
    if (pos >= text.size()) {
      return false;
    }
    size_t start = pos;
    while (pos < text.size()) {
      char c = text[pos];
      if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';') {
        break;
      }
      ++pos;
    }
    token = text.substr(start, pos - start);
    return true;
  };
  std::string token;
  while (nextToken(token)) {
    if (!token.empty()) {
      if (token == "default") {
        for (const auto &name : defaultTextFilters()) {
          addUniqueTransform(textOut, name);
        }
        for (const auto &name : defaultSemanticTransforms()) {
          addUniqueTransform(semanticOut, name);
        }
      } else if (token == "none") {
        textOut.clear();
        semanticOut.clear();
      } else {
        const bool isText = primec::isTextTransformName(token);
        const bool isSemantic = primec::isSemanticTransformName(token);
        if (!isText && !isSemantic) {
          error = "unknown transform: " + token;
          return false;
        }
        if (isText && isSemantic) {
          error = "ambiguous transform name: " + token;
          return false;
        }
        if (isText) {
          addUniqueTransform(textOut, token);
        } else {
          addUniqueTransform(semanticOut, token);
        }
      }
    }
  }
  return true;
}

bool parseTransformRule(const std::string &text,
                        const std::vector<std::string> &defaults,
                        bool expectText,
                        primec::TextTransformRule &rule,
                        std::string &error) {
  size_t sep = text.find('=');
  if (sep == std::string::npos) {
    error = "transform rule must include '=': " + text;
    return false;
  }
  std::string pathSpec = trimWhitespace(text.substr(0, sep));
  std::string listSpec = trimWhitespace(text.substr(sep + 1));
  if (pathSpec.empty()) {
    error = "transform rule path cannot be empty";
    return false;
  }
  if (listSpec.empty()) {
    error = "transform rule list cannot be empty";
    return false;
  }
  constexpr std::string_view recurseSuffix = ":recurse";
  constexpr std::string_view recursiveSuffix = ":recursive";
  bool recursive = false;
  if (pathSpec.size() >= recurseSuffix.size() &&
      pathSpec.compare(pathSpec.size() - recurseSuffix.size(), recurseSuffix.size(), recurseSuffix) == 0) {
    recursive = true;
    pathSpec = trimWhitespace(pathSpec.substr(0, pathSpec.size() - recurseSuffix.size()));
  } else if (pathSpec.size() >= recursiveSuffix.size() &&
             pathSpec.compare(pathSpec.size() - recursiveSuffix.size(), recursiveSuffix.size(), recursiveSuffix) == 0) {
    recursive = true;
    pathSpec = trimWhitespace(pathSpec.substr(0, pathSpec.size() - recursiveSuffix.size()));
  }
  bool wildcard = false;
  if (pathSpec.size() >= 2 && pathSpec.compare(pathSpec.size() - 2, 2, "/*") == 0) {
    wildcard = true;
    pathSpec = trimWhitespace(pathSpec.substr(0, pathSpec.size() - 2));
  }
  if (!pathSpec.empty() && pathSpec[0] != '/') {
    error = "transform rule path must start with '/': " + pathSpec;
    return false;
  }
  if (pathSpec.find('*') != std::string::npos) {
    error = "transform rule '*' is only allowed as trailing '/*'";
    return false;
  }
  if (recursive && !wildcard) {
    error = "transform rule recursion requires a '/*' suffix";
    return false;
  }
  rule.path = pathSpec;
  rule.wildcard = wildcard;
  rule.recursive = recursive;
  if (!parseTransformListForPhase(listSpec, defaults, expectText, rule.transforms, error)) {
    return false;
  }
  return true;
}

bool parseTransformRules(const std::string &text,
                         const std::vector<std::string> &defaults,
                         bool expectText,
                         std::vector<primec::TextTransformRule> &out,
                         std::string &error) {
  out.clear();
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(';', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string token = trimWhitespace(text.substr(start, end - start));
    if (!token.empty()) {
      if (token == "none") {
        out.clear();
      } else {
        primec::TextTransformRule rule;
        if (!parseTransformRule(token, defaults, expectText, rule, error)) {
          return false;
        }
        out.push_back(std::move(rule));
      }
    }
    if (end == text.size()) {
      break;
    }
    start = end + 1;
  }
  return true;
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

bool parseArgs(int argc, char **argv, primec::Options &out, std::string &error) {
  bool noTransforms = false;
  bool noTextTransforms = false;
  bool noSemanticTransforms = false;
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
      if (!parseTransformListForPhase(argv[++i], defaultTextFilters(), true, out.textFilters, error)) {
        return false;
      }
    } else if (arg.rfind("--text-filters=", 0) == 0) {
      if (!parseTransformListForPhase(arg.substr(std::string("--text-filters=").size()),
                                      defaultTextFilters(),
                                      true,
                                      out.textFilters,
                                      error)) {
        return false;
      }
    } else if (arg == "--text-transforms" && i + 1 < argc) {
      if (!parseTransformListForPhase(argv[++i], defaultTextFilters(), true, out.textFilters, error)) {
        return false;
      }
    } else if (arg.rfind("--text-transforms=", 0) == 0) {
      if (!parseTransformListForPhase(arg.substr(std::string("--text-transforms=").size()),
                                      defaultTextFilters(),
                                      true,
                                      out.textFilters,
                                      error)) {
        return false;
      }
    } else if (arg == "--text-transform-rules" && i + 1 < argc) {
      if (!parseTransformRules(argv[++i], defaultTextFilters(), true, out.textTransformRules, error)) {
        return false;
      }
    } else if (arg.rfind("--text-transform-rules=", 0) == 0) {
      if (!parseTransformRules(arg.substr(std::string("--text-transform-rules=").size()),
                               defaultTextFilters(),
                               true,
                               out.textTransformRules,
                               error)) {
        return false;
      }
    } else if (arg == "--semantic-transform-rules" && i + 1 < argc) {
      if (!parseTransformRules(argv[++i], defaultSemanticTransforms(), false, out.semanticTransformRules, error)) {
        return false;
      }
    } else if (arg.rfind("--semantic-transform-rules=", 0) == 0) {
      if (!parseTransformRules(arg.substr(std::string("--semantic-transform-rules=").size()),
                               defaultSemanticTransforms(),
                               false,
                               out.semanticTransformRules,
                               error)) {
        return false;
      }
    } else if (arg == "--semantic-transforms" && i + 1 < argc) {
      if (!parseTransformListForPhase(argv[++i], defaultSemanticTransforms(), false, out.semanticTransforms, error)) {
        return false;
      }
    } else if (arg.rfind("--semantic-transforms=", 0) == 0) {
      if (!parseTransformListForPhase(arg.substr(std::string("--semantic-transforms=").size()),
                                      defaultSemanticTransforms(),
                                      false,
                                      out.semanticTransforms,
                                      error)) {
        return false;
      }
    } else if (arg == "--transform-list" && i + 1 < argc) {
      if (!parseTransformListAuto(argv[++i], out.textFilters, out.semanticTransforms, error)) {
        return false;
      }
    } else if (arg.rfind("--transform-list=", 0) == 0) {
      if (!parseTransformListAuto(arg.substr(std::string("--transform-list=").size()),
                                  out.textFilters,
                                  out.semanticTransforms,
                                  error)) {
        return false;
      }
    } else if (arg == "--no-transforms") {
      noTransforms = true;
    } else if (arg == "--no-text-transforms") {
      noTextTransforms = true;
    } else if (arg == "--no-semantic-transforms") {
      noSemanticTransforms = true;
    } else if (arg == "--default-effects" && i + 1 < argc) {
      auto effects = parseDefaultEffects(argv[++i]);
      out.defaultEffects = effects;
      out.entryDefaultEffects = effects;
    } else if (arg.rfind("--default-effects=", 0) == 0) {
      auto effects = parseDefaultEffects(arg.substr(std::string("--default-effects=").size()));
      out.defaultEffects = effects;
      out.entryDefaultEffects = effects;
    } else if (!arg.empty() && arg[0] == '-') {
      return false;
    } else {
      if (!out.inputPath.empty()) {
        return false;
      }
      out.inputPath = arg;
    }
  }
  if (noTransforms) {
    out.textFilters.clear();
    out.textTransformRules.clear();
    out.semanticTransforms.clear();
    out.semanticTransformRules.clear();
    out.allowEnvelopeTextTransforms = false;
  } else {
    if (noTextTransforms) {
      out.textFilters.clear();
      out.textTransformRules.clear();
      out.allowEnvelopeTextTransforms = false;
    }
    if (noSemanticTransforms) {
      out.semanticTransforms.clear();
      out.semanticTransformRules.clear();
    }
  }
  out.requireCanonicalSyntax = noTransforms;
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
  std::string argError;
  if (!parseArgs(argc, argv, options, argError)) {
    if (!argError.empty()) {
      std::cerr << "Argument error: " << argError << "\n";
    }
    std::cerr << "Usage: primevm <input.prime> [--entry /path] [--include-path <dir>] [--text-filters <list>] "
                 "[--text-transforms <list>] [--text-transform-rules <rules>] [--semantic-transform-rules <rules>] "
                 "[--semantic-transforms <list>] [--transform-list <list>] [--no-text-transforms] "
                 "[--no-semantic-transforms] [--no-transforms] "
                 "[--default-effects <list>] [--dump-stage pre_ast|ast|ir] "
                 "[-- <program args...>]\n";
    return 2;
  }
  addDefaultStdlibInclude(options.inputPath, options.includePaths);

  std::string source;
  std::string error;
  primec::IncludeResolver includeResolver;
  if (!includeResolver.expandIncludes(options.inputPath, source, error, options.includePaths)) {
    std::cerr << "Include error: " << error << "\n";
    return 2;
  }
  if (shouldAutoIncludeStdlib(source)) {
    if (!appendStdlibSources(options.includePaths, source, error)) {
      std::cerr << "Include error: " << error << "\n";
      return 2;
    }
  }

  primec::TextFilterPipeline textPipeline;
  primec::TextFilterOptions textOptions;
  textOptions.enabledFilters = options.textFilters;
  textOptions.rules = options.textTransformRules;
  textOptions.allowEnvelopeTransforms = options.allowEnvelopeTextTransforms;
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
  primec::Parser parser(lexer.tokenize(), !options.requireCanonicalSyntax);
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

  if (!options.semanticTransformRules.empty()) {
    primec::applySemanticTransformRules(program, options.semanticTransformRules);
  }

  primec::Semantics semantics;
  if (!semantics.validate(program,
                          options.entryPath,
                          error,
                          options.defaultEffects,
                          options.entryDefaultEffects,
                          options.semanticTransforms)) {
    std::cerr << "Semantic error: " << error << "\n";
    return 2;
  }

  primec::IrLowerer lowerer;
  primec::IrModule ir;
  if (!lowerer.lower(program,
                     options.entryPath,
                     options.defaultEffects,
                     options.entryDefaultEffects,
                     ir,
                     error)) {
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
