#include "SemanticsReadOnlyRoots.h"

namespace primec::semantics {

namespace {

bool carriesOwnWriteAccess(const BindingInfo &binding) {
  const std::string typeName = normalizeBindingTypeName(binding.typeName);
  return typeName == "Reference" || typeName == "Pointer";
}

bool isFieldRead(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && expr.isFieldAccess && expr.args.size() == 1;
}

// `items[i]`, `at(items, i)` and `at_unsafe(items, i)`: the shape of an element read. Whether `items` is a
// collection is the lookup's question (`readsCollectionElement`).
bool isElementReadShape(const Expr &expr) {
  std::string accessName;
  return expr.kind == Expr::Kind::Call && !expr.isFieldAccess && expr.args.size() == 2 &&
         getBuiltinArrayAccessName(expr, accessName);
}

// A builtin collection, or a stdlib one (a struct under `/std/collections/`, before or after monomorphization).
bool isCollection(const BindingInfo &binding) {
  const std::string typeName = normalizeBindingTypeName(binding.typeName);
  if (typeName.rfind("/std/collections/", 0) == 0 || typeName.rfind("std/collections/", 0) == 0) {
    return true;
  }
  return !binding.typeTemplateArg.empty() &&
         (typeName == "array" || typeName == "vector" || typeName == "map" || typeName == "args");
}

// Whether `expr` reads an element of a collection place whose elements are held by value: the element is then part
// of the collection's owner. An element that is a `Reference` or a `Pointer` carries its own write access.
bool readsOwnedCollectionElement(const Expr &expr, const ReadOnlyRootLookup &lookup) {
  BindingInfo collection;
  BindingInfo element;
  return isElementReadShape(expr) && lookup.typeOf &&
         lookup.typeOf(expr.args.front(), collection) && isCollection(collection) &&
         lookup.typeOf(expr, element) && !carriesOwnWriteAccess(element);
}

std::string placeText(const Expr &place) {
  if (place.kind == Expr::Kind::Name) {
    return place.name;
  }
  if (isFieldRead(place)) {
    return placeText(place.args.front()) + "." + place.name;
  }
  if (isElementReadShape(place)) {
    return placeText(place.args.front()) + "[...]";
  }
  return "?";
}

std::string wayOut(const ReadOnlyRoot &root) {
  switch (root.kind) {
  case ReadOnlyRoot::Kind::Parameter:
    return root.name + " is a read-only parameter; take it as a [T mut] parameter";
  case ReadOnlyRoot::Kind::Receiver:
    return "this is read-only in a method that is not [mut]; mark the method [mut]";
  case ReadOnlyRoot::Kind::Local:
    break;
  }
  return root.name + " is read-only; declare it with mut: [T mut] " + root.name + "{...}";
}

} // namespace

bool isClosureCallPath(const std::string &path) {
  const size_t slash = path.rfind('/');
  if (slash == std::string::npos || slash == 0 || path.compare(slash, std::string::npos, "/call") != 0) {
    return false;
  }
  const size_t parentSlash = path.rfind('/', slash - 1);
  const std::string parent = path.substr(parentSlash == std::string::npos ? 0 : parentSlash + 1,
                                         slash - (parentSlash == std::string::npos ? 0 : parentSlash + 1));
  return parent.rfind("__Function", 0) == 0 || parent.rfind("__fn_box", 0) == 0 ||
         parent.find("__lambda") != std::string::npos || parent.find("__fnref") != std::string::npos;
}

std::optional<ReadOnlyRoot> findReadOnlyRoot(const Expr &place, const ReadOnlyRootLookup &lookup,
                                             bool placeIsReceiver) {
  // Every place on the way from `place` to its root, `place` itself included when a method is called on it: a field
  // that cannot be resolved, a `Reference` or `Pointer` field, and anything that is not a field or an element of a
  // collection end the chain with nothing to report.
  const Expr *current = &place;
  bool onTheWay = placeIsReceiver;
  while (current->kind != Expr::Kind::Name) {
    if (isFieldRead(*current)) {
      BindingInfo field;
      if (onTheWay && (!lookup.fieldOf(current->args.front(), current->name, field) ||
                       carriesOwnWriteAccess(field))) {
        return std::nullopt;
      }
    } else if (!readsOwnedCollectionElement(*current, lookup)) {
      return std::nullopt;
    }
    current = &current->args.front();
    onTheWay = true;
  }
  bool isParameter = false;
  const BindingInfo *binding = lookup.bindingOf(current->name, isParameter);
  if (binding == nullptr || binding->isMutable) {
    return std::nullopt;
  }
  ReadOnlyRoot root;
  root.name = current->name;
  if (current->name == "this") {
    root.kind = ReadOnlyRoot::Kind::Receiver;
    return root;
  }
  if (carriesOwnWriteAccess(*binding)) {
    return std::nullopt;
  }
  root.kind = isParameter ? ReadOnlyRoot::Kind::Parameter : ReadOnlyRoot::Kind::Local;
  return root;
}

std::string readOnlyFieldWriteDiagnostic(const Expr &target, const ReadOnlyRoot &root) {
  return "assign target must be a mutable binding: " + placeText(target) + " (" + wayOut(root) + ")";
}

std::string readOnlyReceiverDiagnostic(const Expr &receiver, const std::string &methodName, const ReadOnlyRoot &root) {
  return "mutating method needs a mutable receiver: " + placeText(receiver) + "." + methodName + "() (" +
         wayOut(root) + ")";
}

} // namespace primec::semantics
