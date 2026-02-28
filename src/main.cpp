#include "primec/CompilePipeline.h"
#include "primec/Emitter.h"
#include "primec/GlslEmitter.h"
#include "primec/IrLowerer.h"
#include "primec/IrSerializer.h"
#include "primec/NativeEmitter.h"
#include "primec/Options.h"
#include "primec/TransformRegistry.h"
#include "primec/TransformRules.h"
#include "primec/Vm.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>

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

bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg) {
  base.clear();
  arg.clear();
  const size_t open = text.find('<');
  if (open == std::string::npos || open == 0 || text.back() != '>') {
    return false;
  }
  base = text.substr(0, open);
  int depth = 0;
  size_t start = open + 1;
  for (size_t i = start; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth == 0) {
        if (i + 1 != text.size()) {
          return false;
        }
        arg = text.substr(start, i - start);
        return true;
      }
      --depth;
    }
  }
  return false;
}

bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  auto pushSegment = [&](size_t end) {
    size_t segStart = start;
    while (segStart < end && std::isspace(static_cast<unsigned char>(text[segStart]))) {
      ++segStart;
    }
    size_t segEnd = end;
    while (segEnd > segStart && std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
      --segEnd;
    }
    out.push_back(text.substr(segStart, segEnd - segStart));
  };
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (c == ',' && depth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  for (const auto &seg : out) {
    if (seg.empty()) {
      return false;
    }
  }
  return !out.empty();
}

std::optional<std::string> findSoftwareNumericType(const std::string &typeName) {
  auto isSoftwareNumericName = [](const std::string &name) {
    return name == "integer" || name == "decimal" || name == "complex";
  };
  if (typeName.empty()) {
    return std::nullopt;
  }
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg)) {
    if (isSoftwareNumericName(typeName)) {
      return typeName;
    }
    return std::nullopt;
  }
  if (isSoftwareNumericName(base)) {
    return base;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(arg, args)) {
    return std::nullopt;
  }
  for (const auto &nested : args) {
    if (auto found = findSoftwareNumericType(nested)) {
      return found;
    }
  }
  return std::nullopt;
}

std::optional<std::string> scanSoftwareNumericTypes(const primec::Program &program) {
  auto scanTransforms = [&](const std::vector<primec::Transform> &transforms) -> std::optional<std::string> {
    for (const auto &transform : transforms) {
      if (auto found = findSoftwareNumericType(transform.name)) {
        return found;
      }
      for (const auto &arg : transform.templateArgs) {
        if (auto found = findSoftwareNumericType(arg)) {
          return found;
        }
      }
    }
    return std::nullopt;
  };
  std::function<std::optional<std::string>(const primec::Expr &)> scanExpr;
  scanExpr = [&](const primec::Expr &expr) -> std::optional<std::string> {
    if (auto found = scanTransforms(expr.transforms)) {
      return found;
    }
    for (const auto &arg : expr.templateArgs) {
      if (auto found = findSoftwareNumericType(arg)) {
        return found;
      }
    }
    for (const auto &arg : expr.args) {
      if (auto found = scanExpr(arg)) {
        return found;
      }
    }
    for (const auto &arg : expr.bodyArguments) {
      if (auto found = scanExpr(arg)) {
        return found;
      }
    }
    return std::nullopt;
  };
  for (const auto &def : program.definitions) {
    if (auto found = scanTransforms(def.transforms)) {
      return found;
    }
    for (const auto &param : def.parameters) {
      if (auto found = scanExpr(param)) {
        return found;
      }
    }
    for (const auto &stmt : def.statements) {
      if (auto found = scanExpr(stmt)) {
        return found;
      }
    }
    if (def.returnExpr.has_value()) {
      if (auto found = scanExpr(*def.returnExpr)) {
        return found;
      }
    }
  }
  for (const auto &exec : program.executions) {
    if (auto found = scanTransforms(exec.transforms)) {
      return found;
    }
    for (const auto &arg : exec.arguments) {
      if (auto found = scanExpr(arg)) {
        return found;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      if (auto found = scanExpr(arg)) {
        return found;
      }
    }
  }
  return std::nullopt;
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

bool hasPathPrefix(const std::filesystem::path &path, const std::filesystem::path &prefix) {
  auto pathIter = path.begin();
  auto prefixIter = prefix.begin();
  for (; prefixIter != prefix.end(); ++prefixIter, ++pathIter) {
    if (pathIter == path.end() || *pathIter != *prefixIter) {
      return false;
    }
  }
  return true;
}

std::filesystem::path resolveOutputPath(const primec::Options &options) {
  std::filesystem::path output(options.outputPath);
  if (output.is_absolute()) {
    return output;
  }
  if (options.outDir.empty() || options.outDir == ".") {
    return output;
  }
  std::filesystem::path outDir(options.outDir);
  if (hasPathPrefix(output.lexically_normal(), outDir.lexically_normal())) {
    return output;
  }
  return outDir / output;
}

bool ensureOutputDirectory(const std::filesystem::path &outputPath, std::string &error) {
  std::filesystem::path parent = outputPath.parent_path();
  if (parent.empty()) {
    return true;
  }
  std::error_code ec;
  std::filesystem::create_directories(parent, ec);
  if (ec) {
    error = "failed to create output directory: " + parent.string();
    return false;
  }
  return true;
}

std::string transformAvailability(const primec::TransformInfo &info) {
  std::string availability;
  if (info.availableInPrimec) {
    availability = "primec";
  }
  if (info.availableInPrimevm) {
    if (!availability.empty()) {
      availability += ",";
    }
    availability += "primevm";
  }
  if (availability.empty()) {
    return "none";
  }
  return availability;
}

void printTransformList(std::ostream &out) {
  out << "name\tphase\taliases\tavailability\n";
  for (const auto &transform : primec::listTransforms()) {
    out << transform.name << '\t' << primec::transformPhaseName(transform.phase) << '\t' << transform.aliases << '\t'
        << transformAvailability(transform) << '\n';
  }
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
    if (arg == "--emit=cpp" || arg == "--emit=exe" || arg == "--emit=native" || arg == "--emit=ir" ||
        arg == "--emit=vm" || arg == "--emit=glsl" || arg == "--emit=spirv") {
      out.emitKind = arg.substr(std::string("--emit=").size());
    } else if (arg == "--list-transforms") {
      out.listTransforms = true;
    } else if (arg == "-o" && i + 1 < argc) {
      out.outputPath = argv[++i];
    } else if (arg == "--entry" && i + 1 < argc) {
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
    } else if (arg == "--out-dir" && i + 1 < argc) {
      out.outDir = argv[++i];
    } else if (arg.rfind("--out-dir=", 0) == 0) {
      out.outDir = arg.substr(std::string("--out-dir=").size());
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
  if (out.listTransforms) {
    return true;
  }
  if (out.inputPath.empty()) {
    return false;
  }
  if (!out.dumpStage.empty()) {
    return true;
  }
  if (out.emitKind.empty()) {
    out.emitKind = "native";
  }
  if (out.outputPath.empty()) {
    std::filesystem::path inputPath(out.inputPath);
    std::string stem = inputPath.stem().string();
    if (stem.empty()) {
      stem = inputPath.filename().string();
    }
    if (out.emitKind == "cpp") {
      out.outputPath = stem + ".cpp";
    } else if (out.emitKind == "ir") {
      out.outputPath = stem + ".psir";
    } else if (out.emitKind == "glsl") {
      out.outputPath = stem + ".glsl";
    } else if (out.emitKind == "spirv") {
      out.outputPath = stem + ".spv";
    } else {
      out.outputPath = stem;
    }
  }
  return !out.emitKind.empty() && !out.outputPath.empty();
}

bool writeFile(const std::string &path, const std::string &contents) {
  std::ofstream file(path);
  if (!file) {
    return false;
  }
  file << contents;
  return file.good();
}

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &contents) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(reinterpret_cast<const char *>(contents.data()), static_cast<std::streamsize>(contents.size()));
  return file.good();
}

bool runCommand(const std::string &command) {
  return std::system(command.c_str()) == 0;
}

std::string injectComputeLayout(const std::string &glslSource) {
  const std::string layoutLine = "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
  size_t versionPos = glslSource.find("#version");
  if (versionPos == std::string::npos) {
    return "#version 450\n" + layoutLine + glslSource;
  }
  size_t lineEnd = glslSource.find('\n', versionPos);
  if (lineEnd == std::string::npos) {
    return glslSource + "\n" + layoutLine;
  }
  return glslSource.substr(0, lineEnd + 1) + layoutLine + glslSource.substr(lineEnd + 1);
}

bool findSpirvCompiler(std::string &toolName) {
  if (runCommand("glslangValidator -v > /dev/null 2>&1")) {
    toolName = "glslangValidator";
    return true;
  }
  if (runCommand("glslc --version > /dev/null 2>&1")) {
    toolName = "glslc";
    return true;
  }
  return false;
}

std::string quotePath(const std::filesystem::path &path) {
  std::string text = path.string();
  std::string quoted = "\"";
  for (char c : text) {
    if (c == '\"') {
      quoted += "\\\"";
    } else {
      quoted += c;
    }
  }
  quoted += "\"";
  return quoted;
}

} // namespace

int main(int argc, char **argv) {
  primec::Options options;
  std::string argError;
  if (!parseArgs(argc, argv, options, argError)) {
    if (!argError.empty()) {
      std::cerr << "Argument error: " << argError << "\n";
    }
    std::cerr << "Usage: primec [--emit=cpp|exe|native|ir|vm|glsl|spirv] <input.prime> [-o <output>] [--entry /path] "
                 "[--include-path <dir>] [--text-filters <list>] [--text-transforms <list>] "
                 "[--text-transform-rules <rules>] [--semantic-transform-rules <rules>] "
                 "[--semantic-transforms <list>] [--transform-list <list>] [--no-text-transforms] "
                 "[--no-semantic-transforms] [--no-transforms] [--out-dir <dir>] "
                 "[--list-transforms] "
                 "[--default-effects <list>] [--dump-stage pre_ast|ast|ir] [-- <program args...>]\n";
    return 2;
  }
  if (options.listTransforms) {
    printTransformList(std::cout);
    return 0;
  }
  std::string error;
  primec::addDefaultStdlibInclude(options.inputPath, options.includePaths);

  primec::CompilePipelineOutput pipelineOutput;
  primec::CompilePipelineErrorStage pipelineError = primec::CompilePipelineErrorStage::None;
  if (!primec::runCompilePipeline(options, pipelineOutput, pipelineError, error)) {
    if (pipelineError == primec::CompilePipelineErrorStage::Include) {
      std::cerr << "Include error: " << error << "\n";
    } else if (pipelineError == primec::CompilePipelineErrorStage::Transform) {
      std::cerr << "Transform error: " << error << "\n";
    } else if (pipelineError == primec::CompilePipelineErrorStage::Parse) {
      std::cerr << "Parse error: " << error << "\n";
    } else if (pipelineError == primec::CompilePipelineErrorStage::UnsupportedDumpStage) {
      std::cerr << "Unsupported dump stage: " << error << "\n";
    } else if (pipelineError == primec::CompilePipelineErrorStage::Semantic) {
      std::cerr << "Semantic error: " << error << "\n";
    } else {
      std::cerr << "Compile pipeline error: " << error << "\n";
    }
    return 2;
  }
  if (pipelineOutput.hasDumpOutput) {
    std::cout << pipelineOutput.dumpOutput;
    return 0;
  }

  primec::Program &program = pipelineOutput.program;

  if (options.emitKind == "vm") {
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

  if (!options.outputPath.empty()) {
    std::filesystem::path resolved = resolveOutputPath(options);
    options.outputPath = resolved.string();
    if (!ensureOutputDirectory(resolved, error)) {
      std::cerr << "Output error: " << error << "\n";
      return 2;
    }
  }

  if (options.emitKind == "native") {
    primec::IrLowerer lowerer;
    primec::IrModule ir;
    if (!lowerer.lower(program,
                       options.entryPath,
                       options.defaultEffects,
                       options.entryDefaultEffects,
                       ir,
                       error)) {
      std::cerr << "Native lowering error: " << error << "\n";
      return 2;
    }
    primec::NativeEmitter nativeEmitter;
    if (!nativeEmitter.emitExecutable(ir, options.outputPath, error)) {
      std::cerr << "Native emit error: " << error << "\n";
      return 2;
    }
    return 0;
  }

  if (options.emitKind == "ir") {
    primec::IrLowerer lowerer;
    primec::IrModule ir;
    if (!lowerer.lower(program,
                       options.entryPath,
                       options.defaultEffects,
                       options.entryDefaultEffects,
                       ir,
                       error)) {
      std::cerr << "IR lowering error: " << error << "\n";
      return 2;
    }
    std::vector<uint8_t> data;
    if (!primec::serializeIr(ir, data, error)) {
      std::cerr << "IR serialize error: " << error << "\n";
      return 2;
    }
    if (!writeBinaryFile(options.outputPath, data)) {
      std::cerr << "Failed to write output: " << options.outputPath << "\n";
      return 2;
    }
    return 0;
  }

  if (options.emitKind == "glsl") {
    primec::GlslEmitter glslEmitter;
    std::string glslSource;
    if (!glslEmitter.emitSource(program, options.entryPath, glslSource, error)) {
      std::cerr << "GLSL emit error: " << error << "\n";
      return 2;
    }
    if (!writeFile(options.outputPath, glslSource)) {
      std::cerr << "Failed to write output: " << options.outputPath << "\n";
      return 2;
    }
    return 0;
  }

  if (options.emitKind == "spirv") {
    primec::GlslEmitter glslEmitter;
    std::string glslSource;
    if (!glslEmitter.emitSource(program, options.entryPath, glslSource, error)) {
      std::cerr << "GLSL emit error: " << error << "\n";
      return 2;
    }
    std::string spirvSource = injectComputeLayout(glslSource);
    std::string toolName;
    if (!findSpirvCompiler(toolName)) {
      std::cerr << "SPIR-V emit error: glslangValidator or glslc not found\n";
      return 2;
    }

    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / ("primec_spirv_" + std::to_string(timestamp) + ".comp");
    if (!writeFile(tempPath.string(), spirvSource)) {
      std::cerr << "Failed to write output: " << tempPath.string() << "\n";
      return 2;
    }

    std::string command;
    if (toolName == "glslangValidator") {
      command = "glslangValidator -V -S comp " + quotePath(tempPath) + " -o " + quotePath(options.outputPath);
    } else {
      command = "glslc -fshader-stage=compute " + quotePath(tempPath) + " -o " + quotePath(options.outputPath);
    }
    bool ok = runCommand(command);
    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
    if (!ok) {
      std::cerr << "SPIR-V emit error: tool invocation failed\n";
      return 2;
    }
    return 0;
  }

  if (auto softwareType = scanSoftwareNumericTypes(program)) {
    std::cerr << "C++ emit error: software numeric types are not supported: " << *softwareType << "\n";
    return 2;
  }

  primec::Emitter emitter;
  std::string cppSource = emitter.emitCpp(program, options.entryPath);

  if (options.emitKind == "cpp") {
    if (!writeFile(options.outputPath, cppSource)) {
      std::cerr << "Failed to write output: " << options.outputPath << "\n";
      return 2;
    }
    return 0;
  }

  std::filesystem::path outputPath(options.outputPath);
  std::filesystem::path cppPath = outputPath;
  cppPath.replace_extension(".cpp");
  if (!writeFile(cppPath.string(), cppSource)) {
    std::cerr << "Failed to write intermediate C++: " << cppPath.string() << "\n";
    return 2;
  }

  std::string command =
      "clang++ -std=c++23 -O2 " + quotePath(cppPath) + " -o " + quotePath(outputPath);
  if (!runCommand(command)) {
    std::cerr << "Failed to compile output executable\n";
    return 3;
  }

  return 0;
}
