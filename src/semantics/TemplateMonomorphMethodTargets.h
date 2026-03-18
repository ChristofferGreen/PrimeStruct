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
  const Expr &receiver = expr.args.front();
  std::string typeName;
  if (receiver.kind == Expr::Kind::Name) {
    auto it = locals.find(receiver.name);
    if (it != locals.end()) {
      typeName = unwrapCollectionReceiverEnvelope(it->second.typeName, it->second.typeTemplateArg);
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
      typeName = unwrapCollectionReceiverEnvelope(receiverInfo.typeName, receiverInfo.typeTemplateArg);
    }
    if (typeName.empty()) {
      typeName = unwrapCollectionReceiverEnvelope(
          inferExprTypeTextForTemplatedVectorFallback(receiver, locals, receiver.namespacePrefix, ctx, hasMathImport(ctx)));
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
          pathOut = resolved + "/" + methodName;
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
          typeName = unwrapCollectionReceiverEnvelope(returnType);
          break;
        }
        if (typeName.empty()) {
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBindingForTemplatedFallback(
                  defIt->second, hasMathImport(ctx), const_cast<Context &>(ctx), inferredReturn)) {
            typeName = unwrapCollectionReceiverEnvelope(inferredReturn.typeName, inferredReturn.typeTemplateArg);
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
    pathOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
    return true;
  }
  std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
  if (ctx.sourceDefs.count(resolvedType) == 0) {
    auto aliasIt = ctx.importAliases.find(typeName);
    if (aliasIt != ctx.importAliases.end()) {
      resolvedType = aliasIt->second;
    }
  }
  if (ctx.sourceDefs.count(resolvedType) == 0) {
    if (typeName == "array" || typeName == "vector" || typeName == "map" || typeName == "soa_vector") {
      pathOut = "/" + typeName + "/" + normalizedMethodName;
      pathOut = preferVectorStdlibHelperPath(pathOut, ctx.sourceDefs);
      return true;
    }
    if (typeName == "string") {
      pathOut = "/string/" + normalizedMethodName;
      return true;
    }
    return false;
  }
  pathOut = resolvedType + "/" + normalizedMethodName;
  pathOut = preferVectorStdlibHelperPath(pathOut, ctx.sourceDefs);
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
