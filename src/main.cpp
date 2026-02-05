#include "primec/AstPrinter.h"
#include "primec/Emitter.h"
#include "primec/IncludeResolver.h"
#include "primec/IrLowerer.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/NativeEmitter.h"
#include "primec/Options.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/TextFilterPipeline.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
std::vector<std::string> defaultTextFilters() {
  return {"operators", "collections", "implicit-utf8"};
}

std::vector<std::string> defaultEffectsList() {
  return {};
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
bool parseArgs(int argc, char **argv, primec::Options &out) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--emit=cpp" || arg == "--emit=exe" || arg == "--emit=native") {
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
      out.textFilters = parseTextFilters(argv[++i]);
    } else if (arg.rfind("--text-filters=", 0) == 0) {
      out.textFilters = parseTextFilters(arg.substr(std::string("--text-filters=").size()));
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
      out.inputPath = arg;
    }
  }
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
  if (!parseArgs(argc, argv, options)) {
    std::cerr << "Usage: primec --emit=cpp|exe|native <input.prime> -o <output> [--entry /path] "
                 "[--include-path <dir>] [--text-filters <list>] [--out-dir <dir>] [--default-effects <list>] "
                 "[--dump-stage pre_ast|ast|ir]\n";
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
  if (!semantics.validate(program, options.entryPath, error, options.defaultEffects)) {
    std::cerr << "Semantic error: " << error << "\n";
    return 2;
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
