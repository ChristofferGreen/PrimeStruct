#include "IrLowererStructLayoutHelpers.h"

#include <cctype>
#include <limits>

namespace primec::ir_lowerer {

uint32_t alignTo(uint32_t value, uint32_t alignment) {
  if (alignment == 0) {
    return value;
  }
  const uint32_t remainder = value % alignment;
  if (remainder == 0) {
    return value;
  }
  return value + (alignment - remainder);
}

bool parsePositiveInt(const std::string &text, int &valueOut) {
  std::string digits = text;
  if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
    digits.resize(digits.size() - 3);
  }
  if (digits.empty()) {
    return false;
  }
  int parsed = 0;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    const int digit = c - '0';
    if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
      return false;
    }
    parsed = parsed * 10 + digit;
  }
  if (parsed <= 0) {
    return false;
  }
  valueOut = parsed;
  return true;
}

bool extractAlignment(const std::vector<Transform> &transforms,
                      const std::string &context,
                      uint32_t &alignmentOut,
                      bool &hasAlignment,
                      std::string &error) {
  alignmentOut = 1;
  hasAlignment = false;
  for (const auto &transform : transforms) {
    if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
      continue;
    }
    if (hasAlignment) {
      error = "duplicate " + transform.name + " transform on " + context;
      return false;
    }
    if (transform.arguments.size() != 1 || !transform.templateArgs.empty()) {
      error = transform.name + " requires exactly one integer argument";
      return false;
    }
    int value = 0;
    if (!parsePositiveInt(transform.arguments[0], value)) {
      error = transform.name + " requires a positive integer argument";
      return false;
    }
    uint64_t bytes = static_cast<uint64_t>(value);
    if (transform.name == "align_kbytes") {
      bytes *= 1024ull;
    }
    if (bytes > std::numeric_limits<uint32_t>::max()) {
      error = transform.name + " alignment too large on " + context;
      return false;
    }
    alignmentOut = static_cast<uint32_t>(bytes);
    hasAlignment = true;
  }
  return true;
}

bool isLayoutQualifierName(const std::string &name) {
  return name == "public" || name == "private" || name == "static" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "pod" ||
         name == "handle" || name == "gpu_lane";
}

IrStructFieldCategory fieldCategory(const Expr &expr) {
  bool hasPod = false;
  bool hasHandle = false;
  bool hasGpuLane = false;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "pod") {
      hasPod = true;
    } else if (transform.name == "handle") {
      hasHandle = true;
    } else if (transform.name == "gpu_lane") {
      hasGpuLane = true;
    }
  }
  if (hasPod) {
    return IrStructFieldCategory::Pod;
  }
  if (hasHandle) {
    return IrStructFieldCategory::Handle;
  }
  if (hasGpuLane) {
    return IrStructFieldCategory::GpuLane;
  }
  return IrStructFieldCategory::Default;
}

IrStructVisibility fieldVisibility(const Expr &expr) {
  IrStructVisibility visibility = IrStructVisibility::Public;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "public") {
      visibility = IrStructVisibility::Public;
    } else if (transform.name == "private") {
      visibility = IrStructVisibility::Private;
    }
  }
  return visibility;
}

bool isStaticField(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

} // namespace primec::ir_lowerer
