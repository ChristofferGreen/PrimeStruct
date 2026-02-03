#include "primec/AstPrinter.h"
#include "primec/Emitter.h"
#include "primec/IncludeResolver.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/Options.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/TextFilterPipeline.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
bool parseArgs(int argc, char **argv, primec::Options &out) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--emit=cpp" || arg == "--emit=exe") {
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
    } else if (arg == "--no-implicit-i32") {
      out.implicitI32Suffix = false;
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
    std::cerr << "Usage: primec --emit=cpp|exe <input.prime> -o <output> [--entry /path] [--dump-stage pre_ast|ast|ir]\n";
    return 2;
  }

  std::string source;
  std::string error;
  primec::IncludeResolver includeResolver;
  if (!includeResolver.expandIncludes(options.inputPath, source, error)) {
    std::cerr << "Include error: " << error << "\n";
    return 2;
  }

  primec::TextFilterPipeline textPipeline;
  primec::TextFilterOptions textOptions;
  textOptions.implicitI32Suffix = options.implicitI32Suffix;
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
  if (!semantics.validate(program, options.entryPath, error)) {
    std::cerr << "Semantic error: " << error << "\n";
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
