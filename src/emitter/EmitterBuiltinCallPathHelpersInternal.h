#pragma once

#include "EmitterHelpers.h"

namespace primec::emitter {

std::string preferredFileMethodTargetLocal(
    const std::string &methodName,
    const std::unordered_map<std::string, const Definition *> &defMap);
bool isMapCollectionTypeNameLocal(const std::string &name);
bool extractMapKeyValueTypesFromTypeTextLocal(const std::string &typeText,
                                              std::string &keyTypeOut,
                                              std::string &valueTypeOut);
bool extractMapKeyValueTypesLocal(const BindingInfo &binding,
                                  std::string &keyTypeOut,
                                  std::string &valueTypeOut);
bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix);
bool getBuiltinArrayAccessNameLocal(const Expr &expr, std::string &out);

} // namespace primec::emitter
