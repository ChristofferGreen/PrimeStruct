#pragma once

#include "EmitterHelpers.h"

#include <functional>

namespace primec::emitter {

bool isExplicitArrayCountName(const Expr &expr);
size_t getAccessCallReceiverIndex(const Expr &call,
                                  const std::unordered_map<std::string, BindingInfo> &localTypes);
bool inferCollectionElementTypeNameFromBinding(const BindingInfo &binding, std::string &typeOut);
bool inferCollectionElementTypeNameFromExpr(const Expr &expr,
                                            const std::unordered_map<std::string, BindingInfo> &localTypes,
                                            const std::function<bool(const Expr &, std::string &)> &resolveCallElementTypeName,
                                            std::string &typeOut);
std::string inferAccessCallTypeName(const Expr &call,
                                    const std::unordered_map<std::string, BindingInfo> &localTypes,
                                    const std::function<std::string(const Expr &)> &inferPrimitiveTypeName,
                                    const std::function<bool(const Expr &, std::string &)> &resolveCallElementTypeName);

} // namespace primec::emitter
