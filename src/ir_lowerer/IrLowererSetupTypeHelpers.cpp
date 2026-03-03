#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return LocalInfo::ValueKind::Int32;
  }
  if (name == "i64") {
    return LocalInfo::ValueKind::Int64;
  }
  if (name == "u64") {
    return LocalInfo::ValueKind::UInt64;
  }
  if (name == "float" || name == "f32") {
    return LocalInfo::ValueKind::Float32;
  }
  if (name == "f64") {
    return LocalInfo::ValueKind::Float64;
  }
  if (name == "bool") {
    return LocalInfo::ValueKind::Bool;
  }
  if (name == "string") {
    return LocalInfo::ValueKind::String;
  }
  if (name == "FileError") {
    return LocalInfo::ValueKind::Int32;
  }

  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg) && base == "File") {
    return LocalInfo::ValueKind::Int64;
  }
  return LocalInfo::ValueKind::Unknown;
}

} // namespace primec::ir_lowerer
