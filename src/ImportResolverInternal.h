#pragma once

#include "primec/ProcessRunner.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace primec::import_resolver_detail {

bool validateSlashPath(const std::string &text, std::string &error);

bool readFile(const std::string &path, std::string &out);

std::string normalizePathKey(const std::filesystem::path &path);

std::string trim(const std::string &value);

size_t skipQuotedLiteral(const std::string &text, size_t start);

size_t skipLineComment(const std::string &text, size_t start);

size_t skipBlockComment(const std::string &text, size_t start);

size_t skipWhitespaceAndComments(const std::string &text, size_t start);

size_t findIncludePayloadEnd(const std::string &source, size_t start);

bool scanIncludeDirective(const std::string &source, size_t pos, size_t &payloadStart, size_t &payloadEnd);

bool scanLegacyIncludeDirective(const std::string &source, size_t pos, size_t &payloadStart, size_t &payloadEnd);

bool readQuotedString(const std::string &payload, size_t &pos, std::string &out, std::string &error);

bool readBareIncludePath(const std::string &payload, size_t &pos, std::string &out);

bool tryConsumeIncludeKeyword(const std::string &payload, size_t &pos, const std::string &keyword);

bool extractArchive(const std::filesystem::path &archive,
                    const ProcessRunner &processRunner,
                    std::filesystem::path &outDir,
                    std::string &error);

bool appendArchiveRoots(const std::vector<std::filesystem::path> &roots,
                        const ProcessRunner &processRunner,
                        std::vector<std::filesystem::path> &expanded,
                        std::string &error);

bool parseVersionParts(const std::string &text, std::vector<int> &parts, std::string &error);

bool hasPrefix(const std::vector<int> &candidate, const std::vector<int> &prefix);

bool isNewerVersion(const std::vector<int> &lhs, const std::vector<int> &rhs);

bool isPrivatePath(const std::filesystem::path &path);

bool collectPrimeFiles(const std::filesystem::path &root,
                       std::vector<std::filesystem::path> &out,
                       std::string &error);

bool selectVersionDirectory(const std::filesystem::path &baseDir,
                            const std::vector<int> &requested,
                            std::string &selected,
                            std::string &error);

} // namespace primec::import_resolver_detail
