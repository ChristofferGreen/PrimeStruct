#include "SemanticsValidateExperimentalGfxConstructors.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateExperimentalGfxConstructorsInternal.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace primec::semantics {

bool rewriteExperimentalGfxConstructors(Program &program, std::string &error) {
  (void)error;
  ExperimentalGfxRewriteContext rewriteContext = buildExperimentalGfxRewriteContext(program);
  const auto &structNames = rewriteContext.structNames;
  const auto &importAliases = rewriteContext.importAliases;

  auto resolveImportedStructPath = [&](const std::string &name, const std::string &namespacePrefix) {
    return rewriteContext.resolveImportedStructPath(name, namespacePrefix);
  };
  auto hasImportedDefinitionPath = [&](const std::string &path) {
    return hasExperimentalGfxImportedDefinitionPath(program, path);
  };

  enum class BufferBindingInitKind {
    Unknown,
    RawStruct,
    Substrate,
  };

  struct LocalBindingState {
    BindingInfo binding;
    BufferBindingInitKind bufferInitKind = BufferBindingInitKind::Unknown;
    std::optional<Expr> bufferTokenExpr;
    std::optional<Expr> bufferElementCountExpr;
  };

  using LocalBindings = std::unordered_map<std::string, LocalBindingState>;

  auto resolveBindingStructPath = [&](const BindingInfo &binding, const std::string &namespacePrefix) {
    std::string bindingTypePath = binding.typeName;
    if (!bindingTypePath.empty() && bindingTypePath.front() == '/') {
      const size_t suffix = bindingTypePath.find("__t");
      if (suffix != std::string::npos) {
        bindingTypePath.erase(suffix);
      }
      if (structNames.count(bindingTypePath) > 0) {
        return bindingTypePath;
      }
    }
    std::string resolved = resolveImportedStructPath(binding.typeName, namespacePrefix);
    if (!resolved.empty()) {
      return resolved;
    }
    return resolveImportedStructPath(normalizeBindingTypeName(binding.typeName), namespacePrefix);
  };

  auto resolveBufferStructPath = [&](const Expr &expr,
                                     const std::string &namespacePrefix,
                                     const LocalBindings &locals) -> std::string {
    if (expr.kind == Expr::Kind::Name) {
      auto it = locals.find(expr.name);
      if (it != locals.end()) {
        return resolveBindingStructPath(it->second.binding, namespacePrefix);
      }
      return resolveImportedStructPath(expr.name, expr.namespacePrefix);
    }
    if (expr.kind == Expr::Kind::Call && !expr.isBinding) {
      return resolveImportedStructPath(expr.name, expr.namespacePrefix);
    }
    return "";
  };
  auto isBufferTypeText = [&](const std::string &typeText) {
    std::string base;
    std::string nestedArg;
    return splitTemplateTypeName(normalizeBindingTypeName(typeText), base, nestedArg) &&
           normalizeBindingTypeName(base) == "Buffer" &&
           !nestedArg.empty();
  };
  auto isWrappedBufferTypeText = [&](const std::string &typeText) {
    std::string base;
    std::string nestedArg;
    if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, nestedArg) ||
        (normalizeBindingTypeName(base) != "Reference" &&
         normalizeBindingTypeName(base) != "Pointer") ||
        nestedArg.empty()) {
      return false;
    }
    return isBufferTypeText(nestedArg);
  };
  auto bindingTypeText = [&](const BindingInfo &binding) {
    return binding.typeTemplateArg.empty()
               ? binding.typeName
               : binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto isFileHandleTypeText = [&](const std::string &typeText) {
    std::string base;
    std::string nestedArg;
    return splitTemplateTypeName(normalizeBindingTypeName(typeText), base, nestedArg) &&
           normalizeBindingTypeName(base) == "File" && !nestedArg.empty();
  };
  auto isKnownFileHandleReceiver = [&](const Expr &expr, const LocalBindings &locals) {
    if (expr.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = locals.find(expr.name);
    return it != locals.end() && isFileHandleTypeText(bindingTypeText(it->second.binding));
  };
  auto normalizeFileMethodName = [](const std::string &methodName) {
    if (methodName == "readByte") {
      return std::string("read_byte");
    }
    if (methodName == "writeLine") {
      return std::string("write_line");
    }
    if (methodName == "writeByte") {
      return std::string("write_byte");
    }
    if (methodName == "writeBytes") {
      return std::string("write_bytes");
    }
    return methodName;
  };
  auto resolveIndexedArgsPackElementTypeText =
      [&](const Expr &expr, const LocalBindings &localsIn, std::string &elemTypeOut) -> bool {
    std::string accessName;
    auto isExplicitArrayAccessAlias = [&](const Expr &candidate) {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
        return false;
      }
      std::string name = candidate.name;
      if (!name.empty() && name.front() == '/') {
        name.erase(0, 1);
      }
      return name == "array/at" || name == "array/at_unsafe";
    };
    if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.args.size() != 2 ||
        (!getBuiltinArrayAccessName(expr, accessName) && !isExplicitArrayAccessAlias(expr))) {
      return false;
    }
    const Expr *accessReceiver = expr.isMethodCall
                                     ? &expr.args.front()
                                     : &expr.args.front();
    if (accessReceiver == nullptr || accessReceiver->kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(accessReceiver->name);
    return it != localsIn.end() &&
           getArgsPackElementType(it->second.binding, elemTypeOut);
  };
  auto isKnownBufferReceiver = [&](const Expr &expr, const LocalBindings &locals) -> bool {
    if (expr.kind == Expr::Kind::Name) {
      auto it = locals.find(expr.name);
      if (it != locals.end() && normalizeBindingTypeName(it->second.binding.typeName) == "Buffer") {
        return true;
      }
    }
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall && !expr.isBinding &&
        (isSimpleCallName(expr, "buffer") || isSimpleCallName(expr, "upload"))) {
      return true;
    }
    if (expr.kind == Expr::Kind::Call && !expr.isBinding) {
      std::string indexedElemType;
      if (resolveIndexedArgsPackElementTypeText(expr, locals, indexedElemType) &&
          isBufferTypeText(indexedElemType)) {
        return true;
      }
      if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
        const Expr &target = expr.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto it = locals.find(target.name);
          if (it != locals.end() &&
              isWrappedBufferTypeText(bindingTypeText(it->second.binding))) {
            return true;
          }
        }
        if (resolveIndexedArgsPackElementTypeText(target, locals, indexedElemType) &&
            isWrappedBufferTypeText(indexedElemType)) {
          return true;
        }
      }
    }
    return false;
  };

  auto rememberBinding = [&](const Expr &expr, const std::string &namespacePrefix, LocalBindings &locals) {
    if (!expr.isBinding) {
      return;
    }
    BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(expr, namespacePrefix, structNames, importAliases, binding, restrictType, parseError)) {
      return;
    }
    LocalBindingState state;
    state.binding = std::move(binding);
    if (normalizeBindingTypeName(state.binding.typeName) == "Buffer" && expr.args.size() == 1) {
      const Expr &initializer = expr.args.front();
      if (initializer.kind == Expr::Kind::Call && !initializer.isMethodCall && !initializer.isBinding) {
        if (initializer.name == "buffer" || initializer.name == "/std/gpu/buffer") {
          state.bufferInitKind = BufferBindingInitKind::Substrate;
          if (initializer.args.size() == 1) {
            state.bufferElementCountExpr = initializer.args.front();
          }
        } else if (initializer.name == "upload" || initializer.name == "/std/gpu/upload") {
          state.bufferInitKind = BufferBindingInitKind::Substrate;
          if (initializer.args.size() == 1) {
            Expr countExpr;
            countExpr.kind = Expr::Kind::Call;
            countExpr.name = "count";
            countExpr.args.push_back(initializer.args.front());
            state.bufferElementCountExpr = std::move(countExpr);
          }
        } else {
          std::string resolvedInitPath = resolveImportedStructPath(initializer.name, initializer.namespacePrefix);
          if (resolvedInitPath == "/std/gfx/Buffer" || resolvedInitPath == "/std/gfx/experimental/Buffer" ||
              initializer.name.rfind("/std/gfx/Buffer__t", 0) == 0 ||
              initializer.name.rfind("/std/gfx/experimental/Buffer__t", 0) == 0 ||
              initializer.name == "Buffer") {
            state.bufferInitKind = BufferBindingInitKind::RawStruct;
            for (size_t i = 0; i < initializer.args.size(); ++i) {
              const std::optional<std::string> argName =
                  i < initializer.argNames.size() ? initializer.argNames[i] : std::nullopt;
              if (argName.has_value()) {
                if (*argName == "token") {
                  state.bufferTokenExpr = initializer.args[i];
                } else if (*argName == "elementCount") {
                  state.bufferElementCountExpr = initializer.args[i];
                }
                continue;
              }
              if (i == 0 && !state.bufferTokenExpr.has_value()) {
                state.bufferTokenExpr = initializer.args[i];
              } else if (i == 1 && !state.bufferElementCountExpr.has_value()) {
                state.bufferElementCountExpr = initializer.args[i];
              }
            }
          }
        }
      }
    }
    locals[expr.name] = std::move(state);
  };

  auto makeI32Literal = [](int64_t value) {
    Expr literal;
    literal.kind = Expr::Kind::Literal;
    literal.literalValue = static_cast<uint64_t>(value);
    literal.intWidth = 32;
    literal.isUnsigned = false;
    return literal;
  };

  auto makeBareCountCall = [&](Expr receiver) {
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = "count";
    call.args.push_back(std::move(receiver));
    return call;
  };

  auto makeBufferCountComparison = [&](Expr receiver, int64_t lhsValue, int64_t rhsValue) {
    Expr comparison;
    comparison.kind = Expr::Kind::Call;
    comparison.name = "less_than";
    if (rhsValue >= 0) {
      comparison.args.push_back(makeBareCountCall(std::move(receiver)));
      comparison.args.push_back(makeI32Literal(rhsValue));
    } else {
      comparison.args.push_back(makeI32Literal(lhsValue));
      comparison.args.push_back(makeBareCountCall(std::move(receiver)));
    }
    return comparison;
  };

  auto makeLessThanExpr = [&](Expr lhs, Expr rhs) {
    Expr comparison;
    comparison.kind = Expr::Kind::Call;
    comparison.name = "less_than";
    comparison.args.push_back(std::move(lhs));
    comparison.args.push_back(std::move(rhs));
    return comparison;
  };

  auto resolveRawBufferFieldExpr =
      [&](const Expr &expr, const LocalBindings &locals, const char *fieldName) -> std::optional<Expr> {
    if (expr.kind != Expr::Kind::Name) {
      return std::nullopt;
    }
    auto it = locals.find(expr.name);
    if (it == locals.end()) {
      return std::nullopt;
    }
    if (std::string_view(fieldName) == "token") {
      return it->second.bufferTokenExpr;
    }
    if (std::string_view(fieldName) == "elementCount") {
      return it->second.bufferElementCountExpr;
    }
    return std::nullopt;
  };

  auto isFileOpenWrapperDefinition = [](const std::string &definitionPath) {
    return definitionPath == "/File/open_read" || definitionPath == "/File/open_write" ||
           definitionPath == "/File/open_append" || definitionPath == "/File/openRead" ||
           definitionPath == "/File/openWrite" || definitionPath == "/File/openAppend" ||
           definitionPath == "/std/file/File/open_read" ||
           definitionPath == "/std/file/File/open_write" || definitionPath == "/std/file/File/open_append" ||
           definitionPath == "/std/file/File/openRead" ||
           definitionPath == "/std/file/File/openWrite" ||
           definitionPath == "/std/file/File/openAppend";
  };

  auto rewriteExpr = [&](auto &self, Expr &expr, const std::string &namespacePrefix, const std::string &currentDefinitionPath,
                         const LocalBindings &locals) -> bool {
    for (auto &arg : expr.args) {
      if (!self(self, arg, namespacePrefix, currentDefinitionPath, locals)) {
        return false;
      }
    }
    LocalBindings bodyLocals = locals;
    for (auto &arg : expr.bodyArguments) {
      if (!self(self, arg, namespacePrefix, currentDefinitionPath, bodyLocals)) {
        return false;
      }
      rememberBinding(arg, namespacePrefix, bodyLocals);
    }
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return true;
    }
    if (!expr.isMethodCall) {
      if (!isFileOpenWrapperDefinition(currentDefinitionPath) && isSimpleCallName(expr, "File") &&
          expr.templateArgs.size() == 1 && expr.args.size() == 1 && !expr.hasBodyArguments &&
          expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames)) {
        if (expr.templateArgs.front() == "Read" && hasImportedDefinitionPath("/File/open_read")) {
          expr.name = "/File/open_read";
          expr.namespacePrefix.clear();
          expr.templateArgs.clear();
          return true;
        }
        if (expr.templateArgs.front() == "Write" && hasImportedDefinitionPath("/File/open_write")) {
          expr.name = "/File/open_write";
          expr.namespacePrefix.clear();
          expr.templateArgs.clear();
          return true;
        }
        if (expr.templateArgs.front() == "Append" && hasImportedDefinitionPath("/File/open_append")) {
          expr.name = "/File/open_append";
          expr.namespacePrefix.clear();
          expr.templateArgs.clear();
          return true;
        }
      }
      if ((expr.name == "/File/read_byte" || expr.name == "/File/readByte") &&
          hasImportedDefinitionPath("/File/read_byte")) {
        expr.isMethodCall = true;
        expr.name = "read_byte";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        return true;
      }
      const bool isBroaderStdlibFileWriteFacade =
          expr.name == "/File/write" || expr.name == "/File/write_line" ||
          expr.name == "/std/file/File/write" ||
          expr.name == "/std/file/File/write_line";
      const bool hasVisibleBroaderStdlibFileWriteFacade =
          hasImportedDefinitionPath(expr.name) ||
          (expr.name == "/std/file/File/write" &&
           hasImportedDefinitionPath("/File/write")) ||
          (expr.name == "/std/file/File/write_line" &&
           hasImportedDefinitionPath("/File/write_line"));
      if (isBroaderStdlibFileWriteFacade &&
          expr.args.size() > 10 &&
          hasVisibleBroaderStdlibFileWriteFacade) {
        expr.isMethodCall = true;
        expr.name =
            (expr.name == "/File/write" || expr.name == "/std/file/File/write")
                ? "write"
                : "write_line";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        return true;
      }
      if ((expr.name == "/File/write_byte" || expr.name == "/File/writeByte") &&
          hasImportedDefinitionPath("/File/write_byte")) {
        expr.isMethodCall = true;
        expr.name = "write_byte";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        return true;
      }
      if ((expr.name == "/File/write_bytes" || expr.name == "/File/writeBytes") &&
          hasImportedDefinitionPath("/File/write_bytes")) {
        expr.isMethodCall = true;
        expr.name = "write_bytes";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        return true;
      }
      if (expr.name == "/File/flush" && hasImportedDefinitionPath("/File/flush")) {
        expr.isMethodCall = true;
        expr.name = "flush";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        return true;
      }
      if ((expr.name == "/std/gfx/Buffer/allocate" || expr.name == "/std/gfx/experimental/Buffer/allocate") &&
          hasImportedDefinitionPath(expr.name)) {
        expr.name = "buffer";
        expr.namespacePrefix.clear();
        return true;
      }
      if ((expr.name == "/std/gfx/Buffer/upload" || expr.name == "/std/gfx/experimental/Buffer/upload") &&
          hasImportedDefinitionPath(expr.name)) {
        expr.name = "upload";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        return true;
      }
      if ((expr.name == "/std/gfx/Buffer/readback" || expr.name == "/std/gfx/experimental/Buffer/readback") &&
          hasImportedDefinitionPath(expr.name)) {
        expr.name = "readback";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        return true;
      }
      if ((expr.name == "/std/gfx/Buffer/load" || expr.name == "/std/gfx/experimental/Buffer/load") &&
          hasImportedDefinitionPath(expr.name)) {
        expr.name = "buffer_load";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        expr.isMethodCall = false;
        return true;
      }
      if ((expr.name == "/std/gfx/Buffer/store" || expr.name == "/std/gfx/experimental/Buffer/store") &&
          hasImportedDefinitionPath(expr.name)) {
        expr.name = "buffer_store";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        expr.isMethodCall = false;
        return true;
      }
      const std::string directReceiverPath =
          expr.args.empty() ? std::string() : resolveBufferStructPath(expr.args.front(), namespacePrefix, locals);
      const bool isDirectCanonicalBuffer = directReceiverPath == "/std/gfx/Buffer";
      const bool isDirectExperimentalBuffer = directReceiverPath == "/std/gfx/experimental/Buffer";
      const bool isDirectKnownBuffer =
          !expr.args.empty() && isKnownBufferReceiver(expr.args.front(), locals);
      if ((expr.name == "/std/gfx/Buffer/count" || expr.name == "/std/gfx/experimental/Buffer/count") &&
          hasImportedDefinitionPath(expr.name) && expr.args.size() == 1 &&
          (isDirectCanonicalBuffer || isDirectExperimentalBuffer || isDirectKnownBuffer)) {
        if (std::optional<Expr> elementCountExpr = resolveRawBufferFieldExpr(expr.args.front(), locals, "elementCount")) {
          expr = *elementCountExpr;
          return true;
        }
        expr.name = "count";
        expr.namespacePrefix.clear();
        expr.templateArgs.clear();
        expr.isMethodCall = false;
        return true;
      }
      if ((expr.name == "/std/gfx/Buffer/empty" || expr.name == "/std/gfx/experimental/Buffer/empty") &&
          hasImportedDefinitionPath(expr.name) && expr.args.size() == 1 &&
          (isDirectCanonicalBuffer || isDirectExperimentalBuffer || isDirectKnownBuffer)) {
        if (std::optional<Expr> elementCountExpr = resolveRawBufferFieldExpr(expr.args.front(), locals, "elementCount")) {
          expr = makeLessThanExpr(std::move(*elementCountExpr), makeI32Literal(1));
          return true;
        }
        expr = makeBufferCountComparison(std::move(expr.args.front()), 0, 1);
        return true;
      }
      if ((expr.name == "/std/gfx/Buffer/is_valid" || expr.name == "/std/gfx/experimental/Buffer/is_valid") &&
          hasImportedDefinitionPath(expr.name) && expr.args.size() == 1 &&
          (isDirectCanonicalBuffer || isDirectExperimentalBuffer || isDirectKnownBuffer)) {
        if (std::optional<Expr> tokenExpr = resolveRawBufferFieldExpr(expr.args.front(), locals, "token")) {
          expr = makeLessThanExpr(makeI32Literal(0), std::move(*tokenExpr));
          return true;
        }
        expr = makeBufferCountComparison(std::move(expr.args.front()), 0, -1);
        return true;
      }
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return true;
    }
    if (expr.isMethodCall) {
      if (!expr.args.empty() && isKnownFileHandleReceiver(expr.args.front(), locals)) {
        expr.name = normalizeFileMethodName(expr.name);
      }
      if (!expr.args.empty()) {
        const std::string receiverPath = resolveBufferStructPath(expr.args.front(), namespacePrefix, locals);
        const bool isCanonicalBuffer = receiverPath == "/std/gfx/Buffer";
        const bool isExperimentalBuffer = receiverPath == "/std/gfx/experimental/Buffer";
        const bool isKnownBuffer = isKnownBufferReceiver(expr.args.front(), locals);
        if ((isCanonicalBuffer || isExperimentalBuffer) && expr.name == "readback" && expr.args.size() == 1) {
            expr.isMethodCall = false;
            expr.name = "readback";
            expr.namespacePrefix.clear();
            expr.templateArgs.clear();
            return true;
        }
        if ((isCanonicalBuffer || isExperimentalBuffer || isKnownBuffer) && expr.name == "load" && expr.args.size() == 2) {
          expr.isMethodCall = false;
          expr.name = "buffer_load";
          expr.namespacePrefix.clear();
          expr.templateArgs.clear();
          return true;
        }
        if ((isCanonicalBuffer || isExperimentalBuffer || isKnownBuffer) && expr.name == "store" && expr.args.size() == 3) {
          expr.isMethodCall = false;
          expr.name = "buffer_store";
          expr.namespacePrefix.clear();
          expr.templateArgs.clear();
          return true;
        }
        if ((isCanonicalBuffer || isExperimentalBuffer || isKnownBuffer) && expr.name == "count" && expr.args.size() == 1) {
          if (std::optional<Expr> elementCountExpr = resolveRawBufferFieldExpr(expr.args.front(), locals, "elementCount")) {
            expr = *elementCountExpr;
            return true;
          }
          expr.isMethodCall = false;
          expr.name = "count";
          expr.namespacePrefix.clear();
          expr.templateArgs.clear();
          return true;
        }
        if ((isCanonicalBuffer || isExperimentalBuffer || isKnownBuffer) && expr.name == "empty" && expr.args.size() == 1) {
          if (std::optional<Expr> elementCountExpr = resolveRawBufferFieldExpr(expr.args.front(), locals, "elementCount")) {
            expr = makeLessThanExpr(std::move(*elementCountExpr), makeI32Literal(1));
            return true;
          }
          expr = makeBufferCountComparison(std::move(expr.args.front()), 0, 1);
          return true;
        }
        if ((isCanonicalBuffer || isExperimentalBuffer || isKnownBuffer) && expr.name == "is_valid" && expr.args.size() == 1) {
          if (std::optional<Expr> tokenExpr = resolveRawBufferFieldExpr(expr.args.front(), locals, "token")) {
            expr = makeLessThanExpr(makeI32Literal(0), std::move(*tokenExpr));
            return true;
          }
          expr = makeBufferCountComparison(std::move(expr.args.front()), 0, -1);
          return true;
        }
      }
      if (expr.args.empty() || expr.name != "create_pipeline") {
        return true;
      }
      const Expr &receiverExpr = expr.args.front();
      std::string receiverPath;
      if (receiverExpr.kind == Expr::Kind::Name) {
        auto localIt = locals.find(receiverExpr.name);
        if (localIt != locals.end()) {
          receiverPath = resolveBindingStructPath(localIt->second.binding, namespacePrefix);
        } else {
          receiverPath = resolveImportedStructPath(receiverExpr.name, receiverExpr.namespacePrefix);
        }
      }
      const bool isExperimentalDevice = receiverPath == "/std/gfx/experimental/Device";
      const bool isCanonicalDevice = receiverPath == "/std/gfx/Device";
      if (!isExperimentalDevice && !isCanonicalDevice) {
        return true;
      }
      size_t vertexTypeIndex = expr.args.size();
      for (size_t i = 0; i < expr.argNames.size() && i < expr.args.size(); ++i) {
        if (expr.argNames[i].has_value() && *expr.argNames[i] == "vertex_type") {
          vertexTypeIndex = i;
          break;
        }
      }
      if (vertexTypeIndex >= expr.args.size()) {
        return true;
      }
      const Expr &vertexTypeExpr = expr.args[vertexTypeIndex];
      if (vertexTypeExpr.kind != Expr::Kind::Name) {
        error = "experimental gfx create_pipeline requires bare struct type on [vertex_type]";
        return false;
      }
      const std::string vertexTypePath = resolveImportedStructPath(vertexTypeExpr.name, vertexTypeExpr.namespacePrefix);
      const bool isExperimentalVertexType = vertexTypePath == "/std/gfx/experimental/VertexColored";
      const bool isCanonicalVertexType = vertexTypePath == "/std/gfx/VertexColored";
      if (!isExperimentalVertexType && !isCanonicalVertexType) {
        error = "experimental gfx create_pipeline currently supports only VertexColored";
        return false;
      }
      expr.name = "create_pipeline_VertexColored";
      expr.args.erase(expr.args.begin() + static_cast<std::ptrdiff_t>(vertexTypeIndex));
      expr.argNames.erase(expr.argNames.begin() + static_cast<std::ptrdiff_t>(vertexTypeIndex));
      return true;
    }
    std::string resolved = resolveImportedStructPath(expr.name, expr.namespacePrefix);
    if ((resolved == "/std/gfx/experimental/Buffer" || resolved == "/std/gfx/Buffer") && expr.args.size() == 1 &&
        !expr.hasBodyArguments && expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames)) {
      expr.name = "buffer";
      expr.namespacePrefix.clear();
      return true;
    }
    if (!expr.templateArgs.empty()) {
      return true;
    }
    if (resolved.empty()) {
      return true;
    }
    if ((resolved == "/std/gfx/experimental/Window" || resolved == "/std/gfx/Window") && expr.args.size() == 3) {
      bool sawTitleArg = false;
      for (const auto &argName : expr.argNames) {
        if (argName.has_value() && *argName == "title") {
          sawTitleArg = true;
          break;
        }
      }
      if (sawTitleArg) {
        expr.name = resolved == "/std/gfx/Window" ? "/std/gfx/windowCreate" : "/std/gfx/experimental/windowCreate";
        expr.namespacePrefix.clear();
      }
      return true;
    }
    if ((resolved == "/std/gfx/experimental/Device" || resolved == "/std/gfx/Device") && expr.args.empty() &&
        !expr.hasBodyArguments && expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames)) {
      expr.name = resolved == "/std/gfx/Device" ? "/std/gfx/deviceCreate" : "/std/gfx/experimental/deviceCreate";
      expr.namespacePrefix.clear();
      return true;
    }
    return true;
  };

  for (auto &def : program.definitions) {
    LocalBindings locals;
    for (auto &param : def.parameters) {
      if (!rewriteExpr(rewriteExpr, param, def.namespacePrefix, def.fullPath, locals)) {
        return false;
      }
      rememberBinding(param, def.namespacePrefix, locals);
    }
    for (auto &stmt : def.statements) {
      if (!rewriteExpr(rewriteExpr, stmt, def.namespacePrefix, def.fullPath, locals)) {
        return false;
      }
      rememberBinding(stmt, def.namespacePrefix, locals);
    }
    if (def.returnExpr.has_value()) {
      if (!rewriteExpr(rewriteExpr, *def.returnExpr, def.namespacePrefix, def.fullPath, locals)) {
        return false;
      }
    }
  }
  for (auto &exec : program.executions) {
    LocalBindings locals;
    for (auto &arg : exec.arguments) {
      if (!rewriteExpr(rewriteExpr, arg, exec.namespacePrefix, exec.fullPath, locals)) {
        return false;
      }
      rememberBinding(arg, exec.namespacePrefix, locals);
    }
    for (auto &arg : exec.bodyArguments) {
      if (!rewriteExpr(rewriteExpr, arg, exec.namespacePrefix, exec.fullPath, locals)) {
        return false;
      }
      rememberBinding(arg, exec.namespacePrefix, locals);
    }
  }
  return true;
}

} // namespace primec::semantics
