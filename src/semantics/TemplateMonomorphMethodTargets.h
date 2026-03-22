#pragma once

bool resolveMethodCallTemplateTarget(const Expr &expr,
                                     const LocalTypeMap &locals,
                                     const Context &ctx,
                                     std::string &pathOut) {
  pathOut.clear();
  if (!expr.isMethodCall || expr.args.empty() || expr.name.empty()) {
    return false;
  }
  const std::string rawMethodName = expr.name;
  std::string methodName = rawMethodName;
  if (!methodName.empty() && methodName.front() == '/') {
    methodName.erase(methodName.begin());
  }
  auto normalizeCollectionMethodName = [](const std::string &receiverTypeName,
                                          std::string candidate) -> std::string {
    if (receiverTypeName == "array" || receiverTypeName == "vector" || receiverTypeName == "soa_vector") {
      const std::string vectorPrefix = "vector/";
      const std::string arrayPrefix = "array/";
      const std::string stdVectorPrefix = "std/collections/vector/";
      if (candidate.rfind(vectorPrefix, 0) == 0) {
        return candidate.substr(vectorPrefix.size());
      }
      if (candidate.rfind(arrayPrefix, 0) == 0) {
        return candidate.substr(arrayPrefix.size());
      }
      if (candidate.rfind(stdVectorPrefix, 0) == 0) {
        return candidate.substr(stdVectorPrefix.size());
      }
    }
    if (receiverTypeName == "map") {
      const std::string mapPrefix = "map/";
      const std::string stdMapPrefix = "std/collections/map/";
      if (candidate.rfind(mapPrefix, 0) == 0) {
        return candidate.substr(mapPrefix.size());
      }
      if (candidate.rfind(stdMapPrefix, 0) == 0) {
        return candidate.substr(stdMapPrefix.size());
      }
    }
    return candidate;
  };
  std::function<std::string(std::string)> qualifyImportedCollectionTypeText =
      [&](std::string typeText) -> std::string {
    typeText = normalizeBindingTypeName(typeText);
    if (typeText.empty()) {
      return typeText;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(typeText, base, argText) && !base.empty()) {
      base = normalizeBindingTypeName(base);
      if ((base == "Reference" || base == "Pointer") && !argText.empty()) {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
          return typeText;
        }
        return base + "<" + qualifyImportedCollectionTypeText(args.front()) + ">";
      }
      auto aliasIt = ctx.importAliases.find(base);
      if (aliasIt != ctx.importAliases.end()) {
        return aliasIt->second + "<" + argText + ">";
      }
      return typeText;
    }
    auto aliasIt = ctx.importAliases.find(typeText);
    if (aliasIt != ctx.importAliases.end()) {
      return aliasIt->second;
    }
    return typeText;
  };
  auto unwrapImportedCollectionReceiverType = [&](const BindingInfo &binding) {
    std::string typeText = binding.typeName;
    if (!binding.typeTemplateArg.empty()) {
      typeText += "<" + binding.typeTemplateArg + ">";
    }
    return unwrapCollectionReceiverEnvelope(qualifyImportedCollectionTypeText(typeText));
  };
  const Expr &receiver = expr.args.front();
  std::string typeName;
  if (receiver.kind == Expr::Kind::Name) {
    auto it = locals.find(receiver.name);
    if (it != locals.end()) {
      typeName = unwrapImportedCollectionReceiverType(it->second);
    }
  } else if (receiver.kind == Expr::Kind::Literal) {
    typeName = receiver.isUnsigned ? "u64" : (receiver.intWidth == 64 ? "i64" : "i32");
  } else if (receiver.kind == Expr::Kind::BoolLiteral) {
    typeName = "bool";
  } else if (receiver.kind == Expr::Kind::FloatLiteral) {
    typeName = receiver.floatWidth == 64 ? "f64" : "f32";
  } else if (receiver.kind == Expr::Kind::StringLiteral) {
    typeName = "string";
  } else if (receiver.kind == Expr::Kind::Call) {
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(receiver, {}, locals, hasMathImport(ctx), const_cast<Context &>(ctx), receiverInfo)) {
      typeName = unwrapImportedCollectionReceiverType(receiverInfo);
    }
    if (typeName.empty()) {
      typeName = unwrapCollectionReceiverEnvelope(qualifyImportedCollectionTypeText(
          inferExprTypeTextForTemplatedVectorFallback(receiver, locals, receiver.namespacePrefix, ctx, hasMathImport(ctx))));
    }
    if (!receiver.isBinding) {
      std::string resolved;
      if (receiver.isMethodCall) {
        if (!resolveMethodCallTemplateTarget(receiver, locals, ctx, resolved)) {
          resolved.clear();
        }
      } else {
        resolved = resolveCalleePath(receiver, receiver.namespacePrefix, ctx);
      }
      auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        if (isStructDefinition(defIt->second)) {
          pathOut = selectHelperOverloadPath(expr, resolved + "/" + methodName, ctx);
          return true;
        }
        for (const auto &transform : defIt->second.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          const std::string &returnType = transform.templateArgs.front();
          if (returnType == "auto") {
            continue;
          }
          typeName = unwrapCollectionReceiverEnvelope(
              qualifyImportedCollectionTypeText(returnType));
          break;
        }
        if (typeName.empty()) {
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBindingForTemplatedFallback(
                  defIt->second, hasMathImport(ctx), const_cast<Context &>(ctx), inferredReturn)) {
            typeName = unwrapImportedCollectionReceiverType(inferredReturn);
          }
        }
      } else {
        std::string collection;
        if (getBuiltinCollectionName(receiver, collection)) {
          typeName = collection;
        }
      }
    }
  }
  if (typeName.empty()) {
    return false;
  }
  typeName = normalizeCollectionReceiverTypeName(typeName);
  if (isExplicitRemovedCollectionMethodAlias(typeName, rawMethodName)) {
    return false;
  }
  const std::string normalizedMethodName = normalizeCollectionMethodName(typeName, methodName);
  if (isPrimitiveBindingTypeName(typeName)) {
    pathOut = selectHelperOverloadPath(expr, "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName, ctx);
    return true;
  }
  std::string normalizedTypeName = typeName;
  if (!normalizedTypeName.empty() && normalizedTypeName.front() == '/') {
    normalizedTypeName.erase(normalizedTypeName.begin());
  }
  std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
  const bool isCollectionFamilyReceiver =
      typeName == "array" || typeName == "vector" || typeName == "map" || typeName == "soa_vector";
  if (ctx.sourceDefs.count(resolvedType) == 0 && !isCollectionFamilyReceiver) {
    auto aliasIt = ctx.importAliases.find(normalizedTypeName);
    if (aliasIt != ctx.importAliases.end()) {
      resolvedType = aliasIt->second;
    }
  }
  if (ctx.sourceDefs.count(resolvedType) == 0) {
    if (isCollectionFamilyReceiver) {
      pathOut = "/" + typeName + "/" + normalizedMethodName;
      pathOut = preferVectorStdlibHelperPath(pathOut, ctx.sourceDefs);
      pathOut = selectHelperOverloadPath(expr, pathOut, ctx);
      return true;
    }
    if (typeName == "string") {
      pathOut = selectHelperOverloadPath(expr, "/string/" + normalizedMethodName, ctx);
      return true;
    }
    return false;
  }
  pathOut = preferVectorStdlibHelperPath(resolvedType + "/" + normalizedMethodName, ctx.sourceDefs);
  pathOut = selectHelperOverloadPath(expr, pathOut, ctx);
  return true;
}

std::string resolveNameToPath(const std::string &name,
                              const std::string &namespacePrefix,
                              const std::unordered_map<std::string, std::string> &importAliases,
                              const std::unordered_map<std::string, Definition> &defs) {
  if (name.empty()) {
    return "";
  }
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (name.find('/') != std::string::npos) {
    return "/" + name;
  }
  if (!namespacePrefix.empty()) {
    std::string scoped = namespacePrefix + "/" + name;
    if (defs.count(scoped) > 0) {
      return scoped;
    }
    auto aliasIt = importAliases.find(name);
    if (aliasIt != importAliases.end()) {
      return aliasIt->second;
    }
    return scoped;
  }
  std::string root = "/" + name;
  if (defs.count(root) > 0) {
    return root;
  }
  auto aliasIt = importAliases.find(name);
  if (aliasIt != importAliases.end()) {
    return aliasIt->second;
  }
  return root;
}
