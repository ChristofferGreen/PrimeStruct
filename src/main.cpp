#include "primec/AstPrinter.h"
#include "primec/Emitter.h"
#include "primec/IncludeResolver.h"
#include "primec/IrLowerer.h"
#include "primec/IrPrinter.h"
#include "primec/IrSerializer.h"
#include "primec/Lexer.h"
#include "primec/NativeEmitter.h"
#include "primec/Options.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRegistry.h"
#include "primec/Vm.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
std::vector<std::string> defaultTextFilters() {
  return {"collections", "operators", "implicit-utf8"};
}

std::vector<std::string> defaultSemanticTransforms() {
  return {};
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
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(',', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string token = trimWhitespace(text.substr(start, end - start));
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
    if (end == text.size()) {
      break;
    }
    start = end + 1;
  }
  return true;
}

bool parseTransformListAuto(const std::string &text,
                            std::vector<std::string> &textOut,
                            std::vector<std::string> &semanticOut,
                            std::string &error) {
  textOut.clear();
  semanticOut.clear();
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(',', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string token = trimWhitespace(text.substr(start, end - start));
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
    if (end == text.size()) {
      break;
    }
    start = end + 1;
  }
  return true;
}

bool parseTransformRule(const std::string &text,
                        const std::vector<std::string> &defaults,
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
  if (!parseTransformListForPhase(listSpec, defaults, true, rule.transforms, error)) {
    return false;
  }
  return true;
}

bool parseTransformRules(const std::string &text,
                         const std::vector<std::string> &defaults,
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
        if (!parseTransformRule(token, defaults, rule, error)) {
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
    if (arg == "--emit=cpp" || arg == "--emit=exe" || arg == "--emit=native" || arg == "--emit=ir" || arg == "--emit=vm") {
      out.emitKind = arg.substr(std::string("--emit=").size());
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
      if (!parseTransformRules(argv[++i], defaultTextFilters(), out.textTransformRules, error)) {
        return false;
      }
    } else if (arg.rfind("--text-transform-rules=", 0) == 0) {
      if (!parseTransformRules(arg.substr(std::string("--text-transform-rules=").size()),
                               defaultTextFilters(),
                               out.textTransformRules,
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
  if (noTransforms) {
    out.textFilters.clear();
    out.textTransformRules.clear();
    out.semanticTransforms.clear();
    out.allowEnvelopeTextTransforms = false;
  } else {
    if (noTextTransforms) {
      out.textFilters.clear();
      out.textTransformRules.clear();
      out.allowEnvelopeTextTransforms = false;
    }
    if (noSemanticTransforms) {
      out.semanticTransforms.clear();
    }
  }
  out.requireCanonicalSyntax = noTransforms;
  if (out.entryPath.empty()) {
    out.entryPath = "/main";
  } else if (out.entryPath[0] != '/') {
    out.entryPath = "/" + out.entryPath;
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
    std::cerr << "Usage: primec [--emit=cpp|exe|native|ir|vm] <input.prime> [-o <output>] [--entry /path] "
                 "[--include-path <dir>] [--text-filters <list>] [--text-transforms <list>] "
                 "[--text-transform-rules <rules>] "
                 "[--semantic-transforms <list>] [--transform-list <list>] [--no-text-transforms] "
                 "[--no-semantic-transforms] [--no-transforms] [--out-dir <dir>] "
                 "[--default-effects <list>] [--dump-stage pre_ast|ast|ir] [-- <program args...>]\n";
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
    {
      std::cerr << "Unsupported dump stage: " << options.dumpStage << "\n";
      return 2;
    }
  }

  primec::Semantics semantics;
  if (!semantics.validate(program, options.entryPath, error, options.defaultEffects, options.semanticTransforms)) {
    std::cerr << "Semantic error: " << error << "\n";
    return 2;
  }

  if (options.emitKind == "vm") {
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
    if (!lowerer.lower(program, options.entryPath, ir, error)) {
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
    if (!lowerer.lower(program, options.entryPath, ir, error)) {
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
