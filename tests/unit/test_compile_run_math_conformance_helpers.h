#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
std::string runMathConformance(const std::string &source, const std::string &name, const std::string &emitKind) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (testScratchPath("") / (name + "_" + emitKind + ".out")).string();

  if (emitKind == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    return readFile(outPath);
  }

  const std::string exePath = (testScratchPath("") / (name + "_" + emitKind + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitKind + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 0);
  return readFile(outPath);
}

void checkMathConformance(const std::string &source, const std::string &name) {
  const std::string baseline = runMathConformance(source, name, "exe");
  const std::string vmOut = runMathConformance(source, name, "vm");
  CHECK(vmOut == baseline);
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const std::string nativeOut = runMathConformance(source, name, "native");
  CHECK(nativeOut == baseline);
#endif
}

void checkMathConformanceVmParity(const std::string &source, const std::string &name) {
  const std::string baseline = runMathConformance(source, name, "exe");
  const std::string vmOut = runMathConformance(source, name, "vm");
  CHECK(vmOut == baseline);
}

struct LabeledSample {
  std::string label;
  std::string value;
};

std::string trimWhitespace(const std::string &text);

std::vector<LabeledSample> parseLabeledOutput(const std::string &output, const std::string &context) {
  std::vector<LabeledSample> samples;
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    std::string trimmed = trimWhitespace(line);
    if (trimmed.empty()) {
      continue;
    }
    std::istringstream tokenStream(trimmed);
    std::vector<std::string> tokens;
    std::string token;
    while (tokenStream >> token) {
      tokens.push_back(token);
    }
    if (tokens.size() < 2) {
      CHECK_MESSAGE(false, "Labeled output line requires label + value in ", context, ": '", trimmed, "'");
      continue;
    }
    std::string label = tokens.front();
    for (size_t i = 1; i + 1 < tokens.size(); ++i) {
      label += " ";
      label += tokens[i];
    }
    samples.push_back({label, tokens.back()});
  }
  return samples;
}

void compareLabeledOutputs(const std::vector<LabeledSample> &baseline, const std::vector<LabeledSample> &candidate,
                           const std::string &candidateName,
                           const std::unordered_set<std::string> &valueAllowlist) {
  CHECK_MESSAGE(candidate.size() == baseline.size(), candidateName, " output count mismatch: expected ",
                baseline.size(), ", got ", candidate.size());
  const size_t count = std::min(baseline.size(), candidate.size());
  for (size_t i = 0; i < count; ++i) {
    CHECK_MESSAGE(baseline[i].label == candidate[i].label, candidateName, " label mismatch at line ", i + 1,
                  ": expected '", baseline[i].label, "', got '", candidate[i].label, "'");
    if (valueAllowlist.find(baseline[i].label) != valueAllowlist.end()) {
      continue;
    }
    CHECK_MESSAGE(baseline[i].value == candidate[i].value, candidateName, " value mismatch at line ", i + 1);
  }
}

[[maybe_unused]] void checkMathConformanceWithAllowlist(const std::string &source, const std::string &name,
                                                        const std::unordered_set<std::string> &nativeAllowlist) {
  const std::string baseline = runMathConformance(source, name, "exe");
  const std::string vmOut = runMathConformance(source, name, "vm");
  CHECK(vmOut == baseline);
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const std::string nativeOut = runMathConformance(source, name, "native");
  if (nativeAllowlist.empty()) {
    CHECK(nativeOut == baseline);
  } else {
    const std::vector<LabeledSample> baselineSamples = parseLabeledOutput(baseline, name + " exe");
    const std::vector<LabeledSample> nativeSamples = parseLabeledOutput(nativeOut, name + " native");
    compareLabeledOutputs(baselineSamples, nativeSamples, "native", nativeAllowlist);
  }
#endif
}

using EmitCase = std::pair<std::string, std::string>;

std::string renderEmitCases(const std::vector<EmitCase> &cases) {
  std::string out;
  for (const auto &entry : cases) {
    out += "  emit(\"";
    out += escapeStringLiteral(entry.first);
    out += "\"utf8, ";
    out += entry.second;
    out += ")\n";
  }
  return out;
}

struct ParsedFloat {
  double value = 0.0;
  bool isNan = false;
  bool isInf = false;
};

ParsedFloat parseFloatToken(const std::string &token, const std::string &context) {
  errno = 0;
  char *end = nullptr;
  const double value = std::strtod(token.c_str(), &end);
  if (end == token.c_str() || *end != '\0' || errno == ERANGE) {
    CHECK_MESSAGE(false, "Failed to parse float token '", token, "' in ", context);
    return {};
  }
  ParsedFloat parsed;
  parsed.value = value;
  parsed.isNan = std::isnan(value);
  parsed.isInf = std::isinf(value);
  return parsed;
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

struct FloatSample {
  std::string label;
  ParsedFloat value;
};

std::vector<FloatSample> parseFloatOutput(const std::string &output, const std::string &context) {
  std::vector<FloatSample> samples;
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    std::string trimmed = trimWhitespace(line);
    if (trimmed.empty()) {
      continue;
    }
    std::istringstream tokenStream(trimmed);
    std::vector<std::string> tokens;
    std::string token;
    while (tokenStream >> token) {
      tokens.push_back(token);
    }
    if (tokens.empty()) {
      continue;
    }
    std::string label;
    if (tokens.size() > 1) {
      label = tokens.front();
      for (size_t i = 1; i + 1 < tokens.size(); ++i) {
        label += " ";
        label += tokens[i];
      }
    }
    const std::string valueToken = tokens.back();
    samples.push_back({label, parseFloatToken(valueToken, context)});
  }
  return samples;
}

double absDiff(double a, double b) {
  return std::abs(a - b);
}

double relativeError(double a, double b) {
  const double scale = std::max(std::abs(a), std::abs(b));
  if (scale == 0.0) {
    return 0.0;
  }
  return std::abs(a - b) / scale;
}

bool floatsNear(const ParsedFloat &baseline, const ParsedFloat &candidate, double absEps, double relEps) {
  if (baseline.isNan || candidate.isNan) {
    return baseline.isNan && candidate.isNan;
  }
  if (baseline.isInf || candidate.isInf) {
    if (!(baseline.isInf && candidate.isInf)) {
      return false;
    }
    return std::signbit(baseline.value) == std::signbit(candidate.value);
  }
  if (absDiff(baseline.value, candidate.value) <= absEps) {
    return true;
  }
  return relativeError(baseline.value, candidate.value) <= relEps;
}

void compareFloatOutputs(const std::vector<FloatSample> &baseline, const std::vector<FloatSample> &candidate,
                         const std::string &candidateName, double absEps, double relEps) {
  CHECK_MESSAGE(candidate.size() == baseline.size(), candidateName, " output count mismatch: expected ",
                baseline.size(), ", got ", candidate.size());
  const size_t count = std::min(baseline.size(), candidate.size());
  for (size_t i = 0; i < count; ++i) {
    CHECK_MESSAGE(baseline[i].label == candidate[i].label, candidateName, " label mismatch at line ", i + 1,
                  ": expected '", baseline[i].label, "', got '", candidate[i].label, "'");
    CHECK_MESSAGE(floatsNear(baseline[i].value, candidate[i].value, absEps, relEps), candidateName,
                  " value mismatch at line ", i + 1);
  }
}

[[maybe_unused]] void checkMathConformanceFloats(const std::string &source, const std::string &name, double absEps,
                                                 double relEps) {
  const std::string baselineOut = runMathConformance(source, name, "exe");
  const std::vector<FloatSample> baseline = parseFloatOutput(baselineOut, name + " exe");
  const std::vector<FloatSample> vmOut = parseFloatOutput(runMathConformance(source, name, "vm"), name + " vm");
  compareFloatOutputs(baseline, vmOut, "vm", absEps, relEps);
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const std::vector<FloatSample> nativeOut =
      parseFloatOutput(runMathConformance(source, name, "native"), name + " native");
  compareFloatOutputs(baseline, nativeOut, "native", absEps, relEps);
#endif
}

bool signMatches(const ParsedFloat &baseline, const ParsedFloat &candidate) {
  if (baseline.isNan || candidate.isNan) {
    return baseline.isNan && candidate.isNan;
  }
  if (baseline.isInf || candidate.isInf) {
    if (!(baseline.isInf && candidate.isInf)) {
      return false;
    }
  }
  if (baseline.value == 0.0 && candidate.value == 0.0) {
    return std::signbit(baseline.value) == std::signbit(candidate.value);
  }
  return std::signbit(baseline.value) == std::signbit(candidate.value);
}

bool inRange(const ParsedFloat &value, double minValue, double maxValue) {
  if (value.isNan || value.isInf) {
    return false;
  }
  return value.value >= minValue && value.value <= maxValue;
}
} // namespace
