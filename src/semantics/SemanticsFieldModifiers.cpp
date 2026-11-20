#include "SemanticsFieldModifiers.h"

#include <string_view>

namespace primec::semantics {

namespace {

bool hasTransformNamed(const Expr &expr, const char *name) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

bool isExplicitStruct(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "struct" || transform.name == "pod" || transform.name == "handle" ||
        transform.name == "gpu_lane") {
      return true;
    }
  }
  return false;
}

// The first `ctor_only` found anywhere under `expr`, itself included.
const Expr *findConstructionOnly(const Expr &expr) {
  if (isUnstoredConstructionField(expr)) {
    return &expr;
  }
  for (const auto &arg : expr.args) {
    if (const Expr *found = findConstructionOnly(arg)) {
      return found;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    if (const Expr *found = findConstructionOnly(arg)) {
      return found;
    }
  }
  return nullptr;
}

// Whether `expr` reads the name `name` anywhere under it.
bool mentionsName(const Expr &expr, const std::string &name) {
  if (expr.kind == Expr::Kind::Name && expr.name == name) {
    return true;
  }
  for (const auto &arg : expr.args) {
    if (mentionsName(arg, name)) {
      return true;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    if (mentionsName(arg, name)) {
      return true;
    }
  }
  return false;
}

// Whether a field after `fieldIndex` reads the construction-only field in its default.
bool readByLaterDefault(const Definition &def, size_t fieldIndex) {
  const std::string &name = def.statements[fieldIndex].name;
  for (size_t later = fieldIndex + 1; later < def.statements.size(); ++later) {
    for (const auto &arg : def.statements[later].args) {
      if (mentionsName(arg, name)) {
        return true;
      }
    }
  }
  return false;
}

std::string misplacedMessage(const Definition &def, const Expr &expr) {
  return "ctor_only is only valid on struct fields: " + def.fullPath +
         (expr.name.empty() ? "" : "/" + expr.name);
}

} // namespace

FieldBinding fieldBindingFor(const BindingInfo &field) {
  if (field.isConstructionOnly) {
    return FieldBinding::ConstructionOnlyUnstored;
  }
  if (field.typeName == "Reference") {
    return FieldBinding::ConstructionOnly;
  }
  return FieldBinding::Assignable;
}

std::optional<std::string> fieldAssignmentDiagnostic(const BindingInfo &field, const std::string &fieldName) {
  if (fieldBindingFor(field) == FieldBinding::Assignable) {
    return std::nullopt;
  }
  if (fieldBindingFor(field) == FieldBinding::ConstructionOnlyUnstored) {
    return "construction-only field is not stored: " + fieldName +
           " (a ctor_only field is read by the field defaults that follow it during brace "
           "construction)";
  }
  return "Reference field cannot be reassigned: " + fieldName +
         " (a Reference field is bound once by brace construction; to write the referent, assign "
         "through it "
         "with *this." +
         fieldName + " = value)";
}

std::optional<std::string> referenceFieldWriteDiagnostic(const BindingInfo &field,
                                                         const std::string &fieldName) {
  if (field.isMutable) {
    return std::nullopt;
  }
  return "Reference field is not mutable: " + fieldName +
         " (declare it [Reference<T> mut] to write through it with *this." + fieldName +
         " = value)";
}

bool isLifecycleHelperPath(const std::string &fullPath) {
  static const char *const suffixes[] = {"/Create",
                                         "/Destroy",
                                         "/Copy",
                                         "/Move",
                                         "/CreateStack",
                                         "/DestroyStack",
                                         "/CreateHeap",
                                         "/DestroyHeap",
                                         "/CreateBuffer",
                                         "/DestroyBuffer"};
  for (const std::string_view suffix : suffixes) {
    if (fullPath.size() >= suffix.size() &&
        fullPath.compare(fullPath.size() - suffix.size(), suffix.size(), suffix) == 0) {
      return true;
    }
  }
  return false;
}

std::string constructionOnlyPlacementError(const Program &program) {
  for (const auto &def : program.definitions) {
    const bool structDef = isExplicitStruct(def);
    for (const auto &param : def.parameters) {
      if (const Expr *found = findConstructionOnly(param)) {
        return misplacedMessage(def, *found);
      }
    }
    for (size_t index = 0; index < def.statements.size(); ++index) {
      const Expr &stmt = def.statements[index];
      const bool field = structDef && stmt.isBinding;
      if (field && isUnstoredConstructionField(stmt)) {
        if (hasTransformNamed(stmt, "static")) {
          return "ctor_only cannot be combined with static: " + def.fullPath + "/" + stmt.name;
        }
        if (!readByLaterDefault(def, index)) {
          return "ctor_only field is never read by a later field default: " + def.fullPath + "/" +
                 stmt.name;
        }
        for (const auto &arg : stmt.args) {
          if (const Expr *found = findConstructionOnly(arg)) {
            return misplacedMessage(def, *found);
          }
        }
        continue;
      }
      if (const Expr *found = findConstructionOnly(stmt)) {
        return misplacedMessage(def, *found);
      }
    }
  }
  for (const auto &exec : program.executions) {
    for (const auto &arg : exec.arguments) {
      if (findConstructionOnly(arg) != nullptr) {
        return "ctor_only is only valid on struct fields: " + exec.fullPath;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      if (findConstructionOnly(arg) != nullptr) {
        return "ctor_only is only valid on struct fields: " + exec.fullPath;
      }
    }
  }
  return {};
}

} // namespace primec::semantics
