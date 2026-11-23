#include "SemanticsReadOnlyRoots.h"

namespace primec::semantics {

namespace {

bool carriesOwnWriteAccess(const BindingInfo &binding) {
  const std::string typeName = normalizeBindingTypeName(binding.typeName);
  return typeName == "Reference" || typeName == "Pointer";
}

std::string placeText(const Expr &place) {
  if (place.kind == Expr::Kind::Name) {
    return place.name;
  }
  if (place.kind == Expr::Kind::Call && place.isFieldAccess && place.args.size() == 1) {
    return placeText(place.args.front()) + "." + place.name;
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
  const Expr *current = &place;
  if (placeIsReceiver && place.kind == Expr::Kind::Call && place.isFieldAccess && place.args.size() == 1) {
    BindingInfo lastField;
    if (!lookup.fieldOf(place.args.front(), place.name, lastField) || carriesOwnWriteAccess(lastField)) {
      return std::nullopt;
    }
  }
  while (current->kind == Expr::Kind::Call && current->isFieldAccess && current->args.size() == 1) {
    const Expr &receiver = current->args.front();
    // A field on the way that is a `Reference` or a `Pointer` writes to its referent, whatever the root is.
    if (receiver.kind == Expr::Kind::Call && receiver.isFieldAccess && receiver.args.size() == 1) {
      BindingInfo viaField;
      if (!lookup.fieldOf(receiver.args.front(), receiver.name, viaField) || carriesOwnWriteAccess(viaField)) {
        return std::nullopt;
      }
    }
    current = &receiver;
  }
  if (current->kind != Expr::Kind::Name) {
    return std::nullopt;
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
