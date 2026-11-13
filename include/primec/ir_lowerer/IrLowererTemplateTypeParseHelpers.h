#pragma once

#include "primec/support/TypeText.h"

#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

using primec::splitTemplateTypeName;
using primec::trimTemplateTypeText;
bool splitTemplateArgs(const std::string &text, std::vector<std::string> &out);
std::string mangleTemplateTypeArgsSuffix(const std::vector<std::string> &args);
bool isResultTemplateTypeBaseName(const std::string &base);
bool parseResultTypeName(const std::string &typeName,
                         bool &hasValue,
                         LocalInfo::ValueKind &valueKind,
                         std::string &errorType);

} // namespace primec::ir_lowerer
