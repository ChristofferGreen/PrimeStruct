#include "IrPrinterInternal.h"

#include <cctype>
#include <sstream>

namespace primec::ir_printer {

std::string joinTemplateArgs(const std::vector<std::string> &args) {
  std::ostringstream out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << args[i];
  }
  return out.str();
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
      depth++;
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
      depth--;
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
  if (!text.empty()) {
    pushSegment(text.size());
  }
  return !out.empty();
}

std::string bindingTypeName(const Expr &expr) {
  std::string typeName;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut" || transform.name == "copy" || transform.name == "restrict" ||
        transform.name == "public" || transform.name == "private" || transform.name == "static" ||
        transform.name == "align_bytes" || transform.name == "align_kbytes") {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (!transform.templateArgs.empty()) {
      typeName = transform.name + "<" + joinTemplateArgs(transform.templateArgs) + ">";
    } else {
      typeName = transform.name;
    }
  }
  if (typeName == "int" || typeName == "i32") {
    return "i32";
  }
  if (typeName == "i64") {
    return "i64";
  }
  if (typeName == "u64") {
    return "u64";
  }
  if (typeName == "bool") {
    return "bool";
  }
  if (typeName == "float" || typeName == "f32") {
    return "f32";
  }
  if (typeName == "f64") {
    return "f64";
  }
  if (typeName == "string") {
    return "string";
  }
  return "";
}

bool isAssignCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "assign";
}

bool isReturnCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "return";
}

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == nameToMatch;
}

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinClampName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "clamp") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "min" || name == "max") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "abs" || name == "sign") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "saturate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "convert") {
    out = name;
    return true;
  }
  return false;
}

ReturnKind getReturnKind(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "struct" || transform.name == "pod" || transform.name == "handle" ||
        transform.name == "gpu_lane" || transform.name == "no_padding" ||
        transform.name == "platform_independent_padding") {
      return ReturnKind::Void;
    }
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &typeName = transform.templateArgs.front();
    if (typeName == "void") {
      return ReturnKind::Void;
    }
    if (typeName == "int") {
      return ReturnKind::Int;
    }
    if (typeName == "i64") {
      return ReturnKind::Int64;
    }
    if (typeName == "u64") {
      return ReturnKind::UInt64;
    }
    if (typeName == "bool") {
      return ReturnKind::Bool;
    }
    if (typeName == "i32") {
      return ReturnKind::Int;
    }
    if (typeName == "float" || typeName == "f32") {
      return ReturnKind::Float32;
    }
    if (typeName == "f64") {
      return ReturnKind::Float64;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(typeName, base, arg) && base == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
  }
  return ReturnKind::Unknown;
}

const char *returnTypeName(ReturnKind kind) {
  if (kind == ReturnKind::Void) {
    return "void";
  }
  if (kind == ReturnKind::Int64) {
    return "i64";
  }
  if (kind == ReturnKind::UInt64) {
    return "u64";
  }
  if (kind == ReturnKind::Float32) {
    return "f32";
  }
  if (kind == ReturnKind::Float64) {
    return "f64";
  }
  if (kind == ReturnKind::Bool) {
    return "bool";
  }
  if (kind == ReturnKind::String) {
    return "string";
  }
  if (kind == ReturnKind::Array) {
    return "array";
  }
  return "i32";
}

ReturnKind returnKindForTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return ReturnKind::Int;
  }
  if (name == "i64") {
    return ReturnKind::Int64;
  }
  if (name == "u64") {
    return ReturnKind::UInt64;
  }
  if (name == "bool") {
    return ReturnKind::Bool;
  }
  if (name == "string") {
    return ReturnKind::String;
  }
  if (name == "float" || name == "f32") {
    return ReturnKind::Float32;
  }
  if (name == "f64") {
    return ReturnKind::Float64;
  }
  if (name == "void") {
    return ReturnKind::Void;
  }
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg)) {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return ReturnKind::Unknown;
    }
    const bool isCollectionLike =
        ((base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer" ||
          base == "Vector" || base == "std/collections/experimental_vector/Vector" ||
          base == "/std/collections/experimental_vector/Vector" ||
          base.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
          base.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) &&
         args.size() == 1) ||
        ((base == "map" || base == "Map" || base == "std/collections/experimental_map/Map" ||
          base == "/std/collections/experimental_map/Map" ||
          base.rfind("std/collections/experimental_map/Map__", 0) == 0 ||
          base.rfind("/std/collections/experimental_map/Map__", 0) == 0) &&
         args.size() == 2);
    if (isCollectionLike) {
      return ReturnKind::Array;
    }
  }
  return ReturnKind::Unknown;
}

} // namespace primec::ir_printer
