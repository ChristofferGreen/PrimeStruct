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
  const Expr &receiverExpr = expr.args.front();
  if (!methodName.empty() && methodName.front() == '/') {
    methodName.erase(methodName.begin());
  }
  auto normalizeCollectionMethodName = [](const std::string &receiverTypeName,
                                          std::string candidate) -> std::string {
    if (receiverTypeName == "array" || receiverTypeName == "vector" || receiverTypeName == "soa_vector") {
      const std::string vectorPrefix = "vector/";
      const std::string arrayPrefix = "array/";
      const std::string soaVectorPrefix = "soa_vector/";
      const std::string stdVectorPrefix = "std/collections/vector/";
      const std::string stdSoaVectorPrefix = "std/collections/soa_vector/";
      if (candidate.rfind(vectorPrefix, 0) == 0) {
        return candidate.substr(vectorPrefix.size());
      }
      if (candidate.rfind(arrayPrefix, 0) == 0) {
        return candidate.substr(arrayPrefix.size());
      }
      if (candidate.rfind(soaVectorPrefix, 0) == 0) {
        return candidate.substr(soaVectorPrefix.size());
      }
      if (candidate.rfind(stdVectorPrefix, 0) == 0) {
        return candidate.substr(stdVectorPrefix.size());
      }
      if (candidate.rfind(stdSoaVectorPrefix, 0) == 0) {
        return candidate.substr(stdSoaVectorPrefix.size());
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
  auto isFileMethodName = [](std::string_view methodName) {
    return methodName == "write" || methodName == "write_line" || methodName == "write_byte" ||
           methodName == "read_byte" || methodName == "write_bytes" || methodName == "flush" ||
           methodName == "close";
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
      if (const std::string *importAlias =
              lookupScopedImportAliasForNamespace(base, receiverExpr.namespacePrefix, ctx);
          importAlias != nullptr) {
        return *importAlias + "<" + argText + ">";
      }
      return typeText;
    }
    if (const std::string *importAlias =
            lookupScopedImportAliasForNamespace(typeText, receiverExpr.namespacePrefix, ctx);
        importAlias != nullptr) {
      return *importAlias;
    }
    return typeText;
  };
  auto bindingTypeText = [](const BindingInfo &binding) {
    std::string typeText = binding.typeName;
    if (!binding.typeTemplateArg.empty()) {
      typeText += "<" + binding.typeTemplateArg + ">";
    }
    return typeText;
  };
  auto isBorrowedSoaReceiverType = [&](std::string typeText) {
    typeText = normalizeBindingTypeName(qualifyImportedCollectionTypeText(typeText));
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText) || argText.empty()) {
      return false;
    }
    const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
    if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
      return false;
    }
    return normalizeCollectionReceiverTypeName(
               unwrapCollectionReceiverEnvelope(argText)) == "soa_vector";
  };
  auto unwrapImportedCollectionReceiverType = [&](const BindingInfo &binding) {
    return unwrapCollectionReceiverEnvelope(
        qualifyImportedCollectionTypeText(bindingTypeText(binding)));
  };
  auto selectStaticHelperOverloadPath = [&](const std::string &resolvedPath) -> std::string {
    auto familyIt = ctx.helperOverloads.find(resolvedPath);
    if (familyIt == ctx.helperOverloads.end()) {
      return resolvedPath;
    }
    const size_t argumentCount = expr.args.empty() ? 0 : expr.args.size() - 1;
    for (const auto &entry : familyIt->second) {
      if (entry.parameterCount == argumentCount) {
        return entry.internalPath;
      }
    }
    return resolvedPath;
  };
  auto hasDefinitionFamilyPath = [&](std::string_view path) {
    const std::string pathString(path);
    if (ctx.sourceDefs.count(pathString) > 0 || ctx.helperOverloads.count(pathString) > 0) {
      return true;
    }
    const std::string templatedPrefix = pathString + "<";
    const std::string specializedPrefix = pathString + "__t";
    const std::string overloadPrefix = pathString + "__ov";
    for (const auto &[defPath, _] : ctx.sourceDefs) {
      if (defPath.rfind(templatedPrefix, 0) == 0 ||
          defPath.rfind(specializedPrefix, 0) == 0 ||
          defPath.rfind(overloadPrefix, 0) == 0) {
        return true;
      }
    }
    return false;
  };
  auto soaCanonicalMethodPath = [](const std::string &helperNameString) {
    return "/std/collections/soa_vector/" + helperNameString;
  };
  auto preferredSamePathSoaMethodTarget =
      [&](std::string_view helperName, std::string_view samePathPrefix) {
    const std::string helperNameString(helperName);
    const std::string samePathPrefixString(samePathPrefix);
    const std::string samePath = samePathPrefixString + helperNameString;
    if (hasDefinitionFamilyPath(samePath)) {
      return samePath;
    }
    return soaCanonicalMethodPath(helperNameString);
  };
  auto preferredSamePathSoaToAosMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(helperName, "/");
  };
  auto preferredSamePathSoaCountMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(helperName, "/soa_vector/");
  };
  auto preferredSamePathSoaPushReserveMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(helperName, "/soa_vector/");
  };
  auto preferredSamePathSoaGetMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(helperName, "/soa_vector/");
  };
  auto preferredSamePathSoaRefMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(helperName, "/soa_vector/");
  };
  auto borrowedSoaWrapperMethodName = [](std::string_view helperName) {
    if (helperName == "count") {
      return std::string("count_ref");
    }
    if (helperName == "get") {
      return std::string("get_ref");
    }
    if (helperName == "ref") {
      return std::string("ref_ref");
    }
    if (helperName == "to_aos") {
      return std::string("to_aos_ref");
    }
    return std::string(helperName);
  };
  const Expr &receiver = expr.args.front();
  bool isBorrowedSoaReceiver = false;
  if (receiver.kind == Expr::Kind::Name && normalizeBindingTypeName(receiver.name) == "FileError") {
    if (methodName == "result") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/result");
      return true;
    }
    if (methodName == "status") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/status");
      return true;
    }
    if (methodName == "why") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/why");
      return true;
    }
    if (methodName == "is_eof") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/is_eof");
      return true;
    }
    if (methodName == "eof") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/eof");
      return true;
    }
  }
  std::string typeName;
  if (receiver.kind == Expr::Kind::Name) {
    auto it = locals.find(receiver.name);
    if (it != locals.end()) {
      isBorrowedSoaReceiver = isBorrowedSoaReceiverType(bindingTypeText(it->second));
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
      isBorrowedSoaReceiver = isBorrowedSoaReceiverType(bindingTypeText(receiverInfo));
      typeName = unwrapImportedCollectionReceiverType(receiverInfo);
    }
    if (typeName.empty()) {
      const std::string inferredTypeText = qualifyImportedCollectionTypeText(
          inferExprTypeTextForTemplatedVectorFallback(
              receiver, locals, receiver.namespacePrefix, ctx, hasMathImport(ctx)));
      isBorrowedSoaReceiver = isBorrowedSoaReceiverType(inferredTypeText);
      typeName = unwrapCollectionReceiverEnvelope(inferredTypeText);
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
          isBorrowedSoaReceiver = isBorrowedSoaReceiverType(returnType);
          typeName = unwrapCollectionReceiverEnvelope(
              qualifyImportedCollectionTypeText(returnType));
          break;
        }
        if (typeName.empty()) {
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBindingForTemplatedFallback(
                  defIt->second, hasMathImport(ctx), const_cast<Context &>(ctx), inferredReturn)) {
            isBorrowedSoaReceiver =
                isBorrowedSoaReceiverType(bindingTypeText(inferredReturn));
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
  auto preferredFileMethodTarget = [&](std::string_view helperName) {
    const std::string builtinPath = "/file/" + std::string(helperName);
    if (helperName != "write" && helperName != "write_line" && helperName != "close") {
      return builtinPath;
    }
    if ((helperName == "write" || helperName == "write_line") &&
        expr.args.size() > 10) {
      return builtinPath;
    }
    if (receiver.kind == Expr::Kind::Name && receiver.name == "self") {
      return builtinPath;
    }
    const std::string stdlibPath = "/File/" + std::string(helperName);
    if (hasDefinitionFamilyPath(stdlibPath)) {
      return stdlibPath;
    }
    return builtinPath;
  };
  const std::string normalizedMethodName = normalizeCollectionMethodName(typeName, methodName);
  std::string normalizedTypeName = typeName;
  if (!normalizedTypeName.empty() && normalizedTypeName.front() == '/') {
    normalizedTypeName.erase(normalizedTypeName.begin());
  }
  const auto normalizedReceiverLeafName = [&]() {
    const size_t slash = normalizedTypeName.find_last_of('/');
    return slash == std::string::npos ? normalizedTypeName
                                      : normalizedTypeName.substr(slash + 1);
  }();
  if ((typeName == "File" || normalizedReceiverLeafName == "File") &&
      isFileMethodName(normalizedMethodName)) {
    pathOut = preferredFileMethodTarget(normalizedMethodName);
    return true;
  }
  if (isExplicitRemovedCollectionMethodAlias(typeName, rawMethodName)) {
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    pathOut = selectHelperOverloadPath(expr, "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName, ctx);
    return true;
  }
  if (normalizedReceiverLeafName == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "status" || normalizedMethodName == "result")) {
    pathOut = selectStaticHelperOverloadPath("/std/file/FileError/" + normalizedMethodName);
    return true;
  }
  if (normalizedReceiverLeafName == "ImageError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    pathOut = selectStaticHelperOverloadPath("/std/image/ImageError/" + normalizedMethodName);
    return true;
  }
  if (normalizedReceiverLeafName == "ContainerError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    pathOut = selectStaticHelperOverloadPath("/std/collections/ContainerError/" + normalizedMethodName);
    return true;
  }
  if (normalizedReceiverLeafName == "GfxError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    pathOut = selectStaticHelperOverloadPath("/std/gfx/GfxError/" + normalizedMethodName);
    return true;
  }
  if (normalizedTypeName == "soa_vector" &&
      (normalizedMethodName == "count" || normalizedMethodName == "count_ref")) {
    const std::string helperName =
        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)
                              : normalizedMethodName;
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaCountMethodTarget(helperName), ctx);
    return true;
  }
  if (normalizedTypeName == "soa_vector" &&
      (normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref")) {
    const std::string helperName =
        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)
                              : normalizedMethodName;
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaToAosMethodTarget(helperName), ctx);
    return true;
  }
  if (normalizedTypeName == "soa_vector" &&
      (normalizedMethodName == "get" || normalizedMethodName == "get_ref")) {
    const std::string helperName =
        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)
                              : normalizedMethodName;
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaGetMethodTarget(helperName), ctx);
    return true;
  }
  if (normalizedTypeName == "soa_vector" &&
      (normalizedMethodName == "push" || normalizedMethodName == "reserve")) {
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaPushReserveMethodTarget(normalizedMethodName), ctx);
    return true;
  }
  if (normalizedTypeName == "soa_vector" &&
      (normalizedMethodName == "ref" || normalizedMethodName == "ref_ref")) {
    const std::string helperName =
        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)
                              : normalizedMethodName;
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaRefMethodTarget(helperName), ctx);
    return true;
  }
  std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
  const bool isCollectionFamilyReceiver =
      typeName == "array" || typeName == "vector" || typeName == "map" || typeName == "soa_vector";
  if (ctx.sourceDefs.count(resolvedType) == 0 && !isCollectionFamilyReceiver) {
    if (const std::string *importAlias =
            lookupScopedImportAliasForNamespace(normalizedTypeName, receiver.namespacePrefix, ctx);
        importAlias != nullptr) {
      resolvedType = *importAlias;
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
  const bool isConcreteExperimentalSoaReceiver =
      normalizedTypeName == "soa_vector" &&
      resolvedType.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0;
  if (isConcreteExperimentalSoaReceiver &&
      (normalizedMethodName == "count" || normalizedMethodName == "count_ref")) {
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaCountMethodTarget(normalizedMethodName), ctx);
    return true;
  }
  if (isConcreteExperimentalSoaReceiver &&
      (normalizedMethodName == "get" || normalizedMethodName == "get_ref")) {
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaGetMethodTarget(normalizedMethodName), ctx);
    return true;
  }
  if (isConcreteExperimentalSoaReceiver &&
      (normalizedMethodName == "push" || normalizedMethodName == "reserve")) {
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaPushReserveMethodTarget(normalizedMethodName), ctx);
    return true;
  }
  if (isConcreteExperimentalSoaReceiver &&
      (normalizedMethodName == "ref" || normalizedMethodName == "ref_ref")) {
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaRefMethodTarget(normalizedMethodName), ctx);
    return true;
  }
  if (isConcreteExperimentalSoaReceiver &&
      (normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref")) {
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaToAosMethodTarget(normalizedMethodName), ctx);
    return true;
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
