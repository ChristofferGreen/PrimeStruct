


struct SemanticProgram;

using BindingKindFromTransformsFn = std::function<LocalInfo::Kind(const Expr &)>;
using IsBindingTypeFn = std::function<bool(const Expr &)>;
using BindingValueKindFromTransformsFn =
    std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using SetReferenceArrayInfoFn = std::function<void(const Expr &, LocalInfo &)>;

struct BindingTypeAdapters {
  BindingKindFromTransformsFn bindingKind{};
  IsBindingTypeFn isStringBinding{};
  IsBindingTypeFn isFileErrorBinding{};
  BindingValueKindFromTransformsFn bindingValueKind{};
  SetReferenceArrayInfoFn setReferenceArrayInfo{};
};

BindingTypeAdapters makeBindingTypeAdapters(const SemanticProgram *semanticProgram = nullptr);
BindingKindFromTransformsFn makeBindingKindFromTransforms();
IsBindingTypeFn makeIsStringBindingType();
IsBindingTypeFn makeIsFileErrorBindingType();
BindingValueKindFromTransformsFn makeBindingValueKindFromTransforms();
SetReferenceArrayInfoFn makeSetReferenceArrayInfoFromTransforms();

std::string normalizeCollectionBindingTypeName(const std::string &name);
LocalInfo::Kind bindingKindFromTransforms(const Expr &expr);
bool isStringTypeName(const std::string &name);
bool isStringBindingType(const Expr &expr);
bool isFileErrorBindingType(const Expr &expr);
LocalInfo::ValueKind bindingValueKindFromTransforms(const Expr &expr, LocalInfo::Kind kind);
void setReferenceArrayInfoFromTransforms(const Expr &expr, LocalInfo &info);
