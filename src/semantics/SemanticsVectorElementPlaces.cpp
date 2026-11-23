#include "SemanticsVectorElementPlaces.h"

#include "SemanticsHelpers.h"
#include "primec/support/CollectionHelperNames.h"

#include <string_view>

namespace primec::semantics {

namespace {

constexpr std::string_view VectorHelperPrefix = collection_helpers::kCanonicalVectorPrefix;
constexpr std::string_view SpecializationMarker = "__t";

struct ElementReadHelper {
  std::string_view read;
  std::string_view place;
};

// `at` checks the index and `at_unsafe` does not; each place helper keeps that.
constexpr ElementReadHelper ElementReadHelpers[] = {
    {"at", "vectorElementRef"},
    {"at_unsafe", "vectorBorrowSlot"},
};

// The specialization suffix (`__t<hash>`) of `path` when it is an instantiation of the vector helper `helper`.
bool vectorHelperSpecialization(std::string_view path,
                                std::string_view helper,
                                std::string_view &suffixOut) {
  if (path.substr(0, VectorHelperPrefix.size()) != VectorHelperPrefix) {
    return false;
  }
  path.remove_prefix(VectorHelperPrefix.size());
  if (path.substr(0, helper.size()) != helper) {
    return false;
  }
  path.remove_prefix(helper.size());
  if (path.substr(0, SpecializationMarker.size()) != SpecializationMarker) {
    return false;
  }
  suffixOut = path;
  return true;
}

const std::string &calleePath(const Expr &expr) {
  return expr.resolvedCallPath.empty() ? expr.name : expr.resolvedCallPath;
}

const ElementReadHelper *elementReadHelper(const Expr &expr, std::string_view &suffixOut) {
  if (expr.kind != Expr::Kind::Call || expr.isFieldAccess || expr.isMethodCall || expr.isBinding ||
      expr.args.size() != 2) {
    return nullptr;
  }
  for (const ElementReadHelper &helper : ElementReadHelpers) {
    if (vectorHelperSpecialization(calleePath(expr), helper.read, suffixOut)) {
      return &helper;
    }
  }
  return nullptr;
}

bool isFieldRead(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && expr.isFieldAccess && expr.args.size() == 1;
}

std::string siteKey(const std::string &definitionPath, const Expr &elementRead) {
  return definitionPath + "@" + std::to_string(elementRead.sourceLine) + ":" +
         std::to_string(elementRead.sourceColumn);
}

void retargetToPlaceHelper(Expr &read, const ElementReadHelper &helper, std::string_view suffix) {
  std::string placePath(VectorHelperPrefix);
  placePath += helper.place;
  placePath += suffix;
  read.name = placePath;
  read.resolvedCallPath = placePath;
}

// `values[i] = x` is `assign(at(values, i), x)`: the target becomes `*vectorElementRef(values, i)`, an assignment
// through the element's reference.
void rewriteAssignedElement(const std::string &definitionPath,
                            Expr &assign,
                            const VectorElementPlaceSites &sites) {
  if (!isAssignCall(assign) || assign.args.size() != 2) {
    return;
  }
  Expr &target = assign.args.front();
  std::string_view suffix;
  const ElementReadHelper *helper = elementReadHelper(target, suffix);
  if (helper == nullptr || !sites.containsAssignedElement(definitionPath, target)) {
    return;
  }
  retargetToPlaceHelper(target, *helper, suffix);
  Expr dereference = target;
  dereference.args.clear();
  dereference.argNames.clear();
  dereference.templateArgs.clear();
  dereference.templateArgDetails.clear();
  dereference.name = "dereference";
  dereference.sourceName.clear();
  dereference.resolvedCallPath.clear();
  dereference.semanticNodeId = 0;
  dereference.args.push_back(std::move(target));
  target = std::move(dereference);
}

void rewriteExpr(const std::string &definitionPath,
                 Expr &expr,
                 const VectorElementPlaceSites &sites) {
  rewriteAssignedElement(definitionPath, expr, sites);
  for (Expr &arg : expr.args) {
    rewriteExpr(definitionPath, arg, sites);
  }
  for (Expr &arg : expr.bodyArguments) {
    rewriteExpr(definitionPath, arg, sites);
  }
  std::string_view suffix;
  const ElementReadHelper *helper = elementReadHelper(expr, suffix);
  if (helper == nullptr || !sites.contains(definitionPath, expr)) {
    return;
  }
  retargetToPlaceHelper(expr, *helper, suffix);
}

} // namespace

void VectorElementPlaceSites::add(const std::string &definitionPath, const Expr &elementRead) {
  sites_.insert(siteKey(definitionPath, elementRead));
}

bool VectorElementPlaceSites::contains(const std::string &definitionPath,
                                       const Expr &elementRead) const {
  return sites_.count(siteKey(definitionPath, elementRead)) > 0;
}

void VectorElementPlaceSites::addAssignedElement(const std::string &definitionPath,
                                                 const Expr &elementRead) {
  assignedSites_.insert(siteKey(definitionPath, elementRead));
}

bool VectorElementPlaceSites::containsAssignedElement(const std::string &definitionPath,
                                                      const Expr &elementRead) const {
  return assignedSites_.count(siteKey(definitionPath, elementRead)) > 0;
}

void VectorElementPlaceSites::merge(const VectorElementPlaceSites &other) {
  sites_.insert(other.sites_.begin(), other.sites_.end());
  assignedSites_.insert(other.assignedSites_.begin(), other.assignedSites_.end());
}

bool isStdlibVectorElementRead(const Expr &expr) {
  std::string_view suffix;
  return elementReadHelper(expr, suffix) != nullptr;
}

void noteVectorElementPlaces(const std::string &definitionPath,
                             const Expr &place,
                             bool placeIsWholeValue,
                             VectorElementPlaceSites &sites) {
  const Expr *current = &place;
  bool writtenThrough = placeIsWholeValue;
  while (current->kind == Expr::Kind::Call && !current->args.empty()) {
    if (isFieldRead(*current)) {
      // A field of the element: the element is what the write goes through.
    } else if (!current->isFieldAccess && !current->isMethodCall && current->args.size() == 2) {
      if (isStdlibVectorElementRead(*current)) {
        if (writtenThrough) {
          sites.add(definitionPath, *current);
        }
      } else if (std::string accessName; !getBuiltinArrayAccessName(*current, accessName)) {
        // Any other call is a temporary; an element of a builtin collection is a place already.
        return;
      }
    } else {
      return;
    }
    writtenThrough = true;
    current = &current->args.front();
  }
}

void rewriteVectorElementPlaces(Program &program, const VectorElementPlaceSites &sites) {
  if (sites.empty()) {
    return;
  }
  for (Definition &def : program.definitions) {
    for (Expr &stmt : def.statements) {
      rewriteExpr(def.fullPath, stmt, sites);
    }
    if (def.returnExpr.has_value()) {
      rewriteExpr(def.fullPath, *def.returnExpr, sites);
    }
  }
}

} // namespace primec::semantics
