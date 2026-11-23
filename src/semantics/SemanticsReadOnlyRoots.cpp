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
// collection is the lookup's question (`readsOwnedCollectionElement`).
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

// The place as the program spells it; a capture is spelled by its name, not as the `this.name` the body reads.
std::string placeText(const Expr &place, const ReadOnlyRoot &root) {
  if (place.kind == Expr::Kind::Name) {
    return place.name;
  }
  if (isFieldRead(place)) {
    const Expr &receiver = place.args.front();
    if (root.kind == ReadOnlyRoot::Kind::Capture && receiver.kind == Expr::Kind::Name &&
        receiver.name == "this") {
      return place.name;
    }
    return placeText(receiver, root) + "." + place.name;
  }
  if (isElementReadShape(place)) {
    return placeText(place.args.front(), root) + "[...]";
  }
  return "?";
}

std::string wayOut(const ReadOnlyRoot &root) {
  switch (root.kind) {
  case ReadOnlyRoot::Kind::Parameter:
    return root.name + " is a read-only parameter; take it as a [T " +
           (root.parameterMode.empty() ? "" : root.parameterMode + " ") + "mut] parameter";
  case ReadOnlyRoot::Kind::Receiver:
    return "this is read-only in a method that is not [mut]; mark the method [mut]";
  case ReadOnlyRoot::Kind::Capture:
    return root.name + " is a capture that is not mut; capture it with mut: [mut " + root.name +
           "]";
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

bool isFunctionCallPath(const std::string &path) {
  const size_t slash = path.rfind('/');
  const size_t parentSlash =
      slash == std::string::npos || slash == 0 ? std::string::npos : path.rfind('/', slash - 1);
  return isClosureCallPath(path) &&
         path.compare(parentSlash == std::string::npos ? 0 : parentSlash + 1, 10, "__Function") ==
             0;
}

std::optional<ReadOnlyRoot> findReadOnlyRoot(const Expr &place, const ReadOnlyRootLookup &lookup,
                                             bool placeIsReceiver) {
  // Every place on the way from `place` to its root, `place` itself included when a method is called on it: a field
  // that cannot be resolved, a `Reference` or `Pointer` field, and anything that is not a field or an element of a
  // collection end the chain with nothing to report.
  const Expr *current = &place;
  const Expr *above = nullptr;
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
    above = current;
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
    if (lookup.inClosureBody && above != nullptr && isFieldRead(*above)) {
      root.kind = ReadOnlyRoot::Kind::Capture;
      root.name = above->name;
    }
    return root;
  }
  if (carriesOwnWriteAccess(*binding)) {
    return std::nullopt;
  }
  root.kind = isParameter ? ReadOnlyRoot::Kind::Parameter : ReadOnlyRoot::Kind::Local;
  if (isParameter) {
    root.parameterMode = binding->isCopy ? "copy" : (binding->isMove ? "move" : "");
  }
  return root;
}

std::string readOnlyFieldWriteDiagnostic(const Expr &target, const ReadOnlyRoot &root) {
  return "assign target must be a mutable binding: " + placeText(target, root) + " (" +
         wayOut(root) + ")";
}

std::string readOnlyReceiverDiagnostic(const Expr &receiver, const std::string &methodName, const ReadOnlyRoot &root) {
  return "mutating method needs a mutable receiver: " + placeText(receiver, root) + "." +
         methodName + "() (" + wayOut(root) + ")";
}

std::string
readOnlyCallableDiagnostic(const Expr &callable, const ReadOnlyRoot &root, bool isFunction) {
  const std::string place = placeText(callable, root);
  std::string fix = wayOut(root);
  // A read-only local or capture that is the callable itself has one fix; a callable reached through anything else
  // can also be copied, and the copy called.
  const bool rootIsCallable =
      callable.kind == Expr::Kind::Name || root.kind == ReadOnlyRoot::Kind::Capture;
  if (!(rootIsCallable && root.kind != ReadOnlyRoot::Kind::Parameter)) {
    fix += std::string(", or call a mut copy: ") + (isFunction ? "[Function<...> mut]" : "[mut]") +
           " h{" + place + "}";
  }
  return "mutating method needs a mutable receiver: " + place + ".call() (" + fix + ")";
}

} // namespace primec::semantics
