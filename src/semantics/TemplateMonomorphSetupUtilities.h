#pragma once

uint64_t fnv1a64(const std::string &text) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : text) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string mangleTemplateArgs(const std::vector<std::string> &args) {
  const std::string canonical = stripWhitespace(joinTemplateArgs(args));
  const uint64_t hash = fnv1a64(canonical);
  std::ostringstream out;
  out << "__t" << std::hex << hash;
  return out.str();
}

bool isPathPrefix(const std::string &prefix, const std::string &path) {
  if (prefix.empty()) {
    return false;
  }
  if (path == prefix) {
    return true;
  }
  if (path.size() <= prefix.size()) {
    return false;
  }
  if (path.rfind(prefix, 0) != 0) {
    return false;
  }
  return path[prefix.size()] == '/';
}

bool isGeneratedTemplateSpecializationPath(std::string_view path) {
  const size_t marker = path.rfind("__t");
  if (marker == std::string::npos || marker + 3 >= path.size()) {
    return false;
  }
  for (size_t i = marker + 3; i < path.size(); ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(path[i]))) {
      return false;
    }
  }
  return true;
}

bool isEnclosingTemplateParamName(const std::string &name,
                                  const std::string &namespacePrefix,
                                  const Context &ctx) {
  std::string prefix = namespacePrefix;
  while (!prefix.empty()) {
    auto matchesDefinitionParams = [&](const std::string &path) {
      auto it = ctx.sourceDefs.find(path);
      if (it == ctx.sourceDefs.end()) {
        return false;
      }
      return std::find(it->second.templateArgs.begin(), it->second.templateArgs.end(), name) !=
             it->second.templateArgs.end();
    };
    if (matchesDefinitionParams(prefix)) {
      return true;
    }
    if (isGeneratedTemplateSpecializationPath(prefix)) {
      const size_t marker = prefix.rfind("__t");
      if (marker != std::string::npos && matchesDefinitionParams(prefix.substr(0, marker))) {
        return true;
      }
    }
    const size_t slash = prefix.find_last_of('/');
    if (slash == std::string::npos) {
      break;
    }
    prefix.erase(slash);
  }
  return false;
}

std::string replacePathPrefix(const std::string &path, const std::string &prefix, const std::string &replacement) {
  if (!isPathPrefix(prefix, path)) {
    return path;
  }
  if (path == prefix) {
    return replacement;
  }
  return replacement + path.substr(prefix.size());
}

bool hasMathImport(const Context &ctx) {
  auto includesMathImport = [](const std::vector<std::string> &importPaths) {
    for (const auto &importPath : importPaths) {
      if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
        return true;
      }
    }
    return false;
  };
  if (includesMathImport(ctx.program.sourceImports)) {
    return true;
  }
  if (includesMathImport(ctx.program.imports)) {
    return true;
  }
  return false;
}

bool extractExplicitBindingType(const Expr &expr, BindingInfo &infoOut) {
  if (!expr.isBinding) {
    return false;
  }
  if (!hasExplicitBindingTypeTransform(expr)) {
    return false;
  }
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" || transform.name == "return") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    infoOut.typeName = transform.name;
    infoOut.typeTemplateArg.clear();
    if (!transform.templateArgs.empty()) {
      infoOut.typeTemplateArg = joinTemplateArgs(transform.templateArgs);
    }
    return true;
  }
  return false;
}

std::string bindingTypeToString(const BindingInfo &info) {
  if (info.typeTemplateArg.empty()) {
    return info.typeName;
  }
  return info.typeName + "<" + info.typeTemplateArg + ">";
}

std::string generateTemplateParamName(const Definition &def, size_t index) {
  std::string candidate = "T" + std::to_string(index);
  auto isUsed = [&](const std::string &name) -> bool {
    for (const auto &existing : def.templateArgs) {
      if (existing == name) {
        return true;
      }
    }
    return false;
  };
  while (isUsed(candidate)) {
    ++index;
    candidate = "T" + std::to_string(index);
  }
  return candidate;
}

bool replaceBindingTypeTransform(Expr &binding, const std::string &typeName, std::string &error) {
  bool replaced = false;
  for (auto &transform : binding.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" || transform.name == "return") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      error = "binding transforms do not take arguments";
      return false;
    }
    if (!transform.templateArgs.empty()) {
      error = "binding transforms do not take template arguments";
      return false;
    }
    if (replaced) {
      error = "binding requires exactly one type";
      return false;
    }
    transform.name = typeName;
    replaced = true;
  }
  if (!replaced) {
    Transform transform;
    transform.name = typeName;
    binding.transforms.push_back(std::move(transform));
    replaced = true;
  }
  return replaced;
}

bool isTemplatedAutoCompatDefinitionPath(std::string_view fullPath) {
  return fullPath == "/std/collections/vectorPush" || fullPath == "/std/collections/vectorPop" ||
         fullPath == "/std/collections/vectorReserve" || fullPath == "/std/collections/vectorClear" ||
         fullPath == "/std/collections/vectorRemoveAt" || fullPath == "/std/collections/vectorRemoveSwap" ||
         fullPath == "/std/collections/vectorAt" || fullPath == "/std/collections/vectorAtUnsafe";
}

bool applyImplicitAutoTemplates(Program &program, Context &ctx, std::string &error) {
  for (auto &def : program.definitions) {
    std::vector<std::string> implicitParams;
    if (!def.templateArgs.empty()) {
      const bool allowTemplatedAuto = isTemplatedAutoCompatDefinitionPath(def.fullPath);
      for (auto &param : def.parameters) {
        if (!param.isBinding) {
          continue;
        }
        BindingInfo info;
        if (!hasExplicitBindingTypeTransform(param) ||
            !extractExplicitBindingType(param, info)) {
          if (allowTemplatedAuto) {
            continue;
          }
          error = "implicit auto parameters are only supported on non-templated definitions: " + def.fullPath;
          return false;
        }
        if (info.typeName == "auto") {
          if (!allowTemplatedAuto) {
            error = "implicit auto parameters are only supported on non-templated definitions: " + def.fullPath;
            return false;
          }
          if (!info.typeTemplateArg.empty()) {
            error = "auto parameters do not accept template arguments: " + def.fullPath;
            return false;
          }
        }
      }
      continue;
    }
    size_t autoIndex = 0;
    for (auto &param : def.parameters) {
      if (!param.isBinding) {
        continue;
      }
      BindingInfo info;
      bool hasExplicit = extractExplicitBindingType(param, info);
      if (!hasExplicit && hasExplicitBindingTypeTransform(param)) {
        continue;
      }
      if (hasExplicit && info.typeName != "auto") {
        continue;
      }
      if (hasExplicit && !info.typeTemplateArg.empty()) {
        error = "auto parameters do not accept template arguments: " + def.fullPath;
        return false;
      }
      std::string templateParam = generateTemplateParamName(def, autoIndex++);
      if (!replaceBindingTypeTransform(param, templateParam, error)) {
        if (error.empty()) {
          error = "auto parameter requires an explicit type transform: " + def.fullPath;
        }
        return false;
      }
      def.templateArgs.push_back(templateParam);
      implicitParams.push_back(templateParam);
    }
    if (!implicitParams.empty()) {
      ctx.implicitTemplateDefs.insert(def.fullPath);
      ctx.implicitTemplateParams.emplace(def.fullPath, std::move(implicitParams));
    }
  }
  return true;
}
