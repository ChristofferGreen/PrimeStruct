#pragma once

#include <filesystem>
#include <string_view>

namespace primec {

std::filesystem::path primecTempRoot();
std::filesystem::path primecCacheDir(std::string_view category, std::string_view key);
std::filesystem::path primecUniqueTempFile(std::string_view prefix, std::string_view extension);

} // namespace primec
