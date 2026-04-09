#include "primec/OptionsParser.h"

#include "primec/EmitKind.h"
#include "primec/Options.h"
#include "primec/TransformRegistry.h"

#include <cctype>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace primec {
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

bool parseDebugJsonSnapshotMode(std::string_view value, DebugJsonSnapshotMode &out) {
  if (value == "none") {
    out = DebugJsonSnapshotMode::None;
    return true;
  }
  if (value == "stop") {
    out = DebugJsonSnapshotMode::Stop;
    return true;
  }
  if (value == "all") {
    out = DebugJsonSnapshotMode::All;
    return true;
  }
  return false;
}

bool normalizeWasmProfile(const std::string &value, std::string &normalized) {
  if (value == "wasi" || value == "wasm-wasi") {
    normalized = "wasi";
    return true;
  }
  if (value == "browser" || value == "wasm-browser") {
    normalized = "browser";
    return true;
  }
  return false;
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
    if (token.empty()) {
      continue;
    }
    if (token == "default") {
      for (const auto &name : defaults) {
        addUniqueTransform(out, name);
      }
      continue;
    }
    if (token == "none") {
      out.clear();
      continue;
    }

    const bool isText = isTextTransformName(token);
    const bool isSemantic = isSemanticTransformName(token);
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
    if (token.empty()) {
      continue;
    }
    if (token == "default") {
      for (const auto &name : defaultTextFilters()) {
        addUniqueTransform(textOut, name);
      }
      for (const auto &name : defaultSemanticTransforms()) {
        addUniqueTransform(semanticOut, name);
      }
      continue;
    }
    if (token == "none") {
      textOut.clear();
      semanticOut.clear();
      continue;
    }

    const bool isText = isTextTransformName(token);
    const bool isSemantic = isSemanticTransformName(token);
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
  return true;
}

bool parseTransformRule(const std::string &text,
                        const std::vector<std::string> &defaults,
                        bool expectText,
                        TextTransformRule &rule,
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
                         std::vector<TextTransformRule> &out,
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
        TextTransformRule rule;
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

bool parseBenchmarkForceSemanticProduct(const std::string &text, std::optional<bool> &out, std::string &error) {
  const std::string normalized = trimWhitespace(text);
  if (normalized == "on" || normalized == "true" || normalized == "1") {
    out = true;
    return true;
  }
  if (normalized == "off" || normalized == "false" || normalized == "0") {
    out = false;
    return true;
  }
  if (normalized == "auto" || normalized.empty()) {
    out.reset();
    return true;
  }
  error = "unsupported --benchmark-force-semantic-product value: " + normalized +
          " (expected auto|on|off)";
  return false;
}

bool parseBenchmarkSemanticFactFamilies(const std::string &text,
                                        bool &specified,
                                        std::vector<std::string> &out) {
  out.clear();
  specified = true;
  const std::string trimmed = trimWhitespace(text);
  if (trimmed.empty() || trimmed == "auto" || trimmed == "all") {
    specified = false;
    return true;
  }
  if (trimmed == "none") {
    return true;
  }
  size_t start = 0;
  auto addUnique = [&](const std::string &name) {
    for (const auto &existing : out) {
      if (existing == name) {
        return;
      }
    }
    out.push_back(name);
  };
  while (start <= trimmed.size()) {
    size_t end = trimmed.find(',', start);
    if (end == std::string::npos) {
      end = trimmed.size();
    }
    std::string token = trimWhitespace(trimmed.substr(start, end - start));
    if (!token.empty()) {
      addUnique(token);
    }
    if (end == trimmed.size()) {
      break;
    }
    start = end + 1;
  }
  return true;
}

bool parsePositiveUint32(const std::string &text, uint32_t &out, std::string &error, const char *optionName) {
  const std::string normalized = trimWhitespace(text);
  if (normalized.empty()) {
    error = std::string(optionName) + " requires a positive integer value";
    return false;
  }
  try {
    size_t consumed = 0;
    const unsigned long long parsed = std::stoull(normalized, &consumed);
    if (consumed != normalized.size() ||
        parsed == 0 ||
        parsed > static_cast<unsigned long long>(std::numeric_limits<uint32_t>::max())) {
      error = std::string("invalid ") + optionName + " value: " + normalized;
      return false;
    }
    out = static_cast<uint32_t>(parsed);
    return true;
  } catch (...) {
    error = std::string("invalid ") + optionName + " value: " + normalized;
    return false;
  }
}

void applyNoTransformFlags(Options &out, bool noTransforms, bool noTextTransforms, bool noSemanticTransforms) {
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
}

void normalizeEntryPath(Options &out) {
  if (out.entryPath.empty()) {
    out.entryPath = "/main";
    return;
  }
  if (out.entryPath[0] != '/') {
    out.entryPath = "/" + out.entryPath;
  }
}

bool applyPrimecOutputDefaults(Options &out) {
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
    if (out.emitKind == "cpp" || out.emitKind == "cpp-ir") {
      out.outputPath = stem + ".cpp";
    } else if (out.emitKind == "ir") {
      out.outputPath = stem + ".psir";
    } else if (out.emitKind == "glsl" || out.emitKind == "glsl-ir") {
      out.outputPath = stem + ".glsl";
    } else if (out.emitKind == "spirv" || out.emitKind == "spirv-ir") {
      out.outputPath = stem + ".spv";
    } else if (out.emitKind == "wasm") {
      out.outputPath = stem + ".wasm";
    } else {
      out.outputPath = stem;
    }
  }
  return !out.emitKind.empty() && !out.outputPath.empty();
}

} // namespace

bool parseOptions(int argc, char **argv, OptionsParserMode mode, Options &out, std::string &error) {
  const bool isPrimecMode = mode == OptionsParserMode::Primec;
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
    if (!isPrimecMode && arg == "--emit=vm") {
      continue;
    }
    if (isPrimecMode && arg.rfind("--emit=", 0) == 0) {
      const std::string emitKind = arg.substr(std::string("--emit=").size());
      if (!isPrimecEmitKind(emitKind)) {
        error = "unknown option: " + arg;
        return false;
      }
      out.emitKind = emitKind;
    } else if (arg == "--emit-diagnostics") {
      out.emitDiagnostics = true;
    } else if (!isPrimecMode && arg == "--debug-json") {
      out.debugJson = true;
    } else if (!isPrimecMode && arg == "--debug-trace" && i + 1 < argc) {
      out.debugTracePath = argv[++i];
    } else if (!isPrimecMode && arg == "--debug-trace") {
      error = "--debug-trace requires an output path";
      return false;
    } else if (!isPrimecMode && arg.rfind("--debug-trace=", 0) == 0) {
      out.debugTracePath = arg.substr(std::string("--debug-trace=").size());
      if (out.debugTracePath.empty()) {
        error = "--debug-trace requires an output path";
        return false;
      }
    } else if (!isPrimecMode && arg == "--debug-replay" && i + 1 < argc) {
      out.debugReplayPath = argv[++i];
    } else if (!isPrimecMode && arg == "--debug-replay") {
      error = "--debug-replay requires an input trace path";
      return false;
    } else if (!isPrimecMode && arg.rfind("--debug-replay=", 0) == 0) {
      out.debugReplayPath = arg.substr(std::string("--debug-replay=").size());
      if (out.debugReplayPath.empty()) {
        error = "--debug-replay requires an input trace path";
        return false;
      }
    } else if (!isPrimecMode && arg == "--debug-replay-sequence" && i + 1 < argc) {
      const std::string value = argv[++i];
      try {
        size_t consumed = 0;
        const unsigned long long parsed = std::stoull(value, &consumed);
        if (consumed != value.size()) {
          error = "invalid --debug-replay-sequence value: " + value;
          return false;
        }
        out.debugReplaySequence = static_cast<uint64_t>(parsed);
      } catch (...) {
        error = "invalid --debug-replay-sequence value: " + value;
        return false;
      }
    } else if (!isPrimecMode && arg == "--debug-replay-sequence") {
      error = "--debug-replay-sequence requires a numeric value";
      return false;
    } else if (!isPrimecMode && arg.rfind("--debug-replay-sequence=", 0) == 0) {
      const std::string value = arg.substr(std::string("--debug-replay-sequence=").size());
      if (value.empty()) {
        error = "--debug-replay-sequence requires a numeric value";
        return false;
      }
      try {
        size_t consumed = 0;
        const unsigned long long parsed = std::stoull(value, &consumed);
        if (consumed != value.size()) {
          error = "invalid --debug-replay-sequence value: " + value;
          return false;
        }
        out.debugReplaySequence = static_cast<uint64_t>(parsed);
      } catch (...) {
        error = "invalid --debug-replay-sequence value: " + value;
        return false;
      }
    } else if (!isPrimecMode && arg == "--debug-dap") {
      out.debugDap = true;
    } else if (!isPrimecMode && arg == "--debug-json-snapshots") {
      out.debugJsonSnapshotMode = DebugJsonSnapshotMode::All;
    } else if (!isPrimecMode && arg.rfind("--debug-json-snapshots=", 0) == 0) {
      const std::string value = arg.substr(std::string("--debug-json-snapshots=").size());
      if (!parseDebugJsonSnapshotMode(value, out.debugJsonSnapshotMode)) {
        error = "unsupported --debug-json-snapshots value: " + value + " (expected none|stop|all)";
        return false;
      }
    } else if (arg == "--collect-diagnostics") {
      out.collectDiagnostics = true;
    } else if (arg == "--list-transforms") {
      out.listTransforms = true;
    } else if (isPrimecMode && arg == "-o" && i + 1 < argc) {
      out.outputPath = argv[++i];
    } else if (isPrimecMode && arg == "--wasm-profile" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (!normalizeWasmProfile(value, out.wasmProfile)) {
        error = "unsupported --wasm-profile value: " + value + " (expected wasi|browser)";
        return false;
      }
    } else if (isPrimecMode && arg == "--wasm-profile") {
      error = "--wasm-profile requires a value";
      return false;
    } else if (isPrimecMode && arg.rfind("--wasm-profile=", 0) == 0) {
      const std::string value = arg.substr(std::string("--wasm-profile=").size());
      if (value.empty()) {
        error = "--wasm-profile requires a value";
        return false;
      }
      if (!normalizeWasmProfile(value, out.wasmProfile)) {
        error = "unsupported --wasm-profile value: " + value + " (expected wasi|browser)";
        return false;
      }
    } else if (arg == "--entry" && i + 1 < argc) {
      out.entryPath = argv[++i];
    } else if (arg.rfind("--entry=", 0) == 0) {
      out.entryPath = arg.substr(std::string("--entry=").size());
    } else if (arg == "--dump-stage" && i + 1 < argc) {
      out.dumpStage = argv[++i];
    } else if (arg.rfind("--dump-stage=", 0) == 0) {
      out.dumpStage = arg.substr(std::string("--dump-stage=").size());
    } else if (arg == "--import-path" && i + 1 < argc) {
      out.importPaths.push_back(argv[++i]);
    } else if (arg.rfind("--import-path=", 0) == 0) {
      out.importPaths.push_back(arg.substr(std::string("--import-path=").size()));
    } else if (arg == "-I" && i + 1 < argc) {
      out.importPaths.push_back(argv[++i]);
    } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
      out.importPaths.push_back(arg.substr(2));
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
    } else if (isPrimecMode && arg == "--out-dir" && i + 1 < argc) {
      out.outDir = argv[++i];
    } else if (isPrimecMode && arg.rfind("--out-dir=", 0) == 0) {
      out.outDir = arg.substr(std::string("--out-dir=").size());
    } else if (arg == "--default-effects" && i + 1 < argc) {
      auto effects = parseDefaultEffects(argv[++i]);
      out.defaultEffects = effects;
      out.entryDefaultEffects = effects;
    } else if (arg.rfind("--default-effects=", 0) == 0) {
      auto effects = parseDefaultEffects(arg.substr(std::string("--default-effects=").size()));
      out.defaultEffects = effects;
      out.entryDefaultEffects = effects;
    } else if (arg == "--benchmark-force-semantic-product" && i + 1 < argc) {
      if (!parseBenchmarkForceSemanticProduct(argv[++i], out.benchmarkForceSemanticProduct, error)) {
        return false;
      }
    } else if (arg == "--benchmark-force-semantic-product") {
      error = "--benchmark-force-semantic-product requires a value";
      return false;
    } else if (arg.rfind("--benchmark-force-semantic-product=", 0) == 0) {
      if (!parseBenchmarkForceSemanticProduct(arg.substr(std::string("--benchmark-force-semantic-product=").size()),
                                             out.benchmarkForceSemanticProduct,
                                             error)) {
        return false;
      }
    } else if (arg == "--benchmark-semantic-fact-families" && i + 1 < argc) {
      if (!parseBenchmarkSemanticFactFamilies(
              argv[++i], out.benchmarkSemanticFactFamiliesSpecified, out.benchmarkSemanticFactFamilies)) {
        error = "invalid --benchmark-semantic-fact-families value";
        return false;
      }
    } else if (arg == "--benchmark-semantic-fact-families") {
      error = "--benchmark-semantic-fact-families requires a value";
      return false;
    } else if (arg.rfind("--benchmark-semantic-fact-families=", 0) == 0) {
      if (!parseBenchmarkSemanticFactFamilies(arg.substr(std::string("--benchmark-semantic-fact-families=").size()),
                                              out.benchmarkSemanticFactFamiliesSpecified,
                                              out.benchmarkSemanticFactFamilies)) {
        error = "invalid --benchmark-semantic-fact-families value";
        return false;
      }
    } else if (arg == "--benchmark-semantic-no-fact-emission") {
      out.benchmarkSemanticNoFactEmission = true;
    } else if (arg == "--benchmark-semantic-phase-counters") {
      out.benchmarkSemanticPhaseCounters = true;
    } else if (arg == "--benchmark-semantic-allocation-counters") {
      out.benchmarkSemanticAllocationCounters = true;
    } else if (arg == "--benchmark-semantic-rss-checkpoints") {
      out.benchmarkSemanticRssCheckpoints = true;
    } else if (arg == "--benchmark-semantic-repeat-count" && i + 1 < argc) {
      uint32_t repeatCount = 0;
      if (!parsePositiveUint32(argv[++i], repeatCount, error, "--benchmark-semantic-repeat-count")) {
        return false;
      }
      out.benchmarkSemanticRepeatCompileCount = repeatCount;
    } else if (arg == "--benchmark-semantic-repeat-count") {
      error = "--benchmark-semantic-repeat-count requires a value";
      return false;
    } else if (arg.rfind("--benchmark-semantic-repeat-count=", 0) == 0) {
      uint32_t repeatCount = 0;
      if (!parsePositiveUint32(arg.substr(std::string("--benchmark-semantic-repeat-count=").size()),
                               repeatCount,
                               error,
                               "--benchmark-semantic-repeat-count")) {
        return false;
      }
      out.benchmarkSemanticRepeatCompileCount = repeatCount;
    } else if (arg == "--benchmark-semantic-two-chunk-definition-validation") {
      out.benchmarkSemanticTwoChunkDefinitionValidation = true;
    } else if (arg == "--benchmark-semantic-definition-validation-workers" && i + 1 < argc) {
      uint32_t workerCount = 0;
      if (!parsePositiveUint32(argv[++i],
                               workerCount,
                               error,
                               "--benchmark-semantic-definition-validation-workers")) {
        return false;
      }
      out.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    } else if (arg == "--benchmark-semantic-definition-validation-workers") {
      error = "--benchmark-semantic-definition-validation-workers requires a value";
      return false;
    } else if (arg.rfind("--benchmark-semantic-definition-validation-workers=", 0) == 0) {
      uint32_t workerCount = 0;
      if (!parsePositiveUint32(
              arg.substr(std::string("--benchmark-semantic-definition-validation-workers=").size()),
              workerCount,
              error,
              "--benchmark-semantic-definition-validation-workers")) {
        return false;
      }
      out.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    } else if (arg == "--ir-inline") {
      out.inlineIrCalls = true;
    } else if (!arg.empty() && arg[0] == '-') {
      error = "unknown option: " + arg;
      return false;
    } else {
      if (!out.inputPath.empty()) {
        error = "multiple input files are not supported";
        return false;
      }
      out.inputPath = arg;
    }
  }

  applyNoTransformFlags(out, noTransforms, noTextTransforms, noSemanticTransforms);
  normalizeEntryPath(out);

  if (out.listTransforms) {
    return true;
  }
  if (out.inputPath.empty()) {
    error = "missing input path";
    return false;
  }
  if (!isPrimecMode) {
    if (out.debugDap && out.debugJson) {
      error = "--debug-dap cannot be combined with --debug-json";
      return false;
    }
    if (out.debugDap && !out.debugTracePath.empty()) {
      error = "--debug-dap cannot be combined with --debug-trace";
      return false;
    }
    if (out.debugJson && !out.debugTracePath.empty()) {
      error = "--debug-json cannot be combined with --debug-trace";
      return false;
    }
    if (!out.debugReplayPath.empty() && out.debugJson) {
      error = "--debug-replay cannot be combined with --debug-json";
      return false;
    }
    if (!out.debugReplayPath.empty() && out.debugDap) {
      error = "--debug-replay cannot be combined with --debug-dap";
      return false;
    }
    if (!out.debugReplayPath.empty() && !out.debugTracePath.empty()) {
      error = "--debug-replay cannot be combined with --debug-trace";
      return false;
    }
    if (out.debugReplaySequence.has_value() && out.debugReplayPath.empty()) {
      error = "--debug-replay-sequence requires --debug-replay";
      return false;
    }
    if (out.debugJsonSnapshotMode != DebugJsonSnapshotMode::None && !out.debugJson) {
      error = "--debug-json-snapshots requires --debug-json";
      return false;
    }
    return true;
  }
  return applyPrimecOutputDefaults(out);
}

} // namespace primec
