#pragma once

#include <string>

namespace primec {

std::string experimentalVectorConstructorInferencePath(const std::string &resolvedPath);

bool isCollectionVectorConstructorHelperPath(const std::string &resolvedPath);

bool resolvesCollectionVectorValueTypeText(const std::string &typeText);

bool extractCollectionVectorValueTypeFromTypeText(const std::string &typeText, std::string &valueTypeOut);

}  // namespace primec
