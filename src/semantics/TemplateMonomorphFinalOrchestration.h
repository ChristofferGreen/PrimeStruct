#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"
#include "SemanticsHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"

namespace primec {

Context makeTemplateMonomorphContext(Program &program);

void buildImportAliases(Context &ctx);

bool isPathUnderTemplateRoot(const std::string &path, const std::unordered_set<std::string> &templateRoots);

bool rewriteMonomorphizedDefinitions(Context &ctx,
                                     const std::unordered_set<std::string> &templateRoots,
                                     std::string &error);

bool rewriteMonomorphizedExecutions(Context &ctx, std::string &error);

} // namespace primec
